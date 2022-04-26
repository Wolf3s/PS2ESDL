/* EESYNC - Synchronizes the EE and the IOP at the end of every IOP reset. */
/*	Version: 1.20
*	Last updated: 2014/05/17
*/

#include <loadcore.h>
#include <sifman.h>

#include "loadcore_add.h"

#define MODNAME "SyncEE"
IRX_ID(MODNAME, 0x99, 0x99);

static int PostResetCallback(void)
{
	sceSifSetSMFlag(0x40000);
	return 0;
}

int _start(int argc, char** argv)
{
	/*	SP193: Removed all preceding code before the next few lines:
		If this module was to fail to load... the PS2 will freeze anyway.

		Also, it does not have any SCE listed exports. Neither do any modules link to it.
		So why bother registering an export table?	*/

	loadcore20(PostResetCallback, 2, 0);

	return MODULE_RESIDENT_END;
}
