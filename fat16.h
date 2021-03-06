#ifndef _FAT16_H_
#define _FAT16_H_

#define NULL 0

struct _part_entry_t {
	BYTE active; 
	BYTE part_begin_head;
	WORD part_begin_cylinder;
	BYTE type_code;
	BYTE part_end_head;
	WORD part_end_cylinder;
	DWORD num_sectors_skip;
	DWORD num_sectors_part;
};

typedef struct _part_entry_t part_entry_t;

struct _MBR{
	BYTE boot_code[446];
	part_entry_t partition_table[4]; // 16 bytes each
	BYTE magic_number[2];
};

typedef struct _MBR MBR_t;

typedef struct _boot_record_t {
	BYTE jump_code[3];
	char oem_name[8];
	WORD bytes_per_sector;
	BYTE sectors_per_cluster;
	WORD reserved_sectors; 
	BYTE num_copies;
	WORD max_root_entries; 
	WORD num_sectors_less_32m; 
	BYTE media_descriptor; 
	WORD sectors_per_fat;
	WORD sectors_per_track;
	WORD num_heads;
	DWORD num_hidden_sectors; 
	DWORD num_sectors_partition; 
	WORD logical_drive_num_partitions; 
	BYTE extended_signature;
	DWORD serial_num;
	char volume_name[11];
	char fat_name[8];
	BYTE exe_code[448];
	WORD signature; 
} boot_record_t;

typedef struct _dir_entry_t {
	char filename[8];
	char extension[3];
	BYTE attributes;
	BYTE reserved;
	BYTE create_time_fine; 
	WORD create_time;
	WORD create_date;
	WORD last_access;
	WORD ea_index_unused;
	WORD modified_time;
	WORD modified_date;
	WORD cluster_start;
	DWORD size;
} dir_entry_t;

//functions
void init_fat16(void);
WORD find_free_cluster(void);
void create_file(WORD date, WORD time);
void write_buf_to_file(BYTE *buf);


#endif
