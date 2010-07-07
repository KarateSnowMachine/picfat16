typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;
typedef unsigned long qword;

typedef struct _part_entry_t {
	byte active; 
	byte part_begin_head;
	word part_begin_cylinder;
	byte type_code;
	byte part_end_head;
	word part_end_cylinder;
	dword num_sectors_skip;
	dword num_sectors_part;
} __attribute__((packed)) part_entry_t; 

typedef struct _MBR {
	byte boot_code[446];
	part_entry_t partition_table[4]; // 16 bytes each
	byte magic_number[2];
} MBR_t;
typedef struct _boot_record_t {
	byte jump_code[3];
	char oem_name[8];
	word bytes_per_sector;
	byte sectors_per_cluster;
	word reserved_sectors; 
	byte num_copies;
	word max_root_entries; 
	word num_sectors_less_32m; 
	byte media_descriptor; 
	word sectors_per_fat;
	word sectors_per_track;
	word num_heads;
	dword num_hidden_sectors; 
	dword num_sectors_partition; 
	word logical_drive_num_partitions; 
	byte extended_signature;
	dword serial_num;
	char volume_name[11];
	char fat_name[8];
	byte exe_code[448];
	word signature; 
} __attribute__((packed)) boot_record_t;

typedef struct _dir_entry_t {
	char filename[8];
	char extension[3];
	byte attributes;
	byte whatever[14]; 
	word cluster_start;
	dword size;
} __attribute__((packed)) dir_entry_t;

