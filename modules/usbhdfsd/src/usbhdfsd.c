/*
 * usb_mass.c - USB Mass storage driver for PS2 $Revision: 1530 $
 */

#define MAJOR_VER 1
#define MINOR_VER 4

#include <loadcore.h>
#include <stdio.h>

extern int InitFAT();
extern int InitFS();
extern int InitUSB();

int _start( int argc, char **argv)
{
	printf("USB HDD FileSystem Driver v%d.%d\n", MAJOR_VER, MINOR_VER);

    // initialize the FAT driver
    if(InitFAT() != 0)
    {
        printf("Error initializing FAT driver!\n");
        return MODULE_NO_RESIDENT_END;
    }

    // initialize the USB driver
	if(InitUSB() != 0)
    {
        printf("Error initializing USB driver!\n");
        return MODULE_NO_RESIDENT_END;
    }

    // initialize the file system driver	    
    if(InitFS() != 0)
    {
        printf("Error initializing FS driver!\n");
        return MODULE_NO_RESIDENT_END;
    }

    // return resident
    return MODULE_RESIDENT_END;
}
