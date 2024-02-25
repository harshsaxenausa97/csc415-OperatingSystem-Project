#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "filesystem.h"
#include "terminal.h"

int main( int argc, char* argv[] ) {
    startFileSystem( "fsVolume", 512000, 512 );
    startTerminal();
    closeFileSystem();
}
