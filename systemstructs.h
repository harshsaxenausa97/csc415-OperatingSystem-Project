#ifndef SYSTEM_STRUCTS_H
#define SYSTEM_STRUCTS_H

#define IDENTIFIER_FILE 0b00
#define IDENTIFIER_DIRECTORY 0b01
#define IDENTIFIER_FILE_LINK 0b10
#define IDENTIFIER_DIRECTORY_LINK 0b11
#define PERMISSION_READ 0b001
#define PERMISSION_WRITE 0b010
#define PERMISSION_EXECUTE 0b100
#define NUMBER_OF_CHILDREN 64
#define FILESIGNATURE1 0x6512F67ED9EF96A6
#define FILESIGNATURE2 0x45A7D995E6BB8322
#define ID_LENGTH 64

typedef struct fileStruct {
    unsigned long signature1;

    unsigned int identifierType : 2; // helps identify dir/file/link                    
                                     // 00 file, 01 directory
                                     // 10 link to file, 11 link to directory
    char fileName[256];
    char id[64]; 
    unsigned int permissions : 3; // permissions to read/write/exec/etc
					              // 001 read, 010 write, 100 execute
    unsigned long modified;
    unsigned long created;
    unsigned long fileSize;
    unsigned long startingBlock;      
    unsigned long children[NUMBER_OF_CHILDREN];

    unsigned long signature2;
} file;

#define SYSTEMSIGNATURE1 0x11B3DF89400A8A4E
#define SYSTEMSIGNATURE2 0x88AADF38E9904DBC
typedef struct fileSysInfo {
    unsigned long signature1;
	unsigned long volumeSize;
	unsigned long freeHeadBlock;
	unsigned long rootLocation;
	char volumeName[256];
	unsigned int lbaSize;     // LBA Size in bytes per block
    unsigned long signature2;
} sysInfo;

#define FREESIGNATURE1 0xA9F3589E2BB4C421
#define FREESIGNATURE2 0x2B9914DF430A8E83

typedef struct freeSpaceInfo {
	unsigned long signature1;
	unsigned long next;
	unsigned long prev;
	unsigned int count;
	unsigned long signature2;
} freeSpace;

#endif /* SYSTEM_STRUCTS_H end guard */
