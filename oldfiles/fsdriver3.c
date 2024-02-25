
#include <sys/types.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <string.h>

#include "fsLow.h"
#include "hash.h"
#include "command.h"
#include "tools.h"
#include "filesystemdriver.h"
#include "fs.h"

char * filename;
uint64_t volumeSize;
uint64_t blockSize;

/* read eval print loop */
void
repl(void)
{
    char *command;
    argument a;
    int i;

    system_exit = FALSE;
    while(system_exit!=TRUE)
    {
        command = readline ("> ");
        add_history(command);
        if (!command)
          return;
        a = split_string(command, " ");
        if (!a.args[0])
          continue;
        execute_command(a.args[0], a.argc-1, a.args+1);
        for(i=0; i<a.argc-1; i++)
        {
          free(a.args[i]);
        }
        free(a.args);
        free(command);
    }
}

int
main (int argc, char *argv[])
{
    int retVal;
    if (argc > 3)
    {
        filename = argv[1];
        volumeSize = atoll (argv[2]);
        blockSize = atoll (argv[3]);
    }
    else
    {
        fprintf(stderr, "repl <Partition File> <Volume Size> <BlockSize>");
        exit(1);
    }
    retVal = startFileSystem( filename, volumeSize, blockSize );
    printf("Opened %s, Volume Size: %llu;  BlockSize: %llu; Return %d\n", filename, (ull_t)volumeSize, (ull_t)blockSize, retVal);
    if (retVal)
    {
        fprintf(stderr, "startPartitionSystem error %d", retVal);
        exit(retVal);
        
    }
    hash_files=obarray_mk_root();
    hash_commands=obarray_mk_root();

    load_file_system_info();
    dbg_directory_tree();

    repl();
    closeFileSystem();
}


