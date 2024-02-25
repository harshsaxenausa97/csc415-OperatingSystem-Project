
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "hash.h"
#include "command.h"
#include "tools.h"
#include "fsLow.h"
#include "filesystemdriver.h"

obarray *hash_commands;

static char* error;
boolean system_exit;

DEF(LIST, "ls")
{
    CHECK_NUM_PARAMS(1);
}

DEF(MAKE_DIRECTORY, "mkdir")
{
    CHECK_NUM_PARAMS(1);
    CHECK (!directory_tree_root, ERR_LIST_FILE_SYSTEM_NOT_FORMATTED);

    path = ARG1;
    a = split_string(path, "/");
    node=dirtree_follow_path(a, FALSE);
    CHECK(!node, ERR_INVALID_PATH);
    CHECK(search_child_node(node, LAST_ARG), ERR_DIRECTORY_EXISTS);
    printf("create directory %s\n", LAST_ARG);
    create_new_directory(node, LAST_ARG);
    dbg_directory_tree();
}

DEF(REMOVE_DIRECTORY, "rmdir")
{
    char*path;
    argument a;
    directory_tree *child, *parent;

    CHECK_NUM_PARAMS(1);
    CHECK (!directory_tree_root, ERR_LIST_FILE_SYSTEM_NOT_FORMATTED);

    path = ARG1;
    a = split_string(path, "/");
    parent=dirtree_follow_path(a, FALSE);
    CHECK(!parent || (parent->type!=FILETYPE_DIRECTORY), ERR_INVALID_PATH);
    if (!LAST_ARG)
    {
        /* remove root node */
        remove_directory_node(parent, NULL);
        return;
    }
    child = search_child_node(parent, LAST_ARG);
    CHECK(!child, ERR_INVALID_PATH);
    CHECK(child->type!=FILETYPE_DIRECTORY, ERR_EXPECTED_DIRECTORY);
    printf("remove directory %s\n", LAST_ARG);
    remove_directory_node(child, parent);
    dbg_directory_tree();
}

DEF(FILE_ALLOCATE, "fallocate")
{
    char*path;
    unsigned long file_size;
    argument a;
    directory_tree *child, *parent;
    
    CHECK_NUM_PARAMS(2);
    CHECK (!directory_tree_root, ERR_LIST_FILE_SYSTEM_NOT_FORMATTED);

    file_size = (unsigned long) atoll(ARG1);
    path = ARG2;
    CHECK((file_size==0)||(file_size>partInfop->volumesize), ERR_INVALID_FILE_SIZE);
    a = split_string(path, "/");
    parent=dirtree_follow_path(a, FALSE);
    CHECK(!parent, ERR_INVALID_PATH);
    child = search_child_node(parent, LAST_ARG);
    if (!child)
    {
        child = create_new_file(parent, LAST_ARG, file_size);
        CHECK(!child, ERR_CANNOT_CREATE_FILE);
        set_metadata_creation_time(child);
    }
    /* set timestamp */
    set_metadata_modification_time(child);
}

DEF(REMOVE, "rm")
{
    char*path;
    argument a;
    directory_tree *child, *parent;
    
    CHECK_NUM_PARAMS(1);
    CHECK (!directory_tree_root, ERR_LIST_FILE_SYSTEM_NOT_FORMATTED);

    path = ARG1;
    a = split_string(path, "/");
    parent=dirtree_follow_path(a, FALSE);
    CHECK(!parent, ERR_INVALID_PATH);
    child = search_child_node(parent, LAST_ARG);
    CHECK(!child, ERR_INVALID_PATH);
    CHECK(child->type!=FILETYPE_REGULAR, ERR_EXPECTED_FILE);
    printf("remove file %s\n", LAST_ARG);
    remove_file_node(child, parent);
}

/* TODO */
DEF(CONCATENATE, "cat")
{
    CHECK_NUM_MIN_PARAMS(1);
}

/* TODO */
DEF(COPY, "cp")
{
    printf("COPY\n");
    CHECK_NUM_PARAMS(2);
}

/* TODO */
DEF(MOVE, "mv")
{
    CHECK_NUM_PARAMS(1);

    printf("MOVE\n");

}

/* TODO */
DEF(CHANGE_MODE, "chmod")
{
    CHECK_NUM_PARAMS(2);
    printf("CHANGE_MODE\n");

}

/* TODO */
DEF(LINUX_TO_CSC415, "linux->csc415")
{
    CHECK_NUM_PARAMS(2);
    printf("LINUX_TO_CSC415\n");

}

/* TODO */
DEF(CSC415_TO_LINUX, "csc415->linux")
{
    CHECK_NUM_PARAMS(2);
    printf("CSC415_TO_LINUX\n");
}

DEF(EXIT, "exit")
{
    CHECK_NUM_PARAMS(0);
    system_exit = TRUE;
}

DEF(DUMP_SYSTEM, "dump")
{
    CHECK_NUM_PARAMS(0);
    printf("volume size: %lu\n", partInfop->volumesize);
    printf("block size: %lu\n", partInfop->blocksize);
    printf("number of blocks: %lu\n", partInfop->numberOfBlocks);
    printf("header size for each file metadata %lu\n", sizeof(file_node_metadata));
    printf("header size for each tree node %lu\n", sizeof(directory_tree));
    printf("offset area string table %lu / %lx\n",   AREA_STRING_TABLE,   AREA_FILE_OFFSET(AREA_STRING_TABLE));
    printf("offset area directory tree %lu / %lx\n", AREA_DIRECTORY_TREE, AREA_FILE_OFFSET(AREA_DIRECTORY_TREE));
    printf("offset area nodes metadata %lu / %lx\n", AREA_NODES_METADATA, AREA_FILE_OFFSET(AREA_NODES_METADATA));
    printf("offset area free nodes %lu / %lx\n",     AREA_FREE_NODES,     AREA_FILE_OFFSET(AREA_FREE_NODES));
    printf("offset area data %lu / %lx\n",           AREA_DATA,           AREA_FILE_OFFSET(AREA_DATA));
    printf("total tree nodes %lu\n", directory_tree_nodes_count);
    printf("maximum tree nodes %lu\n", BLOCK_COUNT_AREA_NODES_METADATA);
    printf("number of free data blocks %lu\n", total_free_data_blocks);
    printf("file_system_modified: %s\n", file_system_modified==TRUE? "true":"false");
    dbg_directory_tree();
    dbg_free_blocks();
}

DEF(MAKE_FILE_SYSTEM, "mkfs")
{
    CHECK_NUM_PARAMS(0);
    printf("Filesystem is formatted and ready to use\n");
    format_file_system();
}

DEF(SYNCRONIZE, "sync")
{
    CHECK_NUM_PARAMS(0);
    CHECK(file_system_modified==FALSE, ERR_NOTHING_TO_SYNCRONIZE);
    printf("sync file system\n");
    sync_file_system();
}

DEF(HELP, "help")
{
    printf("HELP\n");
    CHECK_OPTIONAL_PARAMS(1);
}

void
intern_command_list(void)
{
    intern_MAKE_FILE_SYSTEM();
    intern_DUMP_SYSTEM();
    intern_CONCATENATE();
    intern_LIST();
    intern_MAKE_DIRECTORY();
    intern_SYNCRONIZE();
    intern_FILE_ALLOCATE();
    intern_REMOVE_DIRECTORY();
    intern_EXIT();
    intern_REMOVE();
}

void
execute_command(char* comm, int argc, char**arguments)
{
    obarray* o;
    o=search_obarray(comm, hash_commands);
    if (o && o->proc)
    {
        error = NULL;
        ((command_procedure*)(o->proc))(argc, arguments);
        (void)(error && printf(error) && printf("\n"));
        return;
    }
    printf("unknown command %s\n", comm);
}
