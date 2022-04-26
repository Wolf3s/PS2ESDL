#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <libcdvd.h>
#include <loadfile.h>
#include <stdio.h>
#include <string.h>
#include <sifrpc.h>

#include "main.h"

extern struct PS2ESDL_EE_conf EE_core_conf;

extern char SifSetReg_lock;

/*----------------------------------------------------------------------------------------------*/
/* This function is called when Hook_LoadExecPS2 is called                                      */
/*----------------------------------------------------------------------------------------------*/
inline void New_LoadExecPS2(const char *filename, int argc, char *argv[]){
	t_ExecData elf;
	int result;

	/* Reset the IOP. The RPC subsystems will be kept synchonised by New_SifResetIop(). */
	New_SifIopReset(NULL, 0);

	SifInitRpc(0);
	SifLoadFileInit();
#ifdef DEBUG_TTY_FEEDBACK
	fioInit();
#endif

	DEBUG_PRINTF("PS2ESDL_EE_CORE: LoadExecPS2 load request: %s\n", filename);

	elf.epc=0;
	result=SifLoadElf(filename, &elf); /* Note! Arguments will not be overwritten as they have been copied into kernel memory by LoadExecPS2() */

	if(result==0 && elf.epc!=0){
		if(EE_core_conf.patch.mode!=PATCH_END) patch_ELF((void *)elf.epc, &EE_core_conf.patch);

		DEBUG_PRINTF("Closing services and attempting to execute ELF...\n");
	}
	else{
	        DEBUG_PRINTF("PS2ESDL_EE_CORE: FATAL! Failure within LoadExecPS2()\n");
	}

	/* Exit SIF RPC services */
#ifdef DEBUG_TTY_FEEDBACK
	fioExit();
#endif
	SifLoadFileExit();
	SifExitRpc();

	/* finally, execute the ELF ... */
	if(result==0 && elf.epc!=0) ExecPS2((void *)elf.epc, (void *)elf.gp, argc, argv);
	else{
        	/* Should not reach here */
		GS_BGCOLOUR=0x0000FF;

	//	ExecOSD(0, NULL);
		SleepThread();
	}
}
