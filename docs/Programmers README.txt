version: 2.30
Date: 2011/02/02

Playstation 2 External Storage Device games Loader (PS2ESDL) readme for DEVELOPERS
=======================================================================================

What you SHOULD know before attempting to work with PS2ESDL:
------------------------------------------------------------
For modifying/working with the following parts of PS2ESDL, I do strongly recommend you have/understand beforehand:

On top of being rather proficient with using the PS2SDK and understanding that the EE/IOP has 32-bit registers sizes (Integers are 32-bits wide)....

when you work with:

CDVDFSV:
	-Understand how the EE communicates with the IOP (Via the SIF).
	-Know how to program RPC servers on the IOP.

CDVDMAN:
	-Understand the ISO9660 standard (ECMA-119).

EE_core:
	-Understand what the Playstation 2/MIPS "Kernel mode"/"Supervisor mode" is and what it does.
	-Understand how the IOP is reset and how modules are loaded via the EE.

GUI:
	-Know how to control the pads, load modules, how ELF files are structured (If you're modifying the loader) and how to use the PS2SDK gsKit.

EESYNC:
	-Know the IOP reset sequence.

ERRORTRAP:
	-Know how MIPS processors handle exceptions.

IOPRPPatch:
	-Understand the structure of the IOPRP image files.

Additional notes:
-----------------

Parts of PS2ESDL:
	PS2ESDL\
		GUI\			<- Main GUI of PS2ESDL. Also includes a special "blind" (Automated) loader for use with debugging.
		EE_CORE\		<- EE core of PS2ESDL. The program here remains in EE memory when a game is running.
		MODULES\

			CDVDMAN\		<- Replacement CDVDMAN module (USBSTOR is integrated here).
			CDVDFSV\		<- Replacement CDVDFSV module.
			<Lots of IOP modules>	<- Compiled binaries of the modules either found in the PS2SDK or in the SRC folder (See below).
			SRC\
				EESYNC\		<- Replacement EESYNC module.
				IMGDRV\		<- A "RAM drive" virtual device. Loaded, and used to reset the IOP with a patched IOPRP image that contains the necessary modules.
				ERRORTRAP\	<- IOP error/crash/exception trap (Used for debugging).
				UDPTTY\		<- TTY output driver. Can be used with any Pukklink compatible client to log debug messages.
				USBHDFSD\	<- USB Mass Storage Device driver with an extra IOCTL call that returns the LBA of a pre-opened file.
						Drivers that replace this one must also support that IOCTL call, offering the same functionality.
				IOPRPPatch\	<- A module that generates/patches IOPRP images to contain PS2ESDL's CDVDMAN driver.

Important notes:
----------------

EE_CORE:
		-WATCH WHAT YOU INSERT AND MODIFY!! SOME GAMES SEEM TO BE SENSITIVE TO THE SIF FUNCTIONS THAT ARE INVOKED ON THE EE!!
		-Try not to insert any printf() statements inside the IOP reset code.
		-Depending on your game, you may need to uncomment out the defination of "DISABLE_FINAL_PRINTFS" (Without the quotes) inside main.h.
			Some games that come bundled with a FILEIO module that crashes when the EE core uses printf().
			When this happens, you'll see some messages like "thread alloc fail"... and the IOP will quickly run out of memory.

		!! NOTE THAT THIS IS WILL ONLY AFFECT DEBUGGING !!

CDVDMAN.IRX:
		-There are actually 3 different structures used by sceCdSearchFile() and sceCdLayerSearchFile() functions.
			Thankfully, SCE did not change the layout of the data present in those structures, but only added more fields in newer (and larger) structures.
			As we don't know what version of CDVDMAN (And hence what version of the library the game has), we can't assume that those extra fields are present.

			The 1st note for CDVDFSV.IRX will explain further.

CDVDFSV.IRX:
		-As CDVDFSV is the RPC server for the EE, this is related to the 1st note for CDVDMAN.IRX:

			There are actually 3 different structures used by sceCdSearchFile() and sceCdLayerSearchFile() functions.

			The 3 structures have the following sizes (In increasing order, in bytes): 0x124, 0x128 and 0x12C.

			The 0x124 byte structure is the most basic structure used, with all the basic fields.
			The 0x128 byte structure is the same as th 0x124, but with 1 extra field at the end of the CDVDMAN.IRX cd_file_t structure: A 32-bit field that is used to store the ISO9660 file type flags.
			The 0x12C byte structure is the same as the 0x124, but with 1 extra 32-bit field at the end of the structure that states the layer on the DVD to search for the file on.

IOPRPPatch:
		-Like with the EE core, watch what you insert and modify.

APPENDIX A: Data structures
---------------------------
1. PS2ESDL Disc Image (PDI) file (v1.10):

	Offset

	0x00 - Signature ("PDI").
	0x02 - Version.

	0x03 and onwards: Data structures that list the games that exist on that disk.


2. PS2ESDL Patch Index (PPI) file:

	Offset

	0x00 - Signature ("PPI" and a 0x00 byte as a padding field).
	0x03 - Version.

	0x04 and onwards: Data structures that list the available patches for games that need patching.

	After that section, there will be a section that lists the games that need patching, the type of patch they need, and how the patch should be applied to the game, once it's loaded into memory.

-------------

All the best!
-SP193
