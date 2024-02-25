#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "commands.h"
#include "filesystem.h"
#include "terminal.h"

int isStillRunning = 1;
char currentFilePath[1025];
char prompt[3] = "> ";
char pathAndPrompt[1025];

int startTerminal() {

    setCurrentFilePath( ROOTNAME );

    char* userInput = NULL;
    char** userInputList = NULL;

    initializeHashmap();

    printf( "------------------------------------------------"
            "\n\n"
            "Type 'quit' to quit\n"
            "Type 'help' for a list of commands\n" );

    while( isStillRunning ) {
        
        userInput = readline( pathAndPrompt );

        if( *userInput != '\0' ) {
            add_history( userInput );
            userInputList = stringToArrayOfStrings(userInput);
            execute( userInputList[0], userInputList );

        }

        free( userInputList );
        free( userInput );
    }
    
    freeHashmap( commandHashmap );
    return 0;
}

char** stringToArrayOfStrings( char* oldString ) {
    int arraySize = 10;
    int arrayIndex = 0;
    char** arrayOfStrings = malloc( arraySize * sizeof( char* ) );
    char* tempString = strtok( oldString, " " );

    while( tempString != NULL ) {
        arrayOfStrings[arrayIndex] = tempString;
        tempString = strtok( NULL, " " );
        ++arrayIndex;

        if( arrayIndex > arraySize ) {
            arraySize = arraySize * 2;
            if( realloc( arrayOfStrings, arraySize * sizeof( char* ) ) == NULL ) {
                printf( "realloc failed\n" );
                return NULL;
            }
        }
    }

    if(arrayIndex > arraySize) {
        arraySize = arraySize + 1;
        if(realloc(arrayOfStrings,arraySize*sizeof(char*)) == NULL) {
            printf("realloc failed\n");
            return NULL;
        }
    }

    arrayOfStrings[arrayIndex] = NULL;
    return arrayOfStrings;
}

void stopRunning() {
    isStillRunning = 0;
}

char* getCurrentFilePath() {
    char* tempPath = malloc( 1025 * sizeof( char ) );
    strcpy( tempPath, currentFilePath );
    return tempPath;
}

void setCurrentFilePath( char* path ) {
    strcpy( currentFilePath, path );
    strcpy( pathAndPrompt, currentFilePath );
    strcat( pathAndPrompt, prompt );
}