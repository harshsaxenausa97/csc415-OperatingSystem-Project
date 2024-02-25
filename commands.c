#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>

#include "filesystem.h"
#include "hashmap.h"
#include "terminal.h"
#include "commands.h"

void private_commandDoesntExist( char* attemptedCommand );

hashmapStruct* commandHashmap;

/* This method is based on execvp. The first parameter is the command to 
 * execute. The second parameter is a null terminated list of arguments,
 * in which by convention, the first item in the list should be the command
 * to execute. The typical form you would use is execute(arg[0], arg) */
void execute( char* command, char** argumentList ) {
    commandFunction function = hashMapLookup( commandHashmap, command );
    if( function == NULL ) {
        private_commandDoesntExist( command );
    }
    else {
        function( argumentList );
    }
}

/* Used by the execute() function to handle the situation where a user provides
 * a non-existant command */
void private_commandDoesntExist( char* attemptedCommand ) {
    printf("Command '%s' doesn't exist\n", attemptedCommand );
}

char* convertToAbsolutePath( char* path ) {
    char* argumentPath = path;
    char* currentFilePath = getCurrentFilePath();
    char* absolutePath = malloc( 1025 * sizeof( char ) );

    if( strncmp( argumentPath, ROOTNAME, 4 ) == 0 ) {
        strcpy( absolutePath, argumentPath );
    }
    else {
        strcpy( absolutePath, currentFilePath );
        strcat( absolutePath, "/" );
        strcat( absolutePath, argumentPath );
    }
    free( currentFilePath );
    return absolutePath;
}

/* MAKE SURE YOUR FUNCTION IS THE SAME FORM AS THE REST
 * It should take the form:
 * void myFunctionName( char** argumentList )
 * Also note that argumentList[0] holds the original name of the command
 * The first argument you need to parse is at argumentList[1] */

void ls( char** argumentList ) {
    char* currentFilePath = getCurrentFilePath();
    listChildrenFromPath( currentFilePath );
    free( currentFilePath );
}

void mkdir( char** argumentList ) {
    char* absolutePath;
    absolutePath = convertToAbsolutePath( argumentList[1] );

    if( getBlockLocationFromPath( absolutePath ) == 0 ) {
        unsigned long returnValue = makeDirectory( absolutePath );
        if( returnValue == 0 ) {
            printf( "Invalid path\n");
        }
    }
    else {
        printf( "Directory already exists\n");
    }
    
    free( absolutePath );
}

void cd( char** argumentList ) {

    if( argumentList[1] == NULL ) {
        printf( "Not a valid directory\n" );
        return;
    }

    char* path = argumentList[1];

    if( strcmp( path, ".." ) == 0 ) {
        path = getCurrentFilePath();
        char *pLastBackslash = strrchr( path, '/' );
        *(pLastBackslash) = '\0';
        setCurrentFilePath( path );
        free( path );
        return;
    }

    char* absolutePath;
    absolutePath = convertToAbsolutePath( path );

    if( isFile_pathVersion( absolutePath ) ) {
        printf( "Cannot cd to a file. Only directories.\n" );
        return;
    }

    if( getBlockLocationFromPath( absolutePath ) != 0 ) {
        setCurrentFilePath( absolutePath );
    }
    else {
        printf( "No such file or directory\n" );
    }

    free( absolutePath );
}

/* Remove: The argument should be a path to the file being removed.
 * If the path points to a directory then the directory along with all
 * of its contents are removed. */
void rm( char** argumentList ) {
    char* path = argumentList[1];
    char* absolutePath;
    absolutePath = convertToAbsolutePath( path );
    deleteFilePath( absolutePath );
    free( absolutePath );
}

void cp( char** argumentList ) {
    char* fromDirectory = convertToAbsolutePath( argumentList[1] );
    char* toDirectory = convertToAbsolutePath( argumentList[2] );

    copyFile( fromDirectory, toDirectory );
}

void mv( char** argumentList ) {
    char* fromDirectory = convertToAbsolutePath( argumentList[1] );
    char* toDirectory = convertToAbsolutePath( argumentList[2] );

    moveFile( fromDirectory, toDirectory );
}

void stat( char** argumentList ) {
    char* path = argumentList[1];
    char* absolutePath = convertToAbsolutePath( path );
    printMetadata( absolutePath );
    free( absolutePath );
}

void linuxtoalpha( char** argumentList ) {
    char* linuxPath = argumentList[1];
    char* alphaPath = argumentList[2];
    alphaPath = convertToAbsolutePath( alphaPath );

    copyFromLinuxToVolume( linuxPath, alphaPath );

    free( alphaPath );
}

void alphatolinux( char** argumentList ) {
    char* alphaPath = argumentList[1];
    char* linuxPath = argumentList[2];
    alphaPath = convertToAbsolutePath( alphaPath );

    copyFromVolumeToLinux( alphaPath, linuxPath );

    free( alphaPath );
}


void textedit( char** argumentList ) {
    if( argumentList[1] == NULL ) {
        printf( "Textedit needs a name\n" );
        return;
    }
    char* path = argumentList[1];
    path = convertToAbsolutePath( path );
    char* content = getContent( path );
    

    if( content == NULL ) {
        content = calloc( 1, 1);
        printf( "Creating file\n");
        if( makeFile( path ) == 0 ) {
            printf( "Couldn't make file\n" );
            return;
        }
    }
    
    unsigned long parentLocation = getBlockLocationFromPath( path );

    int readlineRunning;
    char* newContent;

    void handler( char* line ) {
        newContent = line;
        readlineRunning = 0;
        rl_callback_handler_remove();
    }

    printf("\nStart typing. Press 'enter' to finish.\n");
    rl_callback_handler_install( "", &handler );
    rl_insert_text( content );
    rl_redisplay();
    readlineRunning = 1;
    while( readlineRunning ) {
        rl_callback_read_char();
    }
    printf("\n");
    
    int contentActualSize = strlen( newContent ) + 1;
    int blockSize = ( contentActualSize / mainSystemInfo->lbaSize ) + 1;
    int bufferMallocSize = blockSize * mainSystemInfo->lbaSize;

    void* contentBuffer = calloc( bufferMallocSize, 1 );
    memcpy( contentBuffer, (void*)newContent, contentActualSize );

    writeFileData( parentLocation, blockSize, contentBuffer, contentActualSize );

    if( content != NULL ) {
        free( content );
    }
    if( newContent != NULL ) {
        free( newContent );
    }
    if( contentBuffer != NULL ) {
        free( contentBuffer );
    }
}

/* Quit: Exits the terminal. Simply calls the function stopRunning()
 * from terminal.c */
void quit( char** argumentList ) {
    stopRunning();
}

void help( char** argumentList ) {
    printf(
            "\n"
            "quit\n" 
            "    Leaves the terminal. The very first thing any engineer\n"
            "    needs to install... the breaks.\n\n"
            "ls\n"
            "    Lists the files in the current directory.\n\n"
            "mkdir DIRECTORY\n"
            "    Creates a directory.\n\n"
            "cd DIRECTORY\n"
            "    Changes directories. Also supports '..' to go up a directory\n\n"
            "rm FILE\n"
            "    Removes a directory.\n\n"
            "cp SOURCE DEST\n"
            "    Copies directory.\n\n"
            "mv SOURCE DEST\n"
            "    Moves directories.\n\n"
            "stat FILE\n"
            "    Display file metadata.\n\n"
            "linuxtoalpha LINUXFILE ALPHAFILE\n"
            "    Copies a file from the linux machine to the team alpha system\n\n"
            "alphatolinux ALPHAFILE LINUXFILE\n"
            "    Well what do you know... this guy copies a file from the\n"
            "    alpha system to the linux machine.\n\n"
            "textedit FILE\n"
            "    Allows you to edit the contents of a file. If the file\n"
            "    doesn't exist then one is created and you will be"
            "    prompted for a name.\n\n"
            "help\n"
            "    I think its funny that most help pages list what help does.\n"
            "    If you DON'T know what it does, then this help page isn't\n"
            "    going to help you.\n\n"
            );
}

/* Adds all of the commands to the hashmap. When adding commands, please
 * use the format:
 * hashMapInsert( commandHashmap, "yourCommand", &yourCommand)
 * When implementing the actual command, please add it to the section
 * above with the rest of the commands */
void initializeHashmap() {
    commandHashmap = newHashmap(10);
    hashMapInsert( commandHashmap, "ls", &ls );
    hashMapInsert( commandHashmap, "mkdir", &mkdir );
    hashMapInsert( commandHashmap, "cd", &cd );
    hashMapInsert( commandHashmap, "rm", &rm );
    hashMapInsert( commandHashmap, "cp", &cp );
    hashMapInsert( commandHashmap, "mv", &mv );
    hashMapInsert( commandHashmap, "stat", &stat );
    hashMapInsert( commandHashmap, "linuxtoalpha", &linuxtoalpha );
    hashMapInsert( commandHashmap, "alphatolinux", &alphatolinux );
    hashMapInsert( commandHashmap, "textedit", &textedit );
    hashMapInsert( commandHashmap, "quit", &quit );
    hashMapInsert( commandHashmap, "help", &help );
}
