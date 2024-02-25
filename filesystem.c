#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "fsLow.h"
#include "systemstructs.h"
#include "filesystem.h"

/* Opens the volume through fslow and initializes the volume with the
 * main system info. Then creates the root directory and sets the rest of the
 * volume to free. */
int startFileSystem( char* volumeName, unsigned long volumeSize, unsigned long blockSize ) {

    //Start the system from fsLow
    startPartitionSystem( volumeName, &volumeSize, &blockSize );

    //Initialize the sizes of all the structs to be used
    system_lbaSize = ( sizeof( sysInfo ) / blockSize ) + 1;
    system_mallocSize = system_lbaSize * blockSize;

    free_lbaSize = ( sizeof( freeSpace ) / blockSize ) + 1;
    free_mallocSize = free_lbaSize * blockSize;

    file_lbaSize = ( sizeof( file ) / blockSize ) + 1;
    file_mallocSize = file_lbaSize * blockSize;

    initializeSystemInfo( volumeName, volumeSize, blockSize );

    return 0;
}

/* Initializes the main system info. Either takes the stored info if it exists
 * or creates a new system.
 * Returns 0 if system already exists.
 * Returns 1 if new system was created. */
int initializeSystemInfo( char* volumeName, unsigned long volumeSize, unsigned long blockSize ) {
    mainSystemInfo = calloc( system_mallocSize, 1 );
    LBAread( (void*)mainSystemInfo, system_lbaSize, 0 );
    
    if( isValidSystemInfo( mainSystemInfo ) ) {
        return 0;
    }
    else {
        createNewSystem( volumeName, volumeSize, blockSize );
        return 1;
    }
}

/* Creates and initializes a new block for the main system info.
 * Creates and initializes the free head block.
 * Creates and initializes the root directory.
 * Returns 0 if successful. */
int createNewSystem( char* volumeName, unsigned long volumeSize, unsigned long blockSize ) {

    //Starting Info
    strcpy( mainSystemInfo->volumeName, volumeName );
    mainSystemInfo->volumeSize = volumeSize;
    mainSystemInfo->lbaSize = blockSize;
    mainSystemInfo->signature1 = SYSTEMSIGNATURE1;
    mainSystemInfo->signature2 = SYSTEMSIGNATURE2;

    //If mainSystemInfo uses 2 blocks, then freeHeadBeginningLocation should
    //start at block 2. i.e. mainSystemInfo uses block 0 and block 1.
    unsigned int freeHeadBeginningLocation = system_lbaSize;
    initializeFreeSpace( freeHeadBeginningLocation );
    mainSystemInfo->freeHeadBlock = freeHeadBeginningLocation;

    //Create the root directory
    unsigned long rootLocation = makeBlank();
    setFileIdentifierType( rootLocation, "dr" );
    setDefaultMetadata( rootLocation );
    setFileName( rootLocation, ROOTNAME );
    mainSystemInfo->rootLocation = rootLocation;

    //Write mainSystemInfo to volume at location 0
    //0 is hard-coded because... well, the main system info should be
    //the very first thing right?
    LBAwrite( (void*)mainSystemInfo, system_lbaSize, 0 );

    return 0;
}

/* Creates the first freeSpace block. This function is used when the system
 * is creating the volume for the first time. Here we create a freeSpace struct
 * where we basically set the free space count to the size of the volume (minus
 * a block because of the main system info). */
int initializeFreeSpace( const unsigned long beginLocation ) {

    //Create and initialize the freeSpace
    freeSpace* beginningFreeSpace = calloc( free_mallocSize , 1 );
    beginningFreeSpace->count =
        mainSystemInfo->volumeSize / mainSystemInfo->lbaSize - beginLocation;
    beginningFreeSpace->next = beginLocation;
    beginningFreeSpace->prev = beginLocation;
    beginningFreeSpace->signature1 = FREESIGNATURE1;
    beginningFreeSpace->signature2 = FREESIGNATURE2;

    //Write freeSpace to volume
    LBAwrite( (void*)beginningFreeSpace, free_lbaSize, beginLocation );

    free(beginningFreeSpace);

    return 0;
}


int copyFromVolumeToLinux( char* ourPath, char* linuxPath ) {
// TODO: put comment here explaining the function

    unsigned long fileLocation = getBlockLocationFromPath( ourPath );
    file* ourFile = calloc( file_mallocSize, 1 );
    LBAread( (void*)ourFile, file_lbaSize, fileLocation );
    
    if( !isValidFile( ourFile ) || !isFile( ourFile ) || !isWritable( ourFile ) ) {
        printf( "Not a valid file on the alpha volume\n");
        free( ourFile );
        return -1;
    }
    
    unsigned long contentLocation = ourFile->startingBlock;
    unsigned long fileSize = ourFile->fileSize;
    int contentBlockAmount = ( fileSize / mainSystemInfo->lbaSize ) + 1;
    int bufferMallocSize = contentBlockAmount * mainSystemInfo->lbaSize;
    void* buffer = calloc( bufferMallocSize, 1 );

    LBAread( buffer, contentBlockAmount, contentLocation );

    FILE* linuxFile = fopen( linuxPath, "w" );
    fwrite( buffer, fileSize, 1, linuxFile );
    fclose( linuxFile );

    free( ourFile );
    free( buffer );

    return 0;
}


int copyFromLinuxToVolume( char* linuxFileName, char* volumeFileName ) {

    FILE* linuxFile = fopen( linuxFileName, "r" );

    // get file size
    fseek( linuxFile, 0, SEEK_END );
    int linuxFileSize = ftell( linuxFile );
    rewind( linuxFile );


    unsigned int blockSize = mainSystemInfo->lbaSize;
    // integer divsion that takes ceiling
    unsigned int numberOfBlocks = ( linuxFileSize + blockSize - 1 ) / blockSize;
    int bufferMallocSize = numberOfBlocks * mainSystemInfo->lbaSize;

    void* buffer = calloc( bufferMallocSize, 1 );
    fread( buffer, linuxFileSize, 1, linuxFile );

    unsigned long volumeBlockLocation = addFile( volumeFileName );
    writeFileData( volumeBlockLocation, numberOfBlocks, buffer, linuxFileSize );

    free( buffer );
    fclose( linuxFile );

    return 0;
}


/* Takes two file paths and moves the file from the first path to the second.
 * reads file that will be moved into a temp buffer, then writes 
 * the buffer to the new location. Assumes both are absolute paths. */
unsigned long copyFile( char* moveFrom, char* moveTo ) {

    if( strcmp( moveFrom, moveTo ) == 0 ) {
        printf( "new filename must be different than old filename\n" );
        return 0;
    }

    if( getBlockLocationFromPath( moveFrom ) == 0 ) {
        printf( "Source file doesn't exist\n");
    }

    if( getBlockLocationFromPath( moveTo ) != 0 ) {
        printf( "File name already exists\n");
        return 0;
    }

    unsigned long toBlockLocation;
    unsigned long fromBlockLocation = getBlockLocationFromPath( moveFrom );

    toBlockLocation = addFile( moveTo );

    if( !fromBlockLocation || !toBlockLocation ) {
        printf( "Invalid file path\n" );
        return 0;
    }

    // save file being moved to a temporary buffer
    void* oldFileBuffer = calloc( file_mallocSize, 1 );
    LBAread( oldFileBuffer, file_lbaSize, fromBlockLocation);
    file* oldFile = (file*)oldFileBuffer;

    // directories do not contain any data
    if( oldFile->identifierType == IDENTIFIER_FILE ) {
        // save file data being moved into a temporary buffer
        setFileIdentifierType( toBlockLocation, "fl" );
        int contentBlockAmount = ( oldFile->fileSize / mainSystemInfo->lbaSize ) + 1;
        int bufferMallocSize = contentBlockAmount * mainSystemInfo->lbaSize;
        void* tempDataBuffer = calloc( bufferMallocSize, 1 );
        LBAread( tempDataBuffer, contentBlockAmount, oldFile->startingBlock );
        writeFileData( toBlockLocation, contentBlockAmount, tempDataBuffer, oldFile->fileSize );
        free( tempDataBuffer );
    } else if( oldFile->identifierType == IDENTIFIER_DIRECTORY ) {
        setFileIdentifierType( toBlockLocation, "dr" );
        for( int i = 0; i < NUMBER_OF_CHILDREN; ++i ) {
            if( oldFile->children[i] > 1 ) {
                void* childFileBuffer = calloc( file_mallocSize, 1 );
                LBAread( childFileBuffer, file_lbaSize, oldFile->children[i] );
                file* childFile = (file*)childFileBuffer;

                // appends child file name to given file path 
                char* childMoveFrom = filePathConcat( moveFrom, childFile->fileName );
                char* childMoveTo = filePathConcat( moveTo, childFile->fileName );
                unsigned long childBlockLocation = copyFile( childMoveFrom, childMoveTo );

                void* newFileBuffer = calloc( file_mallocSize, 1 );
                LBAread( newFileBuffer, file_lbaSize, toBlockLocation );
                file* newFile = (file*)newFileBuffer;
                newFile->children[i] = childBlockLocation;
                LBAwrite( newFileBuffer, file_lbaSize, toBlockLocation );

                free( childMoveFrom );
                free( childMoveTo );
                free( childFileBuffer );
                free( newFileBuffer );
            }
        }
    }
    
    free( oldFileBuffer );

    return toBlockLocation;
}


/* Takes two file paths and moves the file from the first path to the second.
 * calls the copy function and then deletes the original file */
int moveFile( char* moveFrom, char* moveTo ) {

    if( copyFile( moveFrom, moveTo ) == 0 ) {
        return -1;
    }

    unsigned long fromBlockLocation = getBlockLocationFromPath( moveFrom );
    char* parentPath = getParentPath( moveFrom );
    unsigned long parentLocation = getBlockLocationFromPath( parentPath );
    recursiveDelete( fromBlockLocation );
    removeChild( parentLocation, fromBlockLocation );

    free( parentPath );

    return 0;
}


/* Parses a filepath then returns the block location of the file
 * at the end of the filepath. Assumes an absolute path.
 * Will return 0 if no match is found. */
unsigned long getBlockLocationFromPath( char* filePath ) {

    filePath = getCopyOfString( filePath );

    // starts at root directory
    unsigned long blockLocation = mainSystemInfo->rootLocation;

    // if no forward slashes present so return root dir
    char *pLastBackslash = strrchr(filePath, '/');
    if( !pLastBackslash || !*(pLastBackslash + 1) ) {
        return blockLocation;
    }

    // remove newline from end of user inputted string
    filePath[strcspn( filePath, "\n" )] = 0;
    char* directoryName = strtok( filePath, "/" );
    if( strcmp( directoryName, ROOTNAME ) != 0 ) {
        return 0;
    }
    directoryName = strtok( NULL, "/" );

    while( directoryName != NULL ) {
        // go to child directory
        blockLocation = getBlockLocationFromName( blockLocation, directoryName );
        if( blockLocation == 0 ) {
            break;
        }
        directoryName = strtok( NULL, "/" );
    }

    free( filePath );
    return blockLocation;
}


/* Searches every child file of a specifed parent file using the file name,
 * and returns the child file's block location if a match is found.
 * Will return 0 if no match is found */
unsigned long getBlockLocationFromName( unsigned long blockLocation, char* fileName ) {

    unsigned long childBlockLocation = 0;

    void* parentBuffer = calloc( file_mallocSize , 1 );
    void* childBuffer = calloc( file_mallocSize , 1 );

    LBAread( parentBuffer, file_lbaSize, blockLocation);
    file* parentFile = (file*)parentBuffer;

    // read in each of the parent file's children
    for( int i = 0; i < NUMBER_OF_CHILDREN; ++i ) {
        LBAread( childBuffer, file_lbaSize, parentFile->children[i] );
        file* childFile = (file*)childBuffer;

        if( strcmp( childFile->fileName, fileName ) == 0 ) {
            childBlockLocation = parentFile->children[i];
            break;
        }
    }

    free( parentBuffer );
    free( childBuffer );

    return childBlockLocation;
}

char* getParentPath( char* filePath ) {
    filePath = getCopyOfString( filePath );
    char *pLastBackslash = strrchr( filePath, '/' );
    *(pLastBackslash) = '\0';
    return filePath;
}

char* filePathConcat( const char *s1, const char *s2 ) {
    char *result = malloc( strlen(s1) + strlen(s2) + 2 );
    strcpy( result, s1 );
    strcat( result, "/" );
    strcat( result, s2 );
    return result;
}


unsigned long addFile( char* filePath ) {
// creates a file at the location of the specified absolute path

    filePath = getCopyOfString( filePath );

    char* newFileName;
    unsigned long newFileLocation;
    unsigned long toDirectoryLocation;

    // goes to directory where file will be created by "splitting" the string
    // on the last forward slash
    char *pLastBackslash = strrchr(filePath, '/');
    if( pLastBackslash && *(pLastBackslash + 1) ) {
        newFileName = pLastBackslash + 1;
        *(pLastBackslash) = '\0';
        toDirectoryLocation = getBlockLocationFromPath( filePath );
        if( toDirectoryLocation == 0 ) {
            free( filePath );
            return 0;
        }
    } else {
        // no forward slashes present so return root dir
        return mainSystemInfo->rootLocation;
    }

    newFileLocation = makeBlank();
    setDefaultMetadata( newFileLocation );
    setFileName( newFileLocation, newFileName );
    addChild( toDirectoryLocation, newFileLocation );
    
    free( filePath );
    return newFileLocation;
}


unsigned long makeBlank() {
    unsigned long newFileLocation;
    file* newFile = calloc( file_mallocSize, 1 );
    newFile->signature1 = FILESIGNATURE1;
    newFile->signature2 = FILESIGNATURE2;
    
    newFileLocation = getFreeBlocks( file_lbaSize );
    LBAwrite( (void*)newFile, file_lbaSize, newFileLocation );

    free( newFile );

    return newFileLocation;
}


unsigned long makeDirectory( char* filePath ) {
    unsigned long directoryLocation;
    directoryLocation = addFile( filePath );
    if( directoryLocation == 0 ) {
        return 0;
    }
    setFileIdentifierType( directoryLocation, "dr" );
    return directoryLocation;
}


unsigned long makeFile( char* filePath ) {
    unsigned long fileLocation;
    fileLocation = addFile( filePath );
    if( fileLocation == 0 ) {
        return 0;
    }
    setFileIdentifierType( fileLocation, "fl" );
    return fileLocation;
}


int addChild( unsigned long parentLocation, unsigned long childLocation ) {
    file* parent = calloc( file_mallocSize, 1 );
    LBAread( (void*)parent, file_lbaSize, parentLocation );

    int currentChild;
    for( currentChild = 0; currentChild < NUMBER_OF_CHILDREN; currentChild++ ) {
        if( parent->children[currentChild] == 0 ) {
            parent->children[currentChild] = childLocation;
            break;
        }
    }

    LBAwrite( (void*)parent, file_lbaSize, parentLocation );

    free( parent );

    int numberOfChildren = currentChild + 1;
    
    if( numberOfChildren <= NUMBER_OF_CHILDREN ) {
        return numberOfChildren;
    }
    else {
        return 0;
    }
}


int removeChild( unsigned long parentLocation, unsigned long childLocation ) {
    file* parent = calloc( file_mallocSize, 1 );
    LBAread( (void*)parent, file_lbaSize, parentLocation );

    int currentChild;
    for( currentChild = 0; currentChild < NUMBER_OF_CHILDREN; currentChild++ ) {
        if( parent->children[currentChild] == childLocation ) {
            break;
        }
    }

    if( currentChild == NUMBER_OF_CHILDREN ) {
        return -1;
    }

    while( parent->children[currentChild] != 0 ) {
        parent->children[currentChild] = parent->children[currentChild+1];
        currentChild++;
    }

    LBAwrite( (void*)parent, file_lbaSize, parentLocation );

    free( parent );

    return currentChild;
}


void listChildren( unsigned long blockLocation ) {
    file* parentDirectory = calloc( file_mallocSize, 1 );
    file* childFile = calloc( file_mallocSize, 1 );
    LBAread( (void*)parentDirectory, file_lbaSize, blockLocation );

    for( int i = 0; i < NUMBER_OF_CHILDREN; i++ ) {
        if( parentDirectory->children[i] != 0 ) {
            LBAread( (void*)childFile, file_lbaSize, parentDirectory->children[i]);
            printf("%s\n", childFile->fileName );
        }
        else {
            break;
        }
    }

    free( parentDirectory );
    free( childFile );
}


void listChildrenFromPath( char* absolutePath ) {
    unsigned long blockLocation;
    blockLocation = getBlockLocationFromPath( absolutePath );
    listChildren( blockLocation );
}


int setDefaultMetadata( unsigned long blockLocation ) {
// convenience function to call all default file setters in one place

    setFileCreatedAt( blockLocation );
    setFileId( blockLocation );
    setFileModifiedAt( blockLocation );
    setFilePermissions( blockLocation, "write" );
    return 0;
}


int setFileCreatedAt( unsigned long blockLocation ) {
// sets the created date of the file to the current time
// this should only be called once during file creation

    void* buffer = calloc( file_mallocSize , 1 );
    LBAread( buffer, file_lbaSize, blockLocation);
    file* currentFile = (file*)buffer;

    // currrent time
    currentFile->created = time( NULL );

    LBAwrite( buffer, file_lbaSize, blockLocation );
    free( buffer );
    return 0;
}


int setFileModifiedAt( unsigned long blockLocation ) {
// sets the modified date of the file to the current time

    void* buffer = calloc( file_mallocSize , 1 );
    LBAread( buffer, file_lbaSize, blockLocation);
    file* currentFile = (file*)buffer;

    // current time
    currentFile->modified = time( NULL );
    
    LBAwrite( buffer, file_lbaSize, blockLocation );
    free( buffer );

    return 0;
}


int setFileId( unsigned long blockLocation ) {
// uses a random number generator to set file id
// this should only be called once during file creation

    void* buffer = malloc( file_mallocSize );
    LBAread( buffer, file_lbaSize, blockLocation );
    file* currentFile = (file*)buffer;

    srand( (unsigned) time(NULL) );

    for ( int i = 0; i < ID_LENGTH-1; ++i ) {
        currentFile->id[i] = rand() % 10 + '0';
    }

    LBAwrite( buffer, file_lbaSize, blockLocation );
    free( buffer );

    return 0;
}


int setFileName( unsigned long blockLocation, char* fileName ) {
// clears the current name and write the new name

    void* buffer = calloc( file_mallocSize , 1 );
    LBAread( buffer, file_lbaSize, blockLocation);
    file* currentFile = (file*)buffer;

    // clear current name
    memset( currentFile->fileName, '\0', sizeof(currentFile->fileName) );
    strcpy( currentFile->fileName, fileName );

    LBAwrite( buffer, file_lbaSize, blockLocation );
    free( buffer );

    return 0;
}


unsigned int getFilePermissons( unsigned long blockLocation ) {
// takes in block location and returns permissions of the file

    void* buffer = malloc( file_mallocSize );
    LBAread( buffer, file_lbaSize, blockLocation );
    file* currentFile = (file*)buffer;

    unsigned int permissions = currentFile->permissions;
    free( buffer );

    return permissions;
}


int setFilePermissions( unsigned long blockLocation, char* newPermissionStr ) {
// takes in a permissions string ("read", "write", "execute") and modifies permissions accordingly

    unsigned int newPermissionInt;

    void* buffer = calloc( file_mallocSize , 1 );
    LBAread( buffer, file_lbaSize, blockLocation );
    file* currentFile = (file*)buffer;

    if( strcmp( "read", newPermissionStr ) == 0 ) {
        newPermissionInt = PERMISSION_READ;
    } else if( strcmp( "write", newPermissionStr ) == 0 ) {
        newPermissionInt = PERMISSION_WRITE;
    } else if( strcmp( "execute", newPermissionStr ) == 0 ) {
        newPermissionInt = PERMISSION_EXECUTE;
    } else {
        printf("Invalid Permissions given\n");
        return -1;
    }

    currentFile->permissions = newPermissionInt;
    
    LBAwrite( buffer, file_lbaSize, blockLocation );
    free( buffer );

    return 0;
}


int setStartingBlock( unsigned long headerBlockLocation, unsigned long dataBlockLocation ) {
// takes in block location and returns starting block location of file data

    void* buffer = malloc( file_mallocSize );
    LBAread( buffer, file_lbaSize, headerBlockLocation );
    file* currentFile = (file*)buffer;

    if( currentFile->startingBlock != 0 ) {
        delete( currentFile->startingBlock, currentFile->fileSize );
    }

    currentFile->startingBlock = dataBlockLocation;
    LBAwrite( buffer, file_lbaSize, headerBlockLocation );
    free( buffer );

    return 0;
}


int setCount( unsigned long blockLocation, unsigned int count ) {
// takes in block location and returns starting block location of file data

    void* buffer = malloc( file_mallocSize );
    LBAread( buffer, file_lbaSize, blockLocation );
    file* currentFile = (file*)buffer;

    currentFile->fileSize = count;
    LBAwrite( buffer, file_lbaSize, blockLocation );
    free( buffer );

    return 0;
}


int setFileIdentifierType( unsigned long blockLocation, char* identifierTypeStr ) {
// takes in a indentifier type ("dr", "fl", "-lnk") and sets type accordingly
// this should only be called once during file creation

    unsigned int identifierTypeInt;

    void* buffer = malloc( file_mallocSize );
    LBAread( buffer, file_lbaSize, blockLocation );
    file* currentFile = (file*)buffer;

    if( strcmp( "dr", identifierTypeStr ) == 0 ) {
        identifierTypeInt = IDENTIFIER_DIRECTORY;
    } else if( strcmp( "fl", identifierTypeStr ) == 0 ) {
        identifierTypeInt = IDENTIFIER_FILE;
    } else if( strcmp( "drlnk", identifierTypeStr ) == 0 ) {
        identifierTypeInt = IDENTIFIER_FILE_LINK;
    } else if( strcmp( "fllnk", identifierTypeStr ) == 0 ) {
        identifierTypeInt = IDENTIFIER_DIRECTORY_LINK;
    } else {
        printf("Invalid Permissions given\n");
        return -1;
    }

    currentFile->identifierType = identifierTypeInt;
    
    LBAwrite( buffer, file_lbaSize, blockLocation );
    free( buffer );

    return 0;
}


int writeFileData( unsigned long headerBlockLocation, int numberOfBlocks, void* fileBuffer, int fileSize ) {
// modifies the content of a file at the block location passed in

    if( getFilePermissons( headerBlockLocation ) < PERMISSION_WRITE) {
        printf( "ERROR: FILE IS READ ONLY\n" );
        return -1;
    }

    unsigned long dataBlockLocation = getFreeBlocks( numberOfBlocks );

    LBAwrite( fileBuffer, numberOfBlocks, dataBlockLocation );
    setStartingBlock( headerBlockLocation, dataBlockLocation );
    setCount( headerBlockLocation, fileSize );

    return 0;
}


int recursiveDelete( unsigned long blockLocation ) {
// TODO: put comment here explaining the function
    
    //Create a space for then read the file at blockLocation
    file* currentFile = calloc( file_mallocSize , 1 );
    LBAread( (void*)currentFile, file_lbaSize, blockLocation );
    
    //Check to see if the file is valid by checking the signature
    if( !isValidFile( currentFile ) ) {
        printf( "WARNING SYSTEM ATTEMPTED TO REFERENCE A BLOCK THAT IS\n"
                "NOT A FILE\n" );
        return -1;
    }

    //If the block location pointed to a directory, recursively
    //delete the children then delete the fileStruct
    if( isDirectory( currentFile ) && isWritable( currentFile ) ) {
        for( int i = 0; i < NUMBER_OF_CHILDREN; i++ ) {
            if( currentFile->children[i] > 1) {
                recursiveDelete( currentFile->children[i] );
            }
        }
        delete( blockLocation, file_lbaSize );
        return 0;
    }

    //If the block location pointed to a file, delete the
    //contents first then delete the fileStruct
    if( isFile( currentFile ) && isWritable( currentFile ) ) {
        int contentBlockAmount = currentFile->fileSize / mainSystemInfo->lbaSize + 1;
        delete( currentFile->startingBlock, contentBlockAmount );
        delete( blockLocation, file_lbaSize );
    }

    //Free the malloc'ed block
    free( currentFile );

    return 0;
}

/* A function that sets a specific location to free and updates the
 * free list */
int delete( unsigned long blockLocation, unsigned int amountToFree ) {

    int numberOfAllocs = 0;

    //Make sure someone doesn't try to delete blocks 0 or 1
    if( blockLocation <= 1 ) {
        printf( "CANNOT FREE BLOCKS 0 OR 1 AS THESE ARE THE SYSTEM\n"
                "INFO AND ROOT DIRECTORIES\n");
        return -1;
    }

    //Malloc the space for the new, head, and last blocks
    freeSpace* newFreeBlock = calloc( free_mallocSize , 1 );
    numberOfAllocs++;
    freeSpace* freeHeadBlock = calloc( free_mallocSize , 1 );
    numberOfAllocs++;
    freeSpace* lastFreeBlock;

    //We get the lastFreeBlock by first getting the head and getting
    //the previous node, since the doubly linked list is circular
    // ... <--> secondToLast <--> Last <--> HEAD <--> Second <--> Third <--> ...
    LBAread( (void*)freeHeadBlock, free_lbaSize, mainSystemInfo->freeHeadBlock );
    if( freeHeadBlock->prev == mainSystemInfo->freeHeadBlock ) {
        lastFreeBlock = freeHeadBlock;
    }
    else {
        lastFreeBlock = calloc( free_mallocSize , 1 );
        numberOfAllocs++;
        LBAread( (void*)lastFreeBlock, free_lbaSize, freeHeadBlock->prev );
    }

    //Links the new free block to the head block and last block
    //This new free block becomes the last block
    newFreeBlock->count = amountToFree;
    newFreeBlock->next = lastFreeBlock->next;
    newFreeBlock->prev = freeHeadBlock->prev;
    
    //Links the head block and the last block to the new block
    freeHeadBlock->prev = blockLocation;
    lastFreeBlock->next = blockLocation;

    //Write all the blocks now that they are set
    LBAwrite( (void*)newFreeBlock, free_lbaSize, blockLocation );
    LBAwrite( (void*)freeHeadBlock, free_lbaSize, newFreeBlock->next );
    LBAwrite( (void*)lastFreeBlock, free_lbaSize, newFreeBlock->prev );

    //Free all the malloc'ed blocks
    if( numberOfAllocs == 1 ) {
        free( newFreeBlock );
    }
    else if( numberOfAllocs == 2 ) {
        free( newFreeBlock );
        free( freeHeadBlock );
    }
    else if( numberOfAllocs == 3 ) {
        free( newFreeBlock );
        free( freeHeadBlock );
        free( lastFreeBlock );
    }

    return 0;
}


int deleteFilePath( char* filePath ) {
    unsigned long blockLocation = getBlockLocationFromPath( filePath );
    int returnValue = recursiveDelete( blockLocation );

    if( returnValue == -1 ) {
        return -1;
    }

    char* parentPath = getParentPath( filePath );
    unsigned long parentLocation = getBlockLocationFromPath( parentPath );
    free( parentPath );
    removeChild( parentLocation, blockLocation );
    return 0;
}


unsigned long getFreeBlocks( unsigned int numberOfFreeBlocksWanted ) {
    unsigned long startBlock = mainSystemInfo->freeHeadBlock;
    int numberOfAllocs = 0;
    
    freeSpace* currentFreeBlock = calloc( free_mallocSize, 1 );
    numberOfAllocs++;
    freeSpace* previousFreeBlock;
    freeSpace* nextFreeBlock;
    LBAread( (void*)currentFreeBlock, free_lbaSize, mainSystemInfo->freeHeadBlock );
    
    /* Iterate through the list and find a block big enough for the desired
     * size */
    while( currentFreeBlock->count < numberOfFreeBlocksWanted &&
           currentFreeBlock->next != mainSystemInfo->freeHeadBlock ) {
               startBlock = currentFreeBlock->next;
               LBAread( (void*)currentFreeBlock, free_lbaSize, currentFreeBlock->next );
    }

    /* I was having problems when the nodes in the list refer to the same node
     * as in current->next and current->prev are the same node. As it would be
     * in a two item circular list. If you do things incorrectly you would have
     * multiple copies of the same node. If you edit the multiple copies you
     * are NOT editing the same node... you are editing a copy of the node.
     * You would then only end up writing half of the changes because copy 1
     * got a change and copy 2 got a change but you are writing copy 2 last so
     * the node only gets the changes in copy 2. */
    if( currentFreeBlock->next == mainSystemInfo->freeHeadBlock ) {
        previousFreeBlock = currentFreeBlock;
        nextFreeBlock = currentFreeBlock;
    }
    else if ( currentFreeBlock->next == currentFreeBlock->prev ) {
        previousFreeBlock = calloc( free_mallocSize, 1 );
        numberOfAllocs++;
        LBAread( (void*)previousFreeBlock, free_lbaSize, currentFreeBlock->prev );
        nextFreeBlock = previousFreeBlock;
    }
    else {
        previousFreeBlock = calloc( free_mallocSize, 1 );
        numberOfAllocs++;
        LBAread( (void*)previousFreeBlock, free_lbaSize, currentFreeBlock->prev );
        nextFreeBlock = calloc( free_mallocSize, 1 );
        numberOfAllocs++;
        LBAread( (void*)nextFreeBlock, free_lbaSize, currentFreeBlock->next );
    }

    /* Sets all the next and previous node values */
    if( currentFreeBlock->count >= numberOfFreeBlocksWanted ) {

        currentFreeBlock->count = currentFreeBlock->count - numberOfFreeBlocksWanted;
        
        if( currentFreeBlock->count == 0 ) {
            previousFreeBlock->next = currentFreeBlock->next;
            nextFreeBlock->prev = currentFreeBlock->prev;
            LBAwrite( (void*)previousFreeBlock, free_lbaSize, currentFreeBlock->prev );
            LBAwrite( (void*)nextFreeBlock, free_lbaSize, currentFreeBlock->next );
        }
        else {
            previousFreeBlock->next = previousFreeBlock->next + numberOfFreeBlocksWanted;
            nextFreeBlock->prev = previousFreeBlock->next;
            LBAwrite( (void*)previousFreeBlock, free_lbaSize, currentFreeBlock->prev );
            LBAwrite( (void*)nextFreeBlock, free_lbaSize, currentFreeBlock->next );
            LBAwrite( (void*)currentFreeBlock, free_lbaSize, previousFreeBlock->next );
        }

        if( startBlock == mainSystemInfo->freeHeadBlock ) {
            mainSystemInfo->freeHeadBlock = previousFreeBlock->next;
        }

    }
    else {
        startBlock = 0;
    }

    if( numberOfAllocs == 1 ) {
        free( currentFreeBlock );
    }
    else if( numberOfAllocs == 2 ) {
        free( currentFreeBlock );
        free( previousFreeBlock );
    }
    else if( numberOfAllocs == 3 ) {
        free( currentFreeBlock );
        free( previousFreeBlock );
        free( nextFreeBlock );
    }

    return startBlock;
}


int isFile( file* fileToCheck ) {
    return fileToCheck->identifierType == IDENTIFIER_FILE;
}

int isFile_pathVersion( char* path ) {
    unsigned long blockLocation = getBlockLocationFromPath( path );
    file* fileToCheck = calloc( file_mallocSize, 1 );
    LBAread( (void*)fileToCheck, file_lbaSize, blockLocation );
    return isFile( fileToCheck );
}


int isDirectory( file* fileToCheck ) {
    return fileToCheck->identifierType == IDENTIFIER_DIRECTORY;
}


int isFileLink( file* fileToCheck ) {
    return fileToCheck->identifierType == IDENTIFIER_FILE_LINK;
}


int isDirectoryLink( file* fileToCheck ) {
    return fileToCheck->identifierType == IDENTIFIER_DIRECTORY_LINK;
}


int isReadable( file* fileToCheck ) {
    return ( fileToCheck->permissions >= PERMISSION_READ );
}


int isWritable( file* fileToCheck ) {
    return ( fileToCheck->permissions >= PERMISSION_WRITE );
}


int isExecutable( file* fileToCheck ) {
    return ( fileToCheck->permissions >= PERMISSION_EXECUTE );
}


int isValidFile( file* fileToCheck ) {
    return ( fileToCheck->signature1 == FILESIGNATURE1 ) &&
           ( fileToCheck->signature2 == FILESIGNATURE2 );
}


int isValidSystemInfo( sysInfo* systemInfoToCheck ) {
    return ( systemInfoToCheck->signature1 == SYSTEMSIGNATURE1 ) &&
           ( systemInfoToCheck->signature2 == SYSTEMSIGNATURE2 );
}


int isValidFreeSpace( freeSpace* freeSpaceToCheck ) {
    return ( freeSpaceToCheck->signature1 == FREESIGNATURE1 ) &&
           ( freeSpaceToCheck->signature2 == FREESIGNATURE2 );
}


char* getCopyOfString( char* string ) {
    char* stringCopy = calloc( strlen( string ) + 1 , sizeof( char ) );
    strcpy( stringCopy, string );
    return stringCopy;
}


int closeFileSystem() {
    LBAwrite( (void*)mainSystemInfo, system_lbaSize, 0 );
    free( mainSystemInfo );
    closePartitionSystem();
    return 0;
}

int printMetadata( char* path ) {
    unsigned long blockLocation = getBlockLocationFromPath( path );
    file* fileToPrint = calloc( file_mallocSize, 1 );
    LBAread( fileToPrint, file_lbaSize, blockLocation );
    if( !isValidFile( fileToPrint ) ) {
        printf( "Not a valid file\n" );
        free( fileToPrint );
        return -1;
    }
    
    char* identifierType;
    switch( fileToPrint->identifierType ) {
        case IDENTIFIER_FILE: identifierType = "File"; break;
        case IDENTIFIER_DIRECTORY: identifierType = "Directory"; break;
        case IDENTIFIER_FILE_LINK: identifierType = "File Link"; break;
        case IDENTIFIER_DIRECTORY_LINK: identifierType = "Directory Link"; break;
    }

    char* permissions;
    switch( fileToPrint->permissions ) {
        case PERMISSION_READ: permissions = "Read"; break;
        case PERMISSION_WRITE: permissions = "Read Write"; break;
        case PERMISSION_EXECUTE: permissions = "Read Write Execute"; break;
    }

    printf( "Identifier Type: %s\n", identifierType );
    printf( "File Name: %s\n", fileToPrint->fileName );
    printf( "Permissions: %s\n", permissions );
    printf( "Modified: %lu\n", fileToPrint->modified );
    printf( "Created: %lu\n", fileToPrint->created );
    printf( "File Size: %lu\n", fileToPrint->fileSize );

    return 0;
}

char* getContent( char* filePath ) {
    unsigned long blockLocation = getBlockLocationFromPath( filePath );
    file* fileToRead = calloc( file_mallocSize, 1 );
    LBAread( (void*)fileToRead, file_lbaSize, blockLocation );

    if( !isValidFile( fileToRead ) || !isFile( fileToRead ) || !isReadable( fileToRead ) ) {
        printf( "Not a valid file or file not readable\n");
        return NULL;
    }

    int contentBlockAmount = ( fileToRead->fileSize / mainSystemInfo->lbaSize ) + 1;
    int bufferMallocSize = contentBlockAmount * mainSystemInfo->lbaSize;
    char* content = calloc( bufferMallocSize, 1 );

    LBAread( (void*)content, contentBlockAmount, fileToRead->startingBlock );

    free( fileToRead );
    return content;
}