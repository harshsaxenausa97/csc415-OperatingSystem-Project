
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "tools.h"

argument
split_string(char* line, char* separator)
{
    argument a;
    char *x;
    a.argc = 0;
    a.args = calloc(MAX_NUM_ARGS, sizeof(argument));
    x = strtok(line, separator);
    while(x)
    {
        a.args[a.argc] = malloc(strlen(x));
        strcpy(a.args[a.argc], x);
        if (a.argc == MAX_NUM_ARGS)
        {
            fprintf(stderr, "too many arguments %d\n", a.argc);
            exit(1);
        }
        a.argc++;
        x = strtok(NULL, separator);
    }
    return a;

}

char*
string_dup(char*s)
{
    char *dup;
    unsigned long len;

    len = strlen(s)+1;
    dup = malloc(len);
    memcpy(dup, s, len);
    return dup;
}
