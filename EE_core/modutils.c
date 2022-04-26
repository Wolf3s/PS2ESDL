#include <stdio.h>
#include <iopheap.h>
#include <loadfile.h>
#include <kernel.h>
#include <sifdma.h>
#include <sifrpc.h>
#include <string.h>

#include "main.h"
#include "modutils.h"

extern SifRpcClientData_t _lf_cd; /* Declared in PS2SDK library */

/* Pointers externally generated modules */
unsigned char *UDNL_irx;
unsigned int size_UDNL_irx;

void InitPointers(void){
	struct ModuleEntry *Module;

	Module=(struct ModuleEntry *)0x00088000;

	UDNL_irx=Module[0].buffer;
	size_UDNL_irx=Module[0].size;
}

static inline void _SifLoadModuleBuffer(void *ptr, int arg_len, const char *args)
{
	struct _lf_module_buffer_load_arg arg;

	memset(&arg, 0, sizeof arg);

	arg.p.ptr = ptr;
	if (args && arg_len) {
		arg.q.arg_len = arg_len > LF_ARG_MAX ? LF_ARG_MAX : arg_len;
		memcpy(arg.args, args, arg.q.arg_len);
	}

	SifCallRpc(&_lf_cd, LF_F_MOD_BUF_LOAD, SIF_RPC_M_NOWAIT, &arg, sizeof arg, NULL, 0, NULL, NULL);
}

void SifExecModuleBufferAsync(void *ptr, unsigned int size, unsigned int arg_len, const char *args)
{
	SifDmaTransfer_t dmat;
	void *iop_addr;
	unsigned int qid;

	/* Round the size up to the nearest 16 bytes. */
	size = (size + 15) & -16;

	iop_addr = SifAllocIopHeap(size);

	dmat.src = ptr;
	dmat.dest = iop_addr;
	dmat.size = size;
	dmat.attr = 0;
	do{
		qid = SifSetDma(&dmat, 1);
	}while(!qid);

	while(SifDmaStat(qid) >= 0);

	_SifLoadModuleBuffer(iop_addr, arg_len, args);
}
