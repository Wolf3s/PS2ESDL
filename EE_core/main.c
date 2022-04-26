/*
  Main EE CORE of PlayStation 2 External Storage Device game Loader (PS2ESDL)
 */
#include <kernel.h>
#include <syscallnr.h>
#include <stdio.h>

#include "main.h"

struct PS2ESDL_EE_conf EE_core_conf={
	0xDEADBEEF,

	/* Patches that need to be applied to the game's ELF once it's loaded into memory. */
	{
		0, /* mode */

		{	/* src */
			0x11111111,
			0x22222222,
			0x33333333,
			0x44444444,
		},

		{	/* mask */
			0x55555555,
			0x66666666,
			0x77777777,
			0x88888888,
		},

		{	/* patchData */
			0x99999999,
			0xAAAAAAAA,
			0xBBBBBBBB,
			0xCCCCCCCC,
		},
	}
};

u8 mode3_enabled=0;

extern u32 (*ori_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);
extern int (*ori_SifSetReg)(u32 register_num, int register_value);
/*--------------------------------------------------------------*/
int main(int argc,char *argv[]) /* This EE core replaces rom0:EELOAD */
{
	InitPointers();

	/* Installing kernel syscall hooks, if they were not already installed. */
	if(ori_SifSetDma==NULL){
		Install_Kernel_Hooks();
		if(EE_core_conf.flags&HDL_MODE_3) mode3_enabled=1;
	}

	New_LoadExecPS2(argv[1], argc-1, &argv[1]);

	return 0; /* Should not reach here. */
}

/*--------------------------------------------------------------*/
inline void Install_Kernel_Hooks(void){
	ori_SifSetDma=GetSyscallHandler(__NR_SifSetDma);
	SetSyscall(__NR_SifSetDma, &Hook_SifSetDma);

	ori_SifSetReg=GetSyscallHandler(__NR_SifSetReg);
	SetSyscall(__NR_SifSetReg, &Hook_SifSetReg);
}

