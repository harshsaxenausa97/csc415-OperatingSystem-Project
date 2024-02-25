#ifndef COMMANDS_H
#define COMMANDS_H

#include "hashmap.h"

extern hashmapStruct* commandHashmap;

void execute( char* command, char** argumentList );
void initializeHashmap();

#endif /* COMMANDS_H end guard */