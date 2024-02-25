#include "tools.h"

#include "tools.h"

typedef void (command_procedure) (int, char**);

obarray *hash_commands;
obarray *hash_files;
boolean isRunning;

extern boolean system_exit;

void intern_command_list(void);
void execute_command(char*, int, char**);

#define DEF(command, str)                        \
    static command_procedure command;            \
    void intern_##command(void)                  \
   {                                             \
       obarray* o=intern(str, hash_commands);    \
       printf("intern " str "\n");               \
       o->proc=(command_procedure*)command;      \
   }                                             \
   static void command(int argc, char** args)

#define ARG(n) args[n]
#define ARG1 ARG(0)
#define ARG2 ARG(1)
#define ARG3 ARG(2)
#define ARGC argc
#define LAST_ARG (a.argc? (a.args[a.argc-1]):NULL)

#define CHECK_NUM_PARAMS(N)                 \
    if (ARGC!=N) {                          \
        error = ERR_WRONG_NUM_PARAMS        \
        return;                             \
    }

#define CHECK_OPTIONAL_PARAMS(N)                \
   do {if (ARGC>N) CHECK_NUM_PARAMS(-3);} while(0)

#define CHECK_NUM_MIN_PARAMS(N)                       \
   do {if (ARGC<N) CHECK_NUM_PARAMS(-3);} while(0)


#define CHECK(cond, errmsg)                        \
    do {if (cond) {error = errmsg; return;}} while(0)

#define ERR_LIST_FILE_SYSTEM_NOT_FORMATTED "file system is not formatted"
#define ERR_INVALID_PATH "path is not valid"
#define ERR_DIRECTORY_EXISTS "directory already exists"
#define ERR_EXPECTED_DIRECTORY "expected directory as operand"
#define ERR_EXPECTED_FILE "expected file as operand"
#define ERR_FILE_NOT_EXISTS "file does not exist"
#define ERR_WRONG_NUM_PARAMS "wrong number of operands";
#define ERR_EXPECTED_REGULAR_FILE "expected regular file as parameter"
#define ERR_NOTHING_TO_SYNCRONIZE "system was not modified"
#define ERR_CANNOT_CREATE_FILE "new file creation error"
#define ERR_INVALID_FILE_SIZE "file size is not valid"
