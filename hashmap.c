#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"

int private_hashCode( hashmapStruct* hashmap, char* key );
node** private_hashmapFind( hashmapStruct* hashmap, char* key );

/* Creates a hashmap of the key type String and the value type
 * void function( char** ). The provided size is permanent, as in
 * the hashmap will not grow to accomodate more values. */
hashmapStruct* newHashmap( int size ) {
    hashmapStruct* hashmap = malloc( sizeof( hashmapStruct ) );
    hashmap->size = size;
    hashmap->nodeList = malloc( size * sizeof( node* ) );
    for( int i = 0; i < size; i++ ) {
        hashmap->nodeList[i] = NULL;
    }
    return hashmap;
}

/* Frees all of the allocated memory used in the hashmap */
void freeHashmap( hashmapStruct* hashmap ) {
    node* currentNode;
    node* nextNode;
    for( int i = 0; i < hashmap->size; i++ ) {
        currentNode = hashmap->nodeList[i];
        while( currentNode != NULL ) {
            nextNode = currentNode->next;
            free(currentNode->key);
            free(currentNode);
            currentNode = nextNode;
        }
    }
    free(hashmap->nodeList);
    free(hashmap);
}

/* Inserts the key value pair into the hashmap */
void hashMapInsert(hashmapStruct* hashmap, char* key, commandFunction function ) {
    node** nodeList = private_hashmapFind( hashmap, key );
    if( *nodeList == NULL ) {
        node* newNode = malloc( sizeof( node ));
        char* newKey = malloc( sizeof( char ) * strlen( key ) + 1 );
        strcpy( newKey, key );
        newNode->key = newKey;
        newNode->function = function;
        newNode->next = NULL;
        *nodeList = newNode;
    }
    else {
        (*nodeList)->function = function;
    }
}

/* Finds and returns the function corresponding to the provided key.
 * Returns NULL if the key does not have a corresponding value */
commandFunction hashMapLookup( hashmapStruct* hashmap, char* key ) {
    node** nodeList = private_hashmapFind( hashmap, key );
    if( *nodeList == NULL ) {
        return NULL;
    }
    else {
        return (*nodeList)->function;
    }
}

/* Generates a hashcode based on the provided key */
int private_hashCode( hashmapStruct* hashmap, char* key ) {
    return (int)(*key) % hashmap->size;
}

/* Finds and returns the node** corresponding to the provided key */
node** private_hashmapFind( hashmapStruct* hashmap, char* key ) {

    int listPosition = private_hashCode( hashmap, key );
    node** nodeList = hashmap->nodeList + listPosition;
    
    while( *nodeList != NULL ) {
        if( strcmp( (*nodeList)->key, key ) == 0 ) {
            return nodeList;
        }
        nodeList = &((*nodeList)->next);
    }

    return nodeList;
}