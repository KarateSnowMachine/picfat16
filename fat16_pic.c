#include "light.h"
#include "spi.h"
#include "fat16.h"

//remove the assertions, but leave them as reminders of the assumptions involved
#define assert(x) ;

unsigned previous_dir_entry_offset;
WORD *fat_cluster_entries; 
dir_entry_t *dirs;


/* these will get set by init_fat16 since they are the only important variables from the boot record */
unsigned boot_offset;
unsigned reserved_sectors;
unsigned max_root_entries;
unsigned sectors_per_fat;
unsigned directory_table_offset; 
unsigned fat_offset; 
unsigned cluster_offset;

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

void write_sector(int sector, BYTE *buf)
{
	SD_addr addr = sector;
	SD_write_sector(addr, buf); 
}

void read_sector(int sector, BYTE *buf) {
	SD_addr addr = sector;
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
	for (sector=0; sector < 32; ++sector) {
		read_sector(directory_table_offset + sector, (BYTE *)dirs);
		for (i=0; i<max_root_entries/32; i++) {
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
unsigned find_free_cluster()
{
	int current_cluster=0;
	int sector_offset;
	for (sector_offset=0; sector_offset<sectors_per_fat; sector_offset++) {
		//TODO: clean this up
		read_sector(fat_offset+sector_offset,(BYTE*)fat_cluster_entries);
		for (current_cluster=0; current_cluster < 256; ++current_cluster) {
			if (!fat_cluster_entries[current_cluster]) {
				return current_cluster;
			}
		}
	}
	return 0;
}

void create_file(char *name, char *ext)
{
	unsigned free_cluster = find_free_cluster();
	free_dir = find_free_directory_entry(&previous_dir_sector_offset, &previous_dir_entry_offset);
	free_dir->filename[0]='G';
	free_dir->filename[1]='P';
	free_dir->filename[2]='S';
	free_dir->filename[3]=0;
	free_dir->extension[0]='T';
	free_dir->extension[1]='X';
	free_dir->extension[2]='T';
/*
	snprintf(free_dir->filename, 9, "%s%d", name,previous_dir_entry_offset);
	snprintf(free_dir->extension, 4, "%s", ext);
*/
	free_dir->attributes=0x20; //ARCHIVE
	free_dir->cluster_start=free_cluster;
	free_dir->size = 0;
	fat_cluster_entries[free_cluster] = 0xffff;
	write_sector(directory_table_offset + previous_dir_sector_offset, (BYTE *) dirs);
	write_sector(SECTOR_FOR_CLUSTER(free_cluster), (BYTE *) fat_cluster_entries);
	previous_cluster = free_cluster;
	num_sectors_used_in_cluster=0;
}

void write_buf(BYTE *buf)
{
	unsigned int previous_sector_offset, sector_offset;
	// if we have written more than a cluster, allocate a new cluster and link it into the FAT
	if (num_sectors_used_in_cluster == 32)
	{
		unsigned free_cluster = find_free_cluster();
		fat_cluster_entries[free_cluster] = 0xffff;
		// this is the easy case where since the cluster is the same for both FAT entries, we can modify them in one shot
		// and write back the whole sector 
		previous_sector_offset = SECTOR_FOR_CLUSTER(previous_cluster);
		sector_offset = SECTOR_FOR_CLUSTER(free_cluster);
		if (sector_offset == previous_sector_offset) {
			fat_cluster_entries[previous_cluster] = free_cluster; 
			write_sector(sector_offset, (BYTE*) fat_cluster_entries);
		} else {
			write_sector(sector_offset, (BYTE*) fat_cluster_entries);
			read_sector(previous_sector_offset, (BYTE*) fat_cluster_entries); 
			fat_cluster_entries[previous_cluster] = free_cluster;
			write_sector(previous_sector_offset, (BYTE*) fat_cluster_entries);
		}
		previous_cluster = free_cluster;
		num_sectors_used_in_cluster=0;
	}

	// update data
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
	SD_addr addr;

	/* init pointers */
	buf = mbr_boot_directory_buf2;
	addr.full_addr = 0;
/*
	fat_cluster_entries = (WORD *)fat_cluster_entries2;
	dirs = (dir_entry_t *)mbr_boot_directory_buf2;


*/

	// read the MBR to figure out the "num_sector_skip" value which marks the beginning of partition 0
	SD_read_sector(addr, mbr_boot_directory_buf2);
 	mbr = (MBR_t *) mbr_boot_directory_buf2;

	//after this point, we can throw away the MBR since we don't really need any other fields from it and reuse the buffer to store the boot record
	boot_offset = mbr->partition_table[0].num_sectors_skip;
	
	// read the first sector of the partition which is the boot record
	read_sector(boot_offset, buf); 
	b = (boot_record_t *)mbr_boot_directory_buf2; 
	if (b->signature != 0xaa55) {
		while (1) {
			light_toggle();
		}
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
void SD_read_test()
{
	init_fat16();
//	create_file("A","GPS");
	light_on();
	while(1);

	
}
/*
int main() {
	// sanity check
	assert(sizeof(BYTE) == 1); 
	assert(sizeof(WORD) == 2);
	assert(sizeof(DWORD) == 4); 

	BYTE buf[512];
	init_fat16(buf);
	create_file("S","TXT");
	return 0;
}
*/