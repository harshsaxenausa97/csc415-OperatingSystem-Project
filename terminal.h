#ifndef TERMINAL_H
#define TERMINAL_H

int startTerminal();
char** stringToArrayOfStrings( char* oldString );
void stopRunning();
char* getCurrentFilePath();
void setCurrentFilePath( char* path );

#endif /* TERMINAL_H end guard */