#include "new_cdvdman.h"
#include <dmacman.h>
#include <errno.h>
#include <intrman.h>
#include "new_ioman.h"
#include <irx.h>
#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thevent.h>
#include <thsemap.h>
#include <types.h>

#include "cdvdman_hdr.h"

#define MODNAME "cdvd_driver"
IRX_ID(MODNAME, 0x04, 0x60);

extern struct irx_export_table _exp_cdvdman;

/* Device driver I/O functions */
static iop_device_ops_t cdvdman_functarray={
	&cdrom_init,		/* INIT */
	&cdrom_deinit,		/* DEINIT */
	(void*)&cdrom_nulldev,	/* FORMAT */
	&cdrom_open,		/* OPEN */
	&cdrom_close,		/* CLOSE */
	&cdrom_read,		/* READ */
	(void*)&cdrom_nulldev,	/* WRITE */
	&cdrom_lseek,		/* LSEEK */
	&cdrom_ioctl,		/* IOCTL */
	(void*)&cdrom_nulldev,	/* REMOVE */
	(void*)&cdrom_nulldev,	/* MKDIR */
	(void*)&cdrom_nulldev,	/* RMDIR */
	&cdrom_dopen,		/* DOPEN */
	(void*)&cdrom_nulldev,	/* DCLOSE */
	&cdrom_dread,		/* DREAD */
	&cdrom_getstat,		/* GETSTAT */
	(void*)&cdrom_nulldev,	/* CHSTAT */
	/* For the "new" ioman only */
	(void*)&cdrom_nulldev,	/* RENAME */
	(void*)&cdrom_nulldev,	/* CHDIR */
	(void*)&cdrom_nulldev,	/* SYNC */
	(void*)&cdrom_nulldev,	/* MOUNT */
	(void*)&cdrom_nulldev,	/* UMOUNT */
	(void*)&cdrom_nulldev64,/* LSEEK64 */
	&cdrom_devctl,		/* DEVCTL */
	(void*)&cdrom_nulldev,	/* SYMLINK */
	(void*)&cdrom_nulldev,	/* READLINK */
	&cdrom_ioctl2		/* IOCTL2 */
};

static const char cdvdman_dev_name[]="cdrom";

/* CDVDMAN emulation configuration */
const struct cdvdman_config cdvdman_conf={
	SCECdPS2DVD,	/* Type of disc loaded */       
	0,		/* CDVDMAN operation mode */

	/* Information on the disc/individual layers. */
	{	/* Layer 0 */
		{
			0x105,
			0x400,
			0x107,
			0x10000000,
		},

		/* Layer 1 */
		{
			0x105,
			0x400,
			0x107,
			0x10000000,
		}
	},

	0x12345678,			/* Layer break for DVD9 discs only. It's set to 0 when the disc image represents a DVD5 disc. */
	0x12345678,			/* Maximum number of sectors per disc image slice */

	32,				/* Size of the read buffer (In 2048-byte sectors). */

	{0x11, 0x22, 0x33, 0x44, 0x55},	/* DNAS key */
};

static iop_device_t cdvdman_cddev={
	cdvdman_dev_name,              /* Device name */
	IOP_DT_FS|IOP_DT_FSEXT,        /* Device type flag */
	1,                             /* Version */
	"CD-ROM ",                     /* Description. Yes, there is a space after CD-ROM */
	&cdvdman_functarray            /* Device driver function pointer array */
};

/* Variables declared in "cdvdmanc.c" */
extern unsigned char *cdvdman_fsvrbuf;

extern struct cdvdman_async async_config;
extern struct cdvd_cmdstruct cdvdman_cmdstruct;

extern int cdvdman_intr_ef;
extern int cdvdman_scmd_sema;
extern int cdvdman_ncmd_sema;
extern int cdvdman_srch_sema;

/*******************************************************************
Installs hardware event handlers (DMA ch. 3 and CD/DVD hardware)
********************************************************************/
static inline void install_intr_handlers(void){
	RegisterIntrHandler(INUM_CDROM, 1, &intrh_cdrom, &cdvdman_cmdstruct);
	EnableIntr(INUM_CDROM);

	/* Acknowledge hardware events (e.g. poweroff) */
	if(CDVDreg_PWOFF&CDL_DATA_END) CDVDreg_PWOFF=CDL_DATA_END;
	if(CDVDreg_PWOFF&CDL_DATA_RDY) CDVDreg_PWOFF=CDL_DATA_RDY;
}

/* Entry point */
int _start(int argc,char *argv[])
{
	iop_sema_t SemaData;
	iop_event_t evfp;

	cdvdman_fsvrbuf=malloc(cdvdman_conf.read_buffer_sz*2048); /* Allocate memory for read buffer */

	SemaData.attr=0;
	SemaData.option=0;
	SemaData.initial=1;
	SemaData.max=1;
	cdvdman_scmd_sema=CreateSema(&SemaData);
	cdvdman_ncmd_sema=CreateSema(&SemaData);
	cdvdman_srch_sema=CreateSema(&SemaData);

	evfp.attr=2;	/* evfp.attr=EA_MULTI */
	evfp.option=0;
	evfp.bits=0;
	cdvdman_intr_ef=CreateEventFlag(&evfp);

	memset(&async_config, 0, sizeof(struct cdvdman_async)); /* Flood-fill with 0s */

	RegisterLibraryEntries(&_exp_cdvdman);

	DelDrv(cdvdman_dev_name);
	AddDrv(&cdvdman_cddev);

	install_intr_handlers();

	return MODULE_RESIDENT_END; /* Return 0 on success */
}

int _exit(int argc,char *argv[]){
	int dummy;

	DeleteEventFlag(cdvdman_intr_ef);
	DeleteSema(cdvdman_ncmd_sema);
	DeleteSema(cdvdman_scmd_sema);
	DeleteSema(cdvdman_srch_sema);

	free(cdvdman_fsvrbuf);

	DisableIntr(INUM_CDROM, &dummy);
	ReleaseIntrHandler(INUM_CDROM);

	return MODULE_NO_RESIDENT_END;
}

int invalid_export(){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Warning! Invalid export called.\n");
	return 1;
}
