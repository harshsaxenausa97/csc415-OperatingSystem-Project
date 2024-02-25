
#ifndef __fs__
#define __fs__

#include "hash.h"
#include "tools.h"
#include "filesystemdriver.h"

#define FILETYPE_DIRECTORY 1
#define FILETYPE_REGULAR 0

#define MODE_READ 0x01
#define MODE_WRITE 0x02
#define MODE_EXECUTE 0x04

typedef unsigned long BLOCK_LOCATION;   /* pointer to a block */

#define METADATA_SIGNATURE 0xaabbccdd
typedef struct fileStruct {
    unsigned long signature;
    // char id[64];
    unsigned int permissions : 3; /* permissions to read/write/exec/etc */
    /* 001 read, 010 write, 100 execute */
    char modifiedp;
    unsigned long modified;
    unsigned long created;
    unsigned long file_size;
    unsigned char extensionType; /* type of file */
    unsigned long userId;
    unsigned long data_starting_block;  /* only for regular files */
    /* extent helpers -- not useful */
} file_node_metadata;

#define FS_SIGNATURE1 0xaabbccdd
#define FS_SIGNATURE2 0xddccbbaa
typedef struct fileSysInfo {
    unsigned long int signature1;
    BLOCK_LOCATION string_table;
    BLOCK_LOCATION dir_tree;
    BLOCK_LOCATION node_metadata_info;
    BLOCK_LOCATION free_nodes;
    BLOCK_LOCATION data;
    unsigned long int signature2;
} file_sys_info;

typedef
struct dir_tree {
    /* helps identify dir/file/link */
    /* 00 file, 01 directory */
    /* 10 link to file, 11 link to directory */
    unsigned int type : 2;
    unsigned long node_metadata_block_idx;
    unsigned long children_count;
    struct dir_tree **children;
    file_node_metadata *metadata;       /* usualy not loaded */
    obarray* name;
} directory_tree;


#define NUMBLOCKS(bytes) ((bytes-1)/partInfop->blocksize+1)
#define AREA_FILE_OFFSET(block) (((block) * partInfop->blocksize)+ partInfop->blocksize)
#define AREA_OFFSET(init, percent) ((init)+((partInfop->numberOfBlocks*(percent))/100)+1)
#define AREA_FILE_SYS_INFO 0ul
#define AREA_STRING_TABLE   1ul
#define AREA_DIRECTORY_TREE AREA_OFFSET(AREA_STRING_TABLE, 5)
#define AREA_NODES_METADATA AREA_OFFSET(AREA_DIRECTORY_TREE, 5)
#define AREA_FREE_NODES     AREA_OFFSET(AREA_NODES_METADATA, 10)
#define AREA_DATA           AREA_OFFSET(AREA_FREE_NODES, 5)

#define AREA_SIZE(AREA, NEXT_AREA) (BLOCK_COUNT_AREA(AREA, NEXT_AREA)*partInfop->blocksize)
#define SIZE_AREA_STRING_TABLE   AREA_SIZE(AREA_STRING_TABLE, AREA_DIRECTORY_TREE)
#define SIZE_AREA_FREE_NODES     AREA_SIZE(AREA_FREE_NODES, AREA_DATA)

#define BLOCK_COUNT_AREA(AREA, NEXT_AREA) (NEXT_AREA-AREA)
#define BLOCK_COUNT_AREA_NODES_METADATA BLOCK_COUNT_AREA(AREA_NODES_METADATA, AREA_FREE_NODES)
#define BLOCK_COUNT_AREA_DATA BLOCK_COUNT_AREA(AREA_DATA, partInfop->numberOfBlocks)
#define BLOCK_COUNT_FREE_NODES NUMBLOCKS(SIZE_AREA_FREE_NODES)

/*
 Our free space is basically a doubly linked list with two
 signatures. It has the count of contiguous free blocks, a next
 pointer, a previous pointer, and two signatures which, in order to be
 valid free blocks, must match the const values defined in the struct
 fileSysInfo.

 Our method for handling extents is much like our method for handling
 the free space list. Our file struct has an element that has a count
 of the number of blocks in the current contiguous section, an element
 that points to the next block, and an element that points to the
 previous block.
 */

#define FREESIGNATURE1 0x03E6D615D98394
#define FREESIGNATURE2 0x1B4FA21A7FA64A
/*     unsigned long long int signature1; */
/*     unsigned long long int signature2; */
typedef struct free_space_sector {
    unsigned long start_block;
    unsigned long count_blocks;
    struct free_space_sector *next;
    struct free_space_sector *prev;
} free_space_sector;

extern obarray *hash_files;
extern directory_tree* directory_tree_root;
extern unsigned long directory_tree_nodes_count;
extern unsigned long total_free_data_blocks;

directory_tree* search_child_node(directory_tree*, char*);
directory_tree* create_new_directory(directory_tree*, char*);
directory_tree* create_new_file(directory_tree*, char*, unsigned long);
directory_tree *dirtree_follow_path(argument, boolean);


void remove_file_node(directory_tree*, directory_tree*);
void remove_directory_node(directory_tree*, directory_tree*);
void print_children(directory_tree*);
void dbg_directory_tree(void);
void load_file_system_info(void);
void format_file_system(void);
void close_file_system(void);
void sync_file_system(void);
void set_metadata_modification_time(directory_tree*);
void set_metadata_creation_time(directory_tree*);
void dbg_free_blocks(void);

boolean file_system_modified;

#endif


