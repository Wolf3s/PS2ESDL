#include <libcdvd.h>
#include <kernel.h>
#include <stdio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifdma.h>
#include <sifrpc.h>
#include <string.h>

#include "main.h"
#include "ioprstctrl.h"

extern unsigned char *UDNL_irx;
extern unsigned int size_UDNL_irx;

extern char SifSetReg_lock;
extern char SifSetDma_lock;

extern int _iop_reboot_count;

static void SifIopResetUDNL(void *udnl_irx, unsigned int udnl_irx_len, const char *args, unsigned int arglen){
	SifExecModuleBufferAsync(udnl_irx, udnl_irx_len, arglen, args);

	SifSetReg(SIF_REG_SMFLAG, 0x70000);
	SifSetReg(0x80000002, 0);
	SifSetReg(0x80000000, 0);

	SifExitIopHeap();
	SifLoadFileExit();

	_iop_reboot_count++;

	while(!SifIopSync()){};
}

extern struct PS2ESDL_EE_conf EE_core_conf;
unsigned char IOP_resets=0;
/*--------------------------------------------------------------------------------------------------------------*/
/* A function that creates and send IOP reset SIF DMA packets                                                   */
/* containing commands to reset the IOP with a modified IOPRP image.                                            */
/*--------------------------------------------------------------------------------------------------------------*/
void New_SifIopReset(char *arg, unsigned int arglen)
{
	const char *ioprp_cmd;
	unsigned int ioprp_cmd_length;

	/* Disable all function locks, allowing direct calls to the original Syscall handlers */
	SifSetReg_lock=0;
	SifSetDma_lock=0;

	/* The game's IOPRP image(s) is loaded starting from the 3rd IOPRP reset PS2ESDL makes. */
	SifInitRpc(0);
	while(!SifIopReset(NULL, 0)){};

	/* Do something useful while waiting for the IOP. */
	if(arglen>0){
		ioprp_cmd=&arg[10];
		arg[arglen]='\0';
		ioprp_cmd_length=arglen-10;
	}
	else{
		ioprp_cmd=NULL;
		ioprp_cmd_length=0;
	}

	while(!SifIopSync()){};

	SifInitRpc(0);
	SifInitIopHeap();
	SifLoadFileInit();

	sbv_patch_enable_lmb();

#ifdef DEBUG_TTY_FEEDBACK
	fioInit();
	LoadDebugModules();
	DEBUG_PRINTF(	"PS2ESDL_EE_CORE: IOP reset request received with IOP image: %s\n"
			"PS2ESDL_EE_CORE: Pre-reseting IOP...\n", arg);
	fioExit();
#endif

	SifIopResetUDNL(UDNL_irx, size_UDNL_irx, NULL, 0);

	if(ioprp_cmd_length>0){
		SifInitRpc(0);
		SifInitIopHeap();
		SifLoadFileInit();

		sbv_patch_enable_lmb();

		SifIopResetUDNL(UDNL_irx, size_UDNL_irx, ioprp_cmd, ioprp_cmd_length);
	}

#ifdef DEBUG_TTY_FEEDBACK
	SifInitRpc(0);
	SifInitIopHeap();
	SifLoadFileInit();

	sbv_patch_enable_lmb(); /* Apply the SBV LMB patch on IOPRP images that have disabled LoadModuleBuffer() RPC dispatchers */

	LoadDebugModules();

#ifndef DISABLE_FINAL_PRINTFS
	fioInit();
	DEBUG_PRINTF("PS2ESDL_EE_CORE: Completed IOP reset with IOPRP image.\nPS2ESDL_EE_CORE: Loading debugging modules...\n");
	fioExit();
#endif

	/* Exit SIF RPC services */
	SifLoadFileExit();
	SifExitIopHeap();
#endif

	SifExitRpc();

	/* Enable function locks */
	SifSetDma_lock=1;
}
