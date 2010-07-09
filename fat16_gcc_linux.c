#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "fat16.h"


FILE *image;
dir_entry_t dirs[512/sizeof(dir_entry_t)];
WORD fat_cluster_entries[256];
unsigned previous_dir_entry_offset;

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

#define SECTOR_FOR_CLUSTER_DATA(c) (cluster_offset+(c-2)*32)
#define SECTOR_FOR_CLUSTER(c) (fat_offset+(c/256))
#define SECTOR2BYTE(x) (x<<9)

void read_sector(int sector_number, BYTE *buf)
{
	fseek(image, SECTOR2BYTE(sector_number), SEEK_SET);
	fread(buf, sizeof(BYTE), 512, image);
}

void write_sector(int sector, BYTE *buf) {
	fseek(image, SECTOR2BYTE(sector), SEEK_SET);
	fwrite(buf, sizeof(BYTE), 512, image);
}

DWORD get_next_cluster( int cluster_num)
{
	WORD buf[256];
	read_sector(SECTOR_FOR_CLUSTER(cluster_num), (BYTE *)buf); 
	printf("Accessing cluster %d ... value=%d (0x%x) \n", cluster_num, buf[cluster_num%256], buf[cluster_num%256]); 

	//	printf("\tFound cluster %x \n", buf[cluster_num%256]);
	return buf[cluster_num%256];
}

dir_entry_t *find_free_directory_entry(long *sector_offset, unsigned *entry_offset)
{
	// read the directory table one sector at a time 
	//TODO: fix up the hardcoding
	for (int sector=0; sector < 32; ++sector) {
		read_sector(directory_table_offset + sector, (BYTE *)dirs);
		for (int i=0; i<max_root_entries/32; i++) {
			unsigned char first_byte = dirs[i].filename[0];
			if (first_byte == 0x00 || first_byte == 0xe5 || first_byte == 0x05)
			{
				printf("Found free dir entry with first_byte=%x at %d attributes=%x\n",first_byte, i, dirs[i].attributes);
				*sector_offset=sector;
				*entry_offset=i;
				return &dirs[i];
			}
		}
		printf("Couldn't find free directory entry");
		return NULL;
	}
}
unsigned find_free_cluster()
{
	int current_cluster=0;
	for (int sector_offset=0; sector_offset<sectors_per_fat; sector_offset++) {
		//TODO: clean this up
		read_sector(fat_offset+sector_offset,(BYTE*)fat_cluster_entries);
		for (current_cluster=0; current_cluster < 256; ++current_cluster) {
			if (!fat_cluster_entries[current_cluster]) {
				printf("Found free cluster %d\n",current_cluster);
				return current_cluster;
			}
		}
	}
	printf("Couldn't find a free cluster\n");
	return 0;
}

void create_file(char *name, char *ext)
{
	unsigned free_cluster = find_free_cluster();
	free_dir = find_free_directory_entry(&previous_dir_sector_offset, &previous_dir_entry_offset);
	snprintf(free_dir->filename, 9, "%s%d", name,previous_dir_entry_offset);
	snprintf(free_dir->extension, 4, "%s", ext);
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
	// if we have written more than a cluster, allocate a new cluster and link it into the FAT
	if (num_sectors_used_in_cluster == 32)
	{
		unsigned free_cluster = find_free_cluster();
		fat_cluster_entries[free_cluster] = 0xffff;
		// this is the easy case where since the cluster is the same for both FAT entries, we can modify them in one shot
		// and write back the whole sector 
		unsigned previous_sector_offset = SECTOR_FOR_CLUSTER(previous_cluster);
		unsigned sector_offset = SECTOR_FOR_CLUSTER(free_cluster);
		if (sector_offset == previous_sector_offset) {
			printf("easy case");
			fat_cluster_entries[previous_cluster] = free_cluster; 
			write_sector(sector_offset, (BYTE*) fat_cluster_entries);
		} else {
			printf("hard case");
			write_sector(sector_offset, (BYTE*) fat_cluster_entries);
			read_sector(previous_sector_offset, (BYTE*) fat_cluster_entries); 
			fat_cluster_entries[previous_cluster] = free_cluster;
			write_sector(previous_sector_offset, (BYTE*) fat_cluster_entries);
		}
		previous_cluster = free_cluster;
		num_sectors_used_in_cluster=0;
	}

	// update data
	printf ("Writing buffer to sector=%d, prev=%d\n", SECTOR_FOR_CLUSTER_DATA(previous_cluster)+num_sectors_used_in_cluster, previous_cluster);
	write_sector(SECTOR_FOR_CLUSTER_DATA(previous_cluster)+num_sectors_used_in_cluster, buf);
	num_sectors_used_in_cluster++;
	// update directory entry
	dirs[previous_dir_entry_offset].size+=512;
	write_sector(directory_table_offset + previous_dir_sector_offset, (BYTE *)dirs);
}


void read_cluster(int start_sector, unsigned long size) {
	char buf[512+1];
	int iterations = size > 16384 ? 32 : size/512;
	if (iterations == 0) iterations = 1;
	for (int i=0; i<iterations; ++i) {// 32 sectors in a 16kb cluster
		read_sector(start_sector+i, buf); 
		if (size > 512)
			buf[512] = 0;
		else 
			buf[size] = 0;
	//fprintf(stderr, "%s", buf);
	}
}


void init_fat16(BYTE *buf)
{
	read_sector(0, buf);
	MBR_t *mbr = (MBR_t *) buf;
	printf("MBR INFO: \n");
	for (int i=0; i<4; i++) 
	{
		part_entry_t *p = &mbr->partition_table[i];
		printf("\tpartition %d (active=%u) start: h=%u c=%u, type=%x, end: h=%u c=%u ; skip=%u, num=%u\n", i, p->active, p->part_begin_head,
				p->part_begin_cylinder, p->type_code, p->part_end_head,
				p->part_end_cylinder, p->num_sectors_skip, p->num_sectors_part);
	}
	assert(sizeof(part_entry_t) == 16); 
	assert(sizeof(MBR_t) == 512); 
	//throw away the MBR since we don't really need any other fields from it and reuse the buffer to store the boot record
	boot_offset = mbr->partition_table[0].num_sectors_skip;

	read_sector(boot_offset, buf); 

	boot_record_t *b = (boot_record_t *)buf; 

	printf("OEM Name=%s FAT name=%s Volume Name=%s; reserved sectors=%u, max root entries=%u, (=%ukb/cluster), media_descriptor=%x, sectors_per_fat=%d\n",
			b->oem_name, b->fat_name, b->volume_name, b->reserved_sectors,
			b->max_root_entries,
			b->bytes_per_sector*b->sectors_per_cluster/1024,
			b->media_descriptor,
			b->sectors_per_fat);

	assert(b->max_root_entries == 512); 
	assert(b->sectors_per_cluster == 32);

	// sanity checking
	assert(b->bytes_per_sector == 512); 
	assert(b->signature == 0xaa55);

	max_root_entries = b->max_root_entries;
	reserved_sectors = b->reserved_sectors;
	sectors_per_fat = b->sectors_per_fat;
	fat_offset = boot_offset + reserved_sectors; 
	directory_table_offset = fat_offset + (sectors_per_fat * 2);
	cluster_offset = directory_table_offset + 32; // 32 = 512 max entries x 32b / 512b per sector
	previous_cluster=0;
	previous_dir_sector_offset=-1;
	num_sectors_used_in_cluster = 0;
}

void read_directory_entries() {
	for (int sector=0; sector < 32; ++sector) {
		read_sector(directory_table_offset + sector, (BYTE *)dirs);
		for (int i=0; i<max_root_entries/32; i++) {
			unsigned char first_byte = dirs[i].filename[0];

			if (first_byte != 0x00 && first_byte != 0xe5 && first_byte != 0x05) {
				if ((dirs[i].attributes ==0x10 )) {
					printf("\tsubdir=%s attribute=%x (size=%u, start=%u)\n",dirs[i].filename, dirs[i].attributes,dirs[i].size, dirs[i].cluster_start);
				}
				if ((dirs[i].attributes ==0x20 )) {
					int size_left = dirs[i].size;
					int next = dirs[i].cluster_start;
					printf("\t[%d], %x file=%s.%s attribute=%x (size=%u, start=%u, offset=%x)\n",i,first_byte,dirs[i].filename, dirs[i].extension,dirs[i].attributes,dirs[i].size, dirs[i].cluster_start,cluster_offset);
					while (next != 0xffff) {
						assert(size_left > 0);
						int cluster_sector = (cluster_offset+next*32-64);
						read_cluster(cluster_sector, size_left);
						// get the cluster for the next part of the file
						next = get_next_cluster(next);
						size_left -= 16384;
					}
					assert(size_left <= 0);
				}
			}
		}
	}
}

int main() {
	/* sanity check */
	assert(sizeof(BYTE) == 1); 
	assert(sizeof(WORD) == 2);
	assert(sizeof(DWORD) == 4); 

	BYTE buf[512];
	image = fopen("fat16.img", "rb+");
	if (!image)
	{
		printf("Error opening file"); 
	}
	init_fat16(buf);
	read_directory_entries();
	return 0;

	/*
	create_file("S","TXT");
	for (int i=0; i< 54; i++)
	{
		fread(buf, sizeof(char), 512, stdin); 
		write_buf(buf);
	}

	
	allocate_cluster(buf); 
	read_directory_entries();
	*/
	return 0;
}

