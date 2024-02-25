
#ifndef __tools__
#define __tools__

#define MAX_NUM_ARGS 100
typedef struct {
    int argc;
    char **args;
}argument;

typedef enum
{
    TRUE=1,
    FALSE=0
} boolean;


argument split_string(char* line, char* separator);
char* string_dup(char*);


#endif