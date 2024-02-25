
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "command.h"

static obarray *obarray_new_node (obarray *, unsigned char);

obarray*
intern(char* str, obarray* node)
{
    unsigned char c;
  
    while ((c=*str))
    {
        if (node->input[c])
            node = node->input[c];
        else
            node = obarray_new_node(node, c);
        str++;
    }
    return node;
}

obarray*
search_obarray(char* str, obarray*node)
{
    unsigned char c;
  
    while ((c=*str))
    {
        if (node->input[c])
            node = node->input[c];
        else
            return NULL;
        str++;
    }
    return node;
}

static obarray *
obarray_new_node (obarray *parent, unsigned char input)
{
    obarray *node = malloc(sizeof(obarray));
    memset(node, 0, sizeof(obarray));
    node->object = NULL;
    node->proc = NULL;
    node->offset = -1ul;
    node->depth = parent->depth+1;
    parent->input[input] = node;
    return node;
}

obarray*
obarray_mk_root (void)
{
    obarray* obarray_root;
    obarray_root = malloc(sizeof(obarray));
    memset(obarray_root->input, 0, sizeof(obarray_root->input));
    obarray_root->uniq_id =
    obarray_root->depth =
    obarray_root->use_count = 0;
    obarray_root->object=NULL;
    obarray_root->proc=NULL;
    return obarray_root;
}

/* when obarray->object points to strings */
char*
obarray_mk_linear_table(obarray* node, char* out, unsigned long *offset)
{
    int i;

    if (node->object)
    {
        unsigned long len;
        len = node->depth;
        node->offset = *offset;
        memcpy(out, (char*)(node->object), len+1);
        out += len+1;
        *offset+=len+1;
    }
    for(i=0; i < MAX_VALID_CHAR; i++)
    {
        if (node->input[i])
        {
            out = obarray_mk_linear_table(node->input[i], out, offset);
        }
    }
    return out;
}
