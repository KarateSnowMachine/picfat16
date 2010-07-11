#include "light.h"
#include "spi.h"
#include "fat16.h"

//remove the assertions, but leave them as reminders of the assumptions involved
#define assert(x) ;

unsigned previous_dir_entry_offset;
WORD *fat_cluster_entries; 
dir_entry_t *dirs;


/* these will get set by init_fat16 since they are the only important variables from the boot record */
WORD boot_offset;
WORD reserved_sectors;
WORD max_root_entries;
WORD sectors_per_fat;
WORD directory_table_offset; 
WORD fat_offset; 
WORD cluster_offset;

/* these are temporary variable that are per-file */
unsigned previous_cluster;
unsigned num_sectors_used_in_cluster;
dir_entry_t *free_dir;
long previous_dir_sector_offset;



/* fat_cluster entries array: 512 contiguous bytes 
	that will cache the clusters in the FAT for faster writes */

#pragma udata d
BYTE fat_cluster_entries1[255];
#pragma udata e
BYTE fat_cluster_entries2[255];


#pragma udata f
BYTE mbr_boot_directory_buf1[256];
#pragma udata g
BYTE mbr_boot_directory_buf2[256];

/* don't put any other globals below here because the compiler will think
	you want to put them in the udata section which will cause problems */

#define SECTOR_FOR_CLUSTER_DATA(c) (cluster_offset+(c-2)*32)
#define SECTOR_FOR_CLUSTER(c) (fat_offset+(c/256))

char write_sector(DWORD sector, BYTE *buf)
{
	SD_addr addr;
	addr.full_addr = sector<<9;
	return SD_write_sector(addr, buf); 
}

#define write_sector_delay(sector,buf) write_sector((sector), (buf));

void read_sector(DWORD sector, BYTE *buf) {
	SD_addr addr;
	addr.full_addr = sector<<9;
	SD_read_sector(addr, buf);
}

DWORD get_next_cluster( int cluster_num)
{
	WORD buf[1];
	read_sector(SECTOR_FOR_CLUSTER(cluster_num), (BYTE *)buf); 
	//	printf("\tFound cluster %x \n", buf[cluster_num%256]);
	return buf[cluster_num%256];
}

dir_entry_t *find_free_directory_entry(long *sector_offset, unsigned *entry_offset)
{
	int sector, i; 
	// read the directory table one sector at a time 
	//TODO: fix up the hardcoding

// (512b/sector) / (32b/entry) = 16 entries/sector
#define entries_per_sector 16 
// (512 entries * 32b/entry) / (512 bytes/sector) = 32 sectors 
#define sectors_per_dir_table 32

	for (sector=0; sector < sectors_per_dir_table; ++sector) {
		read_sector(directory_table_offset + sector, (BYTE *)dirs);
		
		for (i=0; i<entries_per_sector; i++) {
			unsigned char first_byte = dirs[i].filename[0];
			if (first_byte == 0x00 || first_byte == 0xe5 || first_byte == 0x05)
			{
				*sector_offset=sector;
				*entry_offset=i;
				return &dirs[i];
			}
		}
		return NULL;
	}
}
WORD find_free_cluster()
{
	int current_cluster;
	int sector_offset;
	//scan the file allocation table sector by sector
	for (sector_offset=0; sector_offset<sectors_per_fat; sector_offset++) {

		read_sector(fat_offset+sector_offset,(BYTE*)fat_cluster_entries);
		for (current_cluster=0; current_cluster < 256; ++current_cluster) {
			if (fat_cluster_entries[current_cluster] == 0) 
			{ // found it
				return sector_offset*256+current_cluster;
			}
		}
	}
	return 0;
}

void create_file(WORD create_date, WORD create_time)
{
	unsigned free_cluster = find_free_cluster();
	unsigned dir_entry_num;
	DWORD sector_for_free_cluster;

	free_dir = find_free_directory_entry(&previous_dir_sector_offset, &previous_dir_entry_offset);
	dir_entry_num = previous_dir_sector_offset*16+previous_dir_entry_offset;
	free_dir->filename[0]='G';
	free_dir->filename[1]='P';
	free_dir->filename[2]='S';
	free_dir->filename[3]='_';
	free_dir->filename[4]=(char) (dir_entry_num%26)+'A'; // convert the dir entry number into a lowercase
	free_dir->filename[5]=(char) (dir_entry_num%10)+48; // numbers 0-9
	free_dir->filename[6]=' ';
	free_dir->filename[7]=' ';
	free_dir->extension[0]='T';
	free_dir->extension[1]='X';
	free_dir->extension[2]='T';
	free_dir->reserved = 0x18; // this is what windows does, apparently
	free_dir->create_time_fine = 0;
	free_dir->create_date = create_date;
	free_dir->create_time = create_time;
	free_dir->modified_date = create_date;
	free_dir->modified_time = create_time;
	free_dir->last_access = create_time;
	
/*
	snprintf(free_dir->filename, 9, "%s%d", name,previous_dir_entry_offset);
	snprintf(free_dir->extension, 4, "%s", ext);
 */
	free_dir->attributes=0x20; //ARCHIVE
	free_dir->cluster_start=free_cluster;
	free_dir->size = 0;
	fat_cluster_entries[free_cluster%256] = 0xffff;
	write_sector_delay(directory_table_offset + previous_dir_sector_offset, (BYTE *) dirs);
	sector_for_free_cluster = SECTOR_FOR_CLUSTER(free_cluster);

	write_sector(sector_for_free_cluster, (BYTE *) fat_cluster_entries);

	previous_cluster = free_cluster;
	num_sectors_used_in_cluster=0;
}

void write_buf(BYTE *buf)
{
	unsigned int previous_sector_offset, sector_offset;
	WORD free_cluster;
	// if we have written more than a cluster's worth of sectors, allocate a new cluster and link it into the FAT
	if (num_sectors_used_in_cluster == 32)
	{
		free_cluster = find_free_cluster();
		fat_cluster_entries[free_cluster%256] = 0xffff;
		// this is the easy case where since the cluster is the same for both FAT entries, we can modify them in one shot
		// and write back the whole sector 
		previous_sector_offset = SECTOR_FOR_CLUSTER(previous_cluster);
		sector_offset = SECTOR_FOR_CLUSTER(free_cluster);

		// this is the easy case where since the cluster is the same for both FAT entries, we can modify them in one shot and write back the whole sector 
		if (sector_offset == previous_sector_offset) {
			fat_cluster_entries[previous_cluster%256] = free_cluster; 
			write_sector(sector_offset, (BYTE*) fat_cluster_entries);
		} else {
			/* this is the tricky case where the currently loaded sector is "sector_offset" (new cluster), so we write that back first with the modified free_cluster entry, and then 
				read the previous sector, update the previous_cluster's value, and write back that sector
					*/
			write_sector(sector_offset, (BYTE*) fat_cluster_entries);
			read_sector(previous_sector_offset, (BYTE*) fat_cluster_entries); 
			fat_cluster_entries[previous_cluster%256] = free_cluster;
			write_sector(previous_sector_offset, (BYTE*) fat_cluster_entries);
		}
		light_toggle(); // to keep the blinky pattern the same 
		previous_cluster = free_cluster;
		num_sectors_used_in_cluster=0;
	}

	// update data in the cluster (by adding a new sector to it)
	write_sector(SECTOR_FOR_CLUSTER_DATA(previous_cluster)+num_sectors_used_in_cluster, buf);
	num_sectors_used_in_cluster++;

	// update directory entry
	dirs[previous_dir_entry_offset].size+=512;
	write_sector(directory_table_offset + previous_dir_sector_offset, (BYTE *)dirs);
}

void init_fat16()
{
	MBR_t *mbr;
	boot_record_t *b;
	BYTE *buf;
	int i;

	/* init pointers */
	buf = mbr_boot_directory_buf2;

	fat_cluster_entries = (WORD *)fat_cluster_entries2;
	dirs = (dir_entry_t *)mbr_boot_directory_buf2;

	// read the MBR to figure out the "num_sector_skip" value which marks the beginning of partition 0
	read_sector(0, mbr_boot_directory_buf2);
 	mbr = (MBR_t *) mbr_boot_directory_buf2;

	//after this point, we can throw away the MBR since we don't really need any other fields from it and reuse the buffer to store the boot record
	boot_offset = mbr->partition_table[0].num_sectors_skip;
	
	// read the first sector of the partition which is the boot record
	read_sector(boot_offset, mbr_boot_directory_buf2); 
	b = (boot_record_t *)mbr_boot_directory_buf2; 
	if (b->signature != 0xaa55) {
		ERROR();
	}
 	if (b->sectors_per_cluster != 32)
	{
		ERROR();
	}
	if (b->max_root_entries != 512) {
		ERROR();
	}
	assert(b->max_root_entries == 512); 
	assert(b->sectors_per_cluster == 32);

	// sanity checking
	assert(b->bytes_per_sector == 512); 
	assert(b->signature == 0xaa55);

	// store all the relevant entries from the boot record
	max_root_entries = b->max_root_entries;
	reserved_sectors = b->reserved_sectors;
	sectors_per_fat = b->sectors_per_fat;
	// at this point, the boot record isn't needed any more either

	// compute some offsets of various important sections of this partiton
	fat_offset = boot_offset + reserved_sectors; 
	directory_table_offset = fat_offset + (sectors_per_fat * 2);
	cluster_offset = directory_table_offset + 32; // 32 = 512 max entries x 32b / 512b per sector

	// some temporary values used to keep track of the file we are currently writing to
	previous_cluster=0;
	previous_dir_sector_offset=-1;
	num_sectors_used_in_cluster = 0;
}
