#include "new_cdvdman.h"
#include <intrman.h>
#include "new_ioman.h"
#include <irx.h>
#include <loadcore.h>
#include <sifcmd.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <types.h>
#include <thbase.h>

#include "cdvdfsv.h"

#define MODNAME "cdvd_ee_driver"
IRX_ID(MODNAME, 0x04, 0x23);

/* Global run-time variables */
int cdvdfsv_def_pri=0x51;

int cdvdfsv_ths_ids[4];
extern struct irx_export_table _exp_cdvdfsv;

/* CDVDMAN's event flags */
extern int cdvdman_scmd_ef;

/* Common thread creation data */
iop_thread_t thread_data={
	TH_C,			/* rpc_thp.attr */
	0,			/* rpc_thp.option */
	&cdvdfsv_main_th,	/* rpc_thp.thread */
	0x800,			/* rpc_thp.stacksize */
	0			/* rpc_thp.piority; Configure this later */
};

static iop_library_t dummy_modload_exp={
	(void *)EXPORT_MAGIC,
	NULL,
	0x101,
	0,
	"modload",
};

/* Entry point */
int _start(int argc,char *argv[]){
	if(argc<0){
		cdvdfsv_remrpcs();
		return MODULE_NO_RESIDENT_END;
	}

	thread_data.priority=cdvdfsv_def_pri-1;
	StartThread(cdvdfsv_ths_ids[0]=CreateThread(&thread_data), NULL);

	RegisterLibraryEntries(&_exp_cdvdfsv);

	/* We want to get the version number of modload. Refer to the iop_library_t structure in loadcode.h.
		If MODLOAD is v1.04 or newer, return REMOVABLE_RESIDENT(2) instead to indicate that this module is unloadable.
		Otherwise, return MODULE_RESIDENT(0) to just indicate that the module is resident.

		The older versions of MODLOAD do not support the MODULE_REMOVABLE return value and will crash.
	*/
	 return((*(u16 *)(QueryLibraryEntryTable(&dummy_modload_exp)-12)<0x104)?MODULE_RESIDENT_END:2);
}

