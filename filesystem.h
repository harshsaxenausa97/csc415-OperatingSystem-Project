#ifndef FILE_SYSTEM_DRIVER_H
#define FILE_SYSTEM_DRIVER_H

#include "systemstructs.h"

sysInfo* mainSystemInfo;
unsigned int system_lbaSize;
unsigned int system_mallocSize;
unsigned int free_lbaSize;
unsigned int free_mallocSize;
unsigned int file_lbaSize;
unsigned int file_mallocSize;

#define ROOTNAME "root"

int startFileSystem( char* volumeName, unsigned long volumeSize, unsigned long blockSize );
int initializeSystemInfo( char* volumeName, unsigned long volumeSize, unsigned long blockSize );
int createNewSystem( char* volumeName, unsigned long volumeSize, unsigned long blockSize );
int initializeFreeSpace( const unsigned long beginLocation );
int copyFromVolumeToLinux( char* ourPath, char* linuxPath );
int copyFromLinuxToVolume( char* linuxFileName, char* volumeFileName );
int moveFile( char* moveFrom, char* moveTo );
unsigned long copyFile( char* moveFrom, char* moveTo );
unsigned long getBlockLocationFromPath( char* filePath );
unsigned long getBlockLocationFromName( unsigned long blockLocation, char* fileName );
char* getParentPath( char* filePath );
unsigned long addFile( char* filePath );
unsigned long makeBlank();
unsigned long makeDirectory( char* filePath );
unsigned long makeFile( char* filePath );
int addChild( unsigned long parentLocation, unsigned long childLocation );
int removeChild( unsigned long parentLocation, unsigned long childLocation );
void listChildren( unsigned long blockLocation );
void listChildrenFromPath( char* absolutePath );
char* filePathConcat( const char *s1, const char *s2 );
unsigned int getFilePermissons( unsigned long blockLocation );
int setCount( unsigned long blockLocation, unsigned int count );
int setStartingBlock( unsigned long headerBlockLocation, unsigned long dataBlockLocation );
int setDefaultMetadata( unsigned long blockLocation );
int setFileCreatedAt( unsigned long blockLocation );
int setFileModifiedAt( unsigned long blockLocation );
int setFileId( unsigned long blockLocation );
int setFileName( unsigned long blockLocation, char* fileName );
int setFilePermissions( unsigned long blockLocation, char* newPermissionStr );
int setFileIdentifierType( unsigned long blockLocation, char* identifierTypeStr );
int writeFileData( unsigned long blockLocation, int numberOfBlocks, void* fileBuffer, int fileSize );
int recursiveDelete( unsigned long blockLocation );
int delete( unsigned long blockLocation, unsigned int amountToFree );
int deleteFilePath( char* filePath );
unsigned long getFreeBlocks( unsigned int numberOfFreeBlocksWanted );
int isFile( file* fileToCheck );
int isFile_pathVersion( char* path );
int isDirectory( file* fileToCheck );
int isFileLink( file* fileToCheck );
int isDirectoryLink( file* fileToCheck );
int isReadable( file* fileToCheck );
int isWritable( file* fileToCheck );
int isExecutable( file* fileToCheck );
int isValidFile( file* fileToCheck );
int isValidSystemInfo( sysInfo* systemInfoToCheck );
int isValidFreeSpace( freeSpace* freeSpaceToCheck );
char* getCopyOfString( char* string );
int closeFileSystem();
int printMetadata( char* path );
char* getContent( char* filePath );

#endif /* FILE_SYSTEM_DRIVER_H end guard */
