
/* internal functions for the file system */

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "hash.h"
#include "fs.h"
#include "fsLow.h"
#include "tools.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

directory_tree *directory_tree_root;
unsigned long directory_tree_nodes_count;
boolean file_system_modified;

obarray *hash_files;
file_sys_info fs;
static char *BUFFER;
/* AREA STRING TABLE */
static char *string_table;
unsigned long size_string_table;
/* AREA METADATA */
boolean *free_metadata_indices;
unsigned long max_metadata_indice;
/* AREA FREE NODES */
free_space_sector first_free_data_sector;
unsigned long total_free_data_blocks;

typedef void(*directory_tree_iterator_callback)(directory_tree *);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void load_string_table();
static void load_directory_tree();
static void sync_save_string_table(void);
static file_node_metadata *allocate_node_metadata(void);
static void maybe_load_metadata(directory_tree *);

static void sync_save_directory_tree(void);
static void load_directory_tree(void);
static void sync_save_string_table(void);
static void load_string_table(void);
static unsigned long get_metadata_free_block(void);
static void load_free_data_nodes(void);
static void sync_save_free_data_nodes(void);
static unsigned long allocate_free_data_block(unsigned long);
static void deallocate_free_data_block(directory_tree*);


static void iterate_directory_tree(directory_tree*, directory_tree_iterator_callback);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static directory_tree*
allocate_node(char* name, unsigned type)
{
    directory_tree *new;
    obarray* o;
    unsigned long new_index;

    if (name)
    {
        o = intern(name, hash_files);
        if (!o->object)
        {
            o->object = malloc(o->depth+1);
            memcpy(o->object, name, o->depth+1);
        }
    }
    else
        o = NULL;

    new = malloc(sizeof(directory_tree));
    new_index = get_metadata_free_block();
    new->node_metadata_block_idx = new_index;
    new->children_count = 0;
    new->children = NULL;
    new->name = o;
    new->metadata = allocate_node_metadata();
    new->type = type;
    free_metadata_indices[new_index] = TRUE;
    directory_tree_nodes_count++;
    return new;
}

void
set_metadata_creation_time(directory_tree* node)
{
    ;
}

void
set_metadata_modification_time(directory_tree* node)
{
    ;
}

static file_node_metadata *
allocate_node_metadata(void)
{
    file_node_metadata *metadata;

    metadata = malloc(sizeof(file_node_metadata));
    metadata->signature = METADATA_SIGNATURE;
    metadata->permissions = MODE_READ + MODE_WRITE + MODE_EXECUTE;
    metadata->modified = metadata->created = -1ul;
    metadata->data_starting_block = -1ul;
    metadata->modifiedp = 1;
    return metadata;
}

directory_tree*
search_child_node(directory_tree* node, char* name)
{
    unsigned int children_count, child_idx;
    directory_tree *child;
    obarray* o;

    o=search_obarray(name, hash_files);
    if(!o) return NULL;
    child_idx = children_count = node->children_count;
    while(child_idx--)
    {
        child = node->children[child_idx];
        if (child->name == o)
            return child;
    }
    return NULL;
}

static void
remove_directory_tree_node(directory_tree* parent, directory_tree* child)
{
    unsigned long x;

    if (!parent) return;
    x = 0;
    while(parent->children[x] != child)
        x++;
    assert(child==parent->children[x]);
    parent->children_count--;
    while(x<parent->children_count)
    {
        parent->children[x] = parent->children[x+1];
        x++;
    }
    parent->children=realloc(parent->children, parent->children_count*sizeof (directory_tree*));
    
}

static directory_tree*
new_directory_tree_node(directory_tree* node, char* name, unsigned type)
{
    directory_tree *new_node;
    new_node = allocate_node(name, type);
    node->children_count++;
    node->children =
    realloc(node->children,
            sizeof(directory_tree*)*
            node->children_count);
    node->children[node->children_count-1] = new_node;
    file_system_modified = TRUE;
    return new_node;
}

/* MKDIR */
directory_tree*
create_new_directory(directory_tree* node, char* name)
{
    directory_tree *new_node;
    
    new_node = new_directory_tree_node(node, name, FILETYPE_DIRECTORY);
    return new_node;
}

/* FALLOCATE */
directory_tree*
create_new_file(directory_tree* node, char* name, unsigned long size)
{
    directory_tree *new_node;
    unsigned long data_block;

    data_block = allocate_free_data_block(size);
    if (-1ul == data_block) return NULL;
    new_node = new_directory_tree_node(node, name, FILETYPE_REGULAR);
    new_node->metadata->data_starting_block = data_block;
    new_node->metadata->file_size = size;
    return new_node;
}

void
print_children(directory_tree* node)
{
    unsigned long i;
    directory_tree *child;
    unsigned type;
    
    i = 0;
    while (i < node->children_count)
    {
        child = node->children[i];
        type = (child->type == FILETYPE_REGULAR)?
        'f':(child->type == FILETYPE_DIRECTORY)?
        'd':'?';
        printf("%c %s\n", type, (char*)child->name->object);
        i++;
    }
}

directory_tree *
dirtree_follow_path(argument a, boolean include_last_name)
{
    int i, last;
    directory_tree *node;

    node = directory_tree_root;
    last = a.argc-(include_last_name?0:1);
    for(i=0; i<last; i++)
    {
        node = search_child_node(node, a.args[i]);
        if (!node||(FILETYPE_DIRECTORY!=node->type))
            return NULL;
    }
    return node;
}

void
format_file_system(void)
{
    file_sys_info x;
    
    directory_tree_root = NULL;
    directory_tree_nodes_count = 0;
    total_free_data_blocks = BLOCK_COUNT_AREA_DATA;
    first_free_data_sector.start_block = AREA_DATA;
    first_free_data_sector.count_blocks = BLOCK_COUNT_AREA_DATA;

    x.signature1 = FS_SIGNATURE1;
    x.signature2 = FS_SIGNATURE2;
    x.string_table = AREA_STRING_TABLE ;
    x.dir_tree = AREA_DIRECTORY_TREE ;
    x.node_metadata_info = AREA_NODES_METADATA;
    x.free_nodes = AREA_FREE_NODES ;
    x.data = AREA_FREE_NODES ;
    
    memset (BUFFER, 'I', partInfop->blocksize);
    memcpy(BUFFER, &x, sizeof(file_sys_info));
    LBAwrite (BUFFER, 1, 0);
    directory_tree_root = allocate_node(NULL, FILETYPE_DIRECTORY);

    file_system_modified = TRUE;
}

/* called once only at initialization */
void
load_file_system_info(void)
{
    file_sys_info *x;
    
    if (sizeof (file_sys_info) > MINBLOCKSIZE)
    {
        printf("file_sys_info size too large %lu\n", sizeof (file_sys_info));
        exit(1);
    }

    max_metadata_indice = BLOCK_COUNT_AREA_NODES_METADATA;
    free_metadata_indices = malloc(max_metadata_indice*sizeof(boolean));
    
    BUFFER = malloc(partInfop->blocksize);
    LBAread (BUFFER, 1, AREA_FILE_SYS_INFO);
    x = (file_sys_info*) BUFFER;
    directory_tree_root = NULL;
    size_string_table = SIZE_AREA_STRING_TABLE;
    string_table = malloc(size_string_table);
    if ((x->signature1 == FS_SIGNATURE1) &&
        (x->signature2 == FS_SIGNATURE2))
    {
        printf("file system found\n");
        memcpy(&fs, x, sizeof(file_sys_info));
        load_string_table();
        load_directory_tree();
        load_free_data_nodes();
    }
    else
    {
        printf("partition not formatted.\n");
        memset(&fs, 0, sizeof(file_sys_info));
    }

    file_system_modified = FALSE;
}

/* called once only at exit */
void
close_file_system(void)
{
    /* sync */
    sync_file_system();
    free(BUFFER);
    free(string_table);

    /* TODO: deallocate all memory */
    
}

/* DIRECTORY TREE INTERNAL FUNCTIONS */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void static
iterate_directory_tree(directory_tree* root, directory_tree_iterator_callback callback)
{
    directory_tree *node, **stack, **sp;
    unsigned long child_idx;

    if (!root)
        return;
    stack = malloc(sizeof(directory_tree*)*directory_tree_nodes_count);
    *stack = root;
    sp = stack+1;
    while(sp-->stack)
    {
        node = *sp;
        callback(node);
        for(child_idx = 0; child_idx<node->children_count; child_idx++)
            *(sp++) = node->children[child_idx];
    }
    free(stack);
}

static void
iter_dbg(directory_tree *node)
{
    unsigned long x;
    unsigned type;

    if (!node)
        return;
    
    type = (node->type == FILETYPE_REGULAR)?
    'f':(node->type == FILETYPE_DIRECTORY)?
    'd':'?';

    printf("node:%lu type:%c %s children count:%lu ",
           node->node_metadata_block_idx,
           type,
           node->name?(char*)node->name->object:"/",
           node->children_count);
    for(x = 0; x<node->children_count; x++)
    {
        printf(". %lu ", (node->children[x])->node_metadata_block_idx );
    }
    printf("\n");
}

void
dbg_directory_tree(void)
{
    printf("tree: [%lu nodes]\n", directory_tree_nodes_count);
    iterate_directory_tree(directory_tree_root, iter_dbg);
}

unsigned long *iter__out, *iter__op;
static void
iter_save(directory_tree *node)
{
    unsigned long x;
    
    *(iter__op++) = node->node_metadata_block_idx; /* node */
    *(iter__op++) = node->type;           /* file or directory */
    *(iter__op++) = node->children_count; /* children_count */
    *(iter__op++) = node->name? node->name->offset:0ul; /* filename hash */
    for(x = 0; x<node->children_count; x++)
        *(iter__op++) = node->children[x]->node_metadata_block_idx;
}

static void
sync_save_directory_tree(void)
{
    unsigned long outblocks;
    
    /* number of out blocks: each node can appear maximum 2 times and
     * for each node one see the number of childre.  so maximum is 3
     * times the number of nodes.
     * Output format:
     NODES_COUNT BLOCK_COUNT [NODE TYPE CHILDREN_COUNT FILENAME_HASH CHILD*]+ */

    outblocks = (sizeof(long)*directory_tree_nodes_count*3)/partInfop->blocksize+1;
    iter__out = malloc(outblocks * partInfop->blocksize);
    memset (iter__out, 'T', outblocks * partInfop->blocksize);
    printf("save tree: [%lu nodes]\n", directory_tree_nodes_count);
    *iter__out = directory_tree_nodes_count;
    /* out[1] will be completed to the end */
    iter__op = iter__out+2;
    iterate_directory_tree(directory_tree_root, iter_save);
    LBAwrite (iter__out, iter__out[1]=NUMBLOCKS(iter__op-iter__out), AREA_DIRECTORY_TREE);
    free(iter__out);
}

static void
load_directory_tree(void)
{
    char* buf;
    unsigned long numblocks, *ptr, node_idx, type, children_count, x, child_idx, name, count;
    directory_tree *tree, *tree_node;
    
    printf("load tree structure\n");
    LBAread (BUFFER, 1, AREA_DIRECTORY_TREE);
    count = directory_tree_nodes_count = *((unsigned long*)BUFFER);
    printf("tree_nodes_count %lu\n", count);
    numblocks = *((unsigned long*)BUFFER+1);
    printf("numblocks %lu\n", numblocks);
    ptr = ((unsigned long*)(buf = malloc(numblocks*partInfop->blocksize)))+2;
    LBAread (buf, numblocks, AREA_DIRECTORY_TREE);
    tree = malloc(BLOCK_COUNT_AREA_NODES_METADATA*sizeof(directory_tree));
    directory_tree_root = tree;
    while(count--)
    {
        node_idx=*ptr++;
        type = *ptr++;
        children_count = *ptr++;
        name = *ptr++;
        printf("N:%lu T:%lu CC:%lu [%s] ",
               node_idx,
               type,
               children_count,
               string_table+name);
        free_metadata_indices[node_idx] = TRUE;
        tree_node = tree+node_idx;
        tree_node->node_metadata_block_idx = node_idx;
        tree_node->type = (unsigned)type;
        tree_node->children_count = children_count;
        if (!*(string_table+name))
            tree_node->name = NULL;
        else
            tree_node->name = search_obarray(string_table+name, hash_files),
            tree_node->name->object = string_dup(string_table+name);
        tree_node->children=malloc(children_count*sizeof(directory_tree*));
        x = 0;
        while(x < children_count)
        {
            child_idx=*ptr++;
            printf(" .%lu", child_idx);
            tree_node->children[x] = tree+child_idx;
            x++;
        }
        if (children_count)
            printf("\n");
    }
    printf("\n");
}

directory_tree **remove_node_list;
unsigned long remove_node_list_index;
static void
iter_remove(directory_tree *node)
{
    remove_node_list[remove_node_list_index++] = node;
}

/* RMDIR */
void
remove_directory_node(directory_tree* child, directory_tree* parent)
{
    unsigned long i;
    directory_tree* x;
    
    remove_node_list_index = 0;
    remove_node_list = malloc(directory_tree_nodes_count*sizeof(directory_tree*));
    iterate_directory_tree(child, iter_remove);
    remove_directory_tree_node(parent, child);
    i = 0;
    while (i<remove_node_list_index)
    {
        x = remove_node_list[i];
        printf("remove node %lu\n", x->node_metadata_block_idx);
        free_metadata_indices[x->node_metadata_block_idx] = FALSE;
        free(x->children);
        if (x->metadata) free(x->metadata);
        if (x->name) x->name->use_count--;
        directory_tree_nodes_count--;
        i++;
    }
    if (!parent) directory_tree_root = NULL;
    file_system_modified = TRUE;
}

/* RM */
void
remove_file_node(directory_tree* child, directory_tree* parent)
{
    remove_directory_tree_node(parent, child);
    directory_tree_nodes_count--;
    if (child->metadata) free(child->metadata);
    if (child->name) child->name->use_count--;
    deallocate_free_data_block(child);
}

/* METADATA INTERNAL FUNCTIONS */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void
iter_save_metadata(directory_tree *node)
{
    if (node->metadata)
    {
        printf("save metadata for %lu\n", node->node_metadata_block_idx);
        memset (BUFFER, 0, partInfop->blocksize);
        memcpy(BUFFER, node->metadata, sizeof(file_node_metadata));
        LBAwrite (BUFFER, 1, AREA_NODES_METADATA+node->node_metadata_block_idx);
    }
}
    
static void
sync_save_metadata(void)
{
    printf("save metadata\n");
    iterate_directory_tree(directory_tree_root, iter_save_metadata);
}

static void
maybe_load_metadata(directory_tree *node)
{
    file_node_metadata *metadata;
    
    if (node->metadata) return;
    LBAread (BUFFER, 1, AREA_NODES_METADATA+node->node_metadata_block_idx);
    metadata = malloc(sizeof(file_node_metadata));
    memcpy(metadata, BUFFER, sizeof(file_node_metadata));
    node->metadata = metadata;
}

/* STRING TABLE INTERNAL FUNCTIONS */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static void
load_string_table(void)
{
    char *p;

    printf("Load string table\n");
    LBAread (string_table, NUMBLOCKS(size_string_table)-1, AREA_STRING_TABLE);
    p = string_table+1;
    while(*p)
    {
        printf("intern %s\n", p);
        intern(p, hash_files);
        p+=strlen(p)+1;
    }
}

static void
sync_save_string_table(void)
{
    char *y;
    unsigned long s;
    
    printf("sync string table\n");
    s = 1, *string_table = 0; /* root node has no name and points to 0 offset */
    memset(string_table, 0, size_string_table);
    y=obarray_mk_linear_table(hash_files, string_table+1, &s);
    LBAwrite (string_table, NUMBLOCKS(y-string_table), AREA_STRING_TABLE);
}

/* FREE DATA AND METADATA BLOCKS -- INTERNAL FUNCTIONS */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static unsigned long
get_metadata_free_block(void)
{
    unsigned long i;

    i=0;
    while(free_metadata_indices[i]) i++;
    return i;
}

static void
load_free_data_nodes(void)
{
    unsigned long *buf, *p, count;
    free_space_sector *x, *new;

    buf = malloc(SIZE_AREA_FREE_NODES);
    LBAread (buf, BLOCK_COUNT_FREE_NODES, AREA_FREE_NODES);
    p=buf;
    count = *p++;
    if (*((unsigned long *)"FFFFFFFFFFFFFFFF") != *p++)
        printf("invalid free node data\n");
    x = &first_free_data_sector;
    x->start_block = *p++;
    x->count_blocks = *p++;
    x->prev = x->next = NULL;
    printf("load free data nodes [%lu]\n", count);
    while(--count)
    {
        new = malloc(sizeof(free_space_sector));
        new->start_block = *p++;
        new->count_blocks = *p++;
        new->next = NULL;
        new->prev = x;
        x->next = new;
        x = new;
    }
}

static void
sync_save_free_data_nodes(void)
{
    unsigned long free_sectors_count, *outbuf, *op;
    free_space_sector *x;

    /* Output format:
     * SECTORS_COUNT [START_FREE_BLOCK COUNT_FREE_BLOCKS]+
     */
    free_sectors_count=0;
    op = (outbuf = malloc(SIZE_AREA_FREE_NODES))+2;
    memset (outbuf, 'F', SIZE_AREA_FREE_NODES);
    x = &first_free_data_sector;
    while(x)
    {
        *op++=x->start_block;
        *op++=x->count_blocks;
        x = x->next, free_sectors_count++;
    }
    printf("save free data nodes: [%lu ]\n", free_sectors_count);
    LBAwrite (outbuf, *outbuf = free_sectors_count, AREA_FREE_NODES);
    free(outbuf);
}

void
dbg_free_blocks(void)
{
    free_space_sector *x;

    x = &first_free_data_sector;
    printf("free blocks: ");
    while(x)
    {
        printf("%lu->%lu[%lu]  ",
               x->start_block,
               x->start_block + x->count_blocks,
               x->count_blocks);
        x = x->next;
    }
    printf("\n");
}

static void
deallocate_free_data_block(directory_tree* node)
{
    free_space_sector *sector, *new, *sect_prev;
    unsigned long data_start_block, data_end_block, data_num_blocks;
    
    maybe_load_metadata(node);
    data_start_block = node->metadata->data_starting_block;
    data_num_blocks = NUMBLOCKS(node->metadata->file_size);
    data_end_block = data_start_block+data_num_blocks;
    printf("deallocate data from %lu to %lu\n",
           data_start_block, data_end_block-1);
    sector = &first_free_data_sector;
    while(sector && (sector->start_block<data_start_block))
        sector = sector->next;
    assert(sector);
    sect_prev = sector->prev;
    /* file is before FIRST FREE SECTOR */
    if (!sect_prev)
    {
        if (sector->start_block == data_end_block)
        {
            first_free_data_sector.start_block = data_start_block;
            first_free_data_sector.count_blocks+=data_num_blocks;
        }
        else
        {
            new = malloc(sizeof(free_space_sector));
            memcpy(new, &first_free_data_sector, sizeof(free_space_sector));
            new->prev = &first_free_data_sector;
            first_free_data_sector.next = new;
            first_free_data_sector.start_block = data_start_block;
            first_free_data_sector.count_blocks = data_num_blocks;
        }
    }
    /* file is in middle of free nodes */
    else
    {
        unsigned long end_prev_sector;

        end_prev_sector = sect_prev->start_block+ sect_prev->count_blocks;
        /* unify sector with previous sector */
        if ((data_start_block == end_prev_sector) &&
            (data_end_block == sector->start_block))
        {
            sect_prev->next = sector->next;
            sect_prev->count_blocks += sector->count_blocks + data_num_blocks;
            if (sector->next)
                sector->next->prev = sect_prev;
            free(sector);
        }
        /* enlarge previous sector */
        else if (end_prev_sector == data_start_block)
        {
            sect_prev->count_blocks+=data_num_blocks;
        }
        /* enlarge sector */
        else if (sector->start_block == data_end_block)
        {
            sector->start_block-=data_num_blocks;
            sector->count_blocks+=data_num_blocks;
        }
        /* add sector in middle of free sectors */
        else
        {
            new = malloc(sizeof(free_space_sector));
            new->next = sector;
            new->prev = sect_prev;
            new->start_block = data_start_block;
            new->count_blocks = data_num_blocks;
            sector->prev = sect_prev->next = new;
        }
        
    }
    dbg_free_blocks();
}

static unsigned long
allocate_free_data_block(unsigned long size)
{
    unsigned long data_block = -1ul;
    unsigned long num_blocks;
    free_space_sector *sector;
    
    sector = &first_free_data_sector;
    num_blocks = NUMBLOCKS(size);

    printf("try to allocate %lu blocks of data\n", num_blocks);
    dbg_free_blocks();
    
    while(sector && sector->count_blocks<num_blocks)
        sector=sector->next;
    if (!sector) return -1ul;           /* no free space found */

    data_block = sector->start_block;
    
    if (sector->prev)
    {
        sector->count_blocks-=num_blocks;
        sector->start_block+=num_blocks;
        /* remove empty sector unless it is the last one */
        if (sector->next && !sector->count_blocks)
        {
            sector->prev->next = sector->next;
            sector->next->prev = sector->prev;
            free(sector);
        }
        
    }
    else
    {
        /* allocate in first sector */
        first_free_data_sector.count_blocks -= num_blocks;
        first_free_data_sector.start_block += num_blocks;
        if (first_free_data_sector.next &&
            !first_free_data_sector.count_blocks)
        {
            sector = first_free_data_sector.next;
            first_free_data_sector = *sector;
            first_free_data_sector.prev = NULL;
            free(sector);
        }
    }
    printf("allocate %lu data block%s at %lu/#x%lx [to %lu]\n",
           num_blocks, num_blocks==1?"":"s", data_block, data_block, data_block+num_blocks-1 );
    dbg_free_blocks();
    
    return data_block;
}

/* SYNC */
void
sync_file_system(void)
{
    if (FALSE==file_system_modified) return;
    sync_save_string_table();
    sync_save_directory_tree();
    sync_save_metadata();
    sync_save_free_data_nodes();
    file_system_modified = FALSE;
}

