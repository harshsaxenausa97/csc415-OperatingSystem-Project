
#ifndef __hash__
#define __hash__

#include <limits.h>

#define MAX_VALID_CHAR CHAR_MAX

struct obarray_node
{
    struct obarray_node* input[MAX_VALID_CHAR];
    int uniq_id;
    int depth;
    int use_count;
    void *object;                       /* for file names */
    void (*proc)();                     /* for command callbacks */
    unsigned long offset;               /* offset in string table */
} typedef obarray;

obarray* intern(char*, obarray*);
obarray* obarray_mk_root(void);
obarray* search_obarray(char*, obarray*);
char* obarray_mk_linear_table(obarray*, char*, unsigned long *);

#endif

