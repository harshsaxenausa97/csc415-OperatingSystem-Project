#ifndef HASHMAP_H
#define HASHMAP_H

/*********************************************************************
 * 
 * Implementation of Hashmap was taken from Kaushik Baruah
 * http://www.kaushikbaruah.com/posts/data-structure-in-c-hashmap/
 * 
 * Implementation was heavily modified to work with the assignment
 * 2 project
 * 
 *********************************************************************/

typedef void ( *commandFunction )( char** );

typedef struct node {
    char* key;
    commandFunction function;
    struct node* next;
} node;

typedef struct hashmapStruct {
    int size;
    node** nodeList;
} hashmapStruct;

hashmapStruct* newHashmap( int size );
void freeHashmap( hashmapStruct* hashmap );
void hashMapInsert( hashmapStruct* hashmap, char* key, commandFunction function );
commandFunction hashMapLookup( hashmapStruct* hashmap, char* key );

#endif /* HASHMAP_H end guard */