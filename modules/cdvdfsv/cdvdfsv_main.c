#include "new_cdvdman.h"
#include <intrman.h>
#include <sifcmd.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>

#include "new_ioman.h"
#include "cdvdfsv.h"

extern int cdvdfsv_ths_ids[4];

/* Data used for registering the RPC servers */
static SifRpcServerData_t rpc_sdata1;
static unsigned char rpc_buffer1[16];	/* RPC #1: sceCdInit */
static SifRpcDataQueue_t rpc_qdata1;

static SifRpcServerData_t rpc_sdata2;
static unsigned char rpc_buffer2[16];	/* RPC #2: sceCdGetDiskReady */
static SifRpcDataQueue_t rpc_qdata2;

static SifRpcServerData_t rpc_sdata3;
static unsigned char rpc_buffer3[48];	/* RPC #3: S-commands */
static SifRpcDataQueue_t rpc_qdata3;

static SifRpcServerData_t rpc_sdata4;	/* RPC #5: File searching... */
static unsigned char rpc_buffer4[304];

static SifRpcServerData_t rpc_sdata5;	/* RPC #5: N-commands */
static unsigned char rpc_buffer5[800];

static SifRpcServerData_t rpc_sdata6;	/* RPC #3: "New" file searching RPC call */

static SifRpcServerData_t rpc_sdata7;	/* "Unknown" RPC: Power-off handler routine?*/
static unsigned char rpc_buffer6[16];

static void *cbrpc_cdinit(int,void *,int);
static void *cbrpc_diskready(int,void *,int);
static void *cbrpc_cdvdscmds(int,void *,int);
static void *cbrpc_fscall(int,void *,int);
static void *cbrpc_cdvdncmds(int,void *,int);
static void *cbrpc_80000596(int,void *,int);

void cdvdfsv_remrpcs(void)
{
	unsigned char i;

	sceSifRemoveRpc(&rpc_sdata1, &rpc_qdata1);
	sceSifRemoveRpc(&rpc_sdata2, &rpc_qdata1);
	sceSifRemoveRpc(&rpc_sdata3, &rpc_qdata1);
	sceSifRemoveRpc(&rpc_sdata6, &rpc_qdata3);
	sceSifRemoveRpc(&rpc_sdata4, &rpc_qdata2);
	sceSifRemoveRpc(&rpc_sdata5, &rpc_qdata2);

	sceSifRemoveRpcQueue(&rpc_qdata1);
	sceSifRemoveRpcQueue(&rpc_qdata2);
	sceSifRemoveRpcQueue(&rpc_qdata3);

	for(i=0; i<4; i++){
		TerminateThread(cdvdfsv_ths_ids[i]);
		DeleteThread(cdvdfsv_ths_ids[i]);
	}
}

/* rpc handling thread #1 */
void cdvdfsv_rpc1_th(void){
	sceSifSetRpcQueue(&rpc_qdata1, GetThreadId());

	sceSifRegisterRpc(&rpc_sdata1, 0x80000592, &cbrpc_cdinit, rpc_buffer1, NULL, NULL, &rpc_qdata1);
	sceSifRegisterRpc(&rpc_sdata2, 0x8000059A, &cbrpc_diskready, rpc_buffer2, NULL, NULL, &rpc_qdata1); /* Old RPC call */
	sceSifRegisterRpc(&rpc_sdata3, 0x80000593, &cbrpc_cdvdscmds, rpc_buffer3, NULL, NULL, &rpc_qdata1);

	sceSifRpcLoop(&rpc_qdata1);
}

/* rpc handling thread #2 */
void cdvdfsv_rpc2_th(void){
	sceSifSetRpcQueue(&rpc_qdata2, GetThreadId());

	sceSifRegisterRpc(&rpc_sdata4, 0x80000597, &cbrpc_fscall, rpc_buffer4, NULL, NULL, &rpc_qdata2);
	sceSifRegisterRpc(&rpc_sdata5, 0x80000595, &cbrpc_cdvdncmds, rpc_buffer5, NULL, NULL, &rpc_qdata2);

	sceSifRpcLoop(&rpc_qdata2);
}

/* rpc handling thread #3 */
void cdvdfsv_rpc3_th(void){
	sceSifSetRpcQueue(&rpc_qdata3, GetThreadId());

	sceSifRegisterRpc(&rpc_sdata6, 0x8000059C, &cbrpc_diskready, rpc_buffer2, NULL, NULL, &rpc_qdata3);	/* New RPC call */
	sceSifRegisterRpc(&rpc_sdata7, 0x80000596, &cbrpc_80000596, rpc_buffer6, NULL, NULL, &rpc_qdata3);	/* RPC 0x80000596 */

	sceSifRpcLoop(&rpc_qdata3);
}

/* RPC #1 callback */
static void *cbrpc_cdinit(int fno, void *buf, int size){
	static CDVDInitResult cdvdfsv_initres;

	DEBUG_PRINTF("PS2ESDL_CDVDFSV: sceCdInit called: fno: %d, Parameter=%x\n", fno, *(int*)buf);

	cdvdfsv_initres.result=sceCdInit(*(int*)buf);
	cdvdfsv_initres.debug_mode=0;
	cdvdfsv_initres.cdvdfsv_ver=CDVDFSV_VERSION;
	cdvdfsv_initres.cdvdman_ver=sceCdSC(0xFFFFFFF7, NULL);

	DEBUG_PRINTF("PS2ESDL_CDVDFSV: sceCdInit initilization completed sucessfully.\n");

	return &cdvdfsv_initres;
}

/* RPC #2 & #6 callback */
static void *cbrpc_diskready(int fno, void *buf, int size){
	static CDVDReadyResult cdvdfsv_hwready;

	DEBUG_PRINTF("PS2ESDL_CDVDFSV: sceCdDiskReady call: Mode=%x\n", ((int*)buf)[0]);
	cdvdfsv_hwready.result=sceCdDiskReady(*(int*)buf);
	return &cdvdfsv_hwready.result;
}

static void NullScmdHandler(void* buffer, int size, CDVDCmdResult* CommandResult){
	CommandResult->result=0;
	DEBUG_PRINTF("PS2ESDL_CDVDFSV: Unknown S-command received.\n");
}

#define FIRST_FNO_SCMD_HANDLERS_GROUP_01	01
#define LAST_FNO_SCMD_HANDLERS_GROUP_01		12
static const ScmdHandler ScmdHandlers01[]={
	&cdvdfsv_readclock,	/* 01 */
	&NullScmdHandler,	/* 02 */
	&cdvdfsv_disktype,	/* 03 */
	&cdvdfsv_geterror,	/* 04 */
	&cdvdfsv_trayreq,	/* 05 */
	&cdvdfsv_cdri,		/* 06 */
	&NullScmdHandler,	/* 07 */
	&NullScmdHandler,	/* 08 */
	&NullScmdHandler,	/* 09 */
	&NullScmdHandler,	/* 10 */
	&cdvdfsv_applyscmd,	/* 11 */
	&cdvdfsv_cdstatus	/* 12 */
};

#define FIRST_FNO_SCMD_HANDLERS_GROUP_02	22
#define LAST_FNO_SCMD_HANDLERS_GROUP_02		26
static const ScmdHandler ScmdHandlers02[]={
	&cdvdfsv_cdBreak,	/* 22 */
	&NullScmdHandler,	/* 23 */
	&NullScmdHandler,	/* 24 */
	&NullScmdHandler,	/* 25 */
	&cdvdfsv_cdrm		/* 26 */
};

#define FIRST_FNO_SCMD_HANDLERS_GROUP_03	33
#define LAST_FNO_SCMD_HANDLERS_GROUP_03		39
static const ScmdHandler ScmdHandlers03[]={
	&cdvdfsv_poweroff,	/* 33 */
	&cdvdfsv_cdmmode,	/* 34 */
	&cdvdfsv_cdvdfsv5,	/* 35 */
	&cdvdfsv_cdReadGUID,	/* 36 */
	&NullScmdHandler,	/* 37 */
	&cdvdfsv_cdReadModelID,	/* 38 */
	&cdvdfsv_dualinfo	/* 39 */
};

/* RPC #3 callback */
static void *cbrpc_cdvdscmds(int fno, void *buf, int size){
	static CDVDCmdResult ccr;

	DEBUG_PRINTF("PS2ESDL_CDVDFSV: CDVD S-command sent=0x%x\n", fno);

	if(fno>=FIRST_FNO_SCMD_HANDLERS_GROUP_01 && fno<=LAST_FNO_SCMD_HANDLERS_GROUP_01){
		ScmdHandlers01[fno-FIRST_FNO_SCMD_HANDLERS_GROUP_01](buf, size, &ccr);
	}
	else if(fno>=FIRST_FNO_SCMD_HANDLERS_GROUP_02 && fno<=LAST_FNO_SCMD_HANDLERS_GROUP_02){
		ScmdHandlers02[fno-FIRST_FNO_SCMD_HANDLERS_GROUP_02](buf, size, &ccr);
	}
	else if(fno>=FIRST_FNO_SCMD_HANDLERS_GROUP_03 && fno<=LAST_FNO_SCMD_HANDLERS_GROUP_03){
		ScmdHandlers03[fno-FIRST_FNO_SCMD_HANDLERS_GROUP_03](buf, size, &ccr);
	}
	else{
		DEBUG_PRINTF("PS2ESDL_CDVDFSV: Unknown S-command sent: 0x%02x\n", fno);
		ccr.result=0;
	}

	DEBUG_PRINTF("PS2ESDL_CDVDFSV: Requested CDVD S-command completed\n"); 

	return &ccr;
}

/* RPC #4 callback */
static void *cbrpc_fscall(int fno, void *buf, int size){
	static CDVDSearchResult cdvdfsv_srchres;
	int oldstate;
	unsigned int dma_id;
	SifDmaTransfer_t cdvdfsv_fssdd;

	switch(size){
		case 0x12C:
			DEBUG_PRINTF("PS2ESDL_CDVDFSV: searching for file %s. Struct sz: 0x%x\n", ((advl_FSVRpc4Prm *)buf)->name, size);

			cdvdfsv_srchres.result=sceCdLayerSearchFile(&((advl_FSVRpc4Prm *)buf)->fp, ((advl_FSVRpc4Prm *)buf)->name, ((advl_FSVRpc4Prm *)buf)->layer);
			cdvdfsv_fssdd.src=&((advl_FSVRpc4Prm *)buf)->fp;
			cdvdfsv_fssdd.size=sizeof(adv_cd_file_t);
			cdvdfsv_fssdd.dest=((advl_FSVRpc4Prm *)buf)->addr;
			break; 
		case 0x128:
			DEBUG_PRINTF("PS2ESDL_CDVDFSV: searching for file %s. Struct sz: 0x%x\n", ((adv_FSVRpc4Prm *)buf)->name, size);

			cdvdfsv_srchres.result=sceCdSearchFile((cd_file_t *)&((adv_FSVRpc4Prm *)buf)->fp, ((adv_FSVRpc4Prm *)buf)->name);
			cdvdfsv_fssdd.src=&((adv_FSVRpc4Prm *)buf)->fp;
			cdvdfsv_fssdd.size=sizeof(adv_cd_file_t);
			cdvdfsv_fssdd.dest=((adv_FSVRpc4Prm *)buf)->addr;
			break;
		default:
			DEBUG_PRINTF("PS2ESDL_CDVDFSV: Call from old SCE lib; searching for file %s. Struct sz: 0x%x\n", ((FSVRpc4Prm *)buf)->name, size);

			cdvdfsv_srchres.result=sceCdSearchFile(&((FSVRpc4Prm *)buf)->fp, ((FSVRpc4Prm *)buf)->name);
			cdvdfsv_fssdd.src=&((FSVRpc4Prm *)buf)->fp;
			cdvdfsv_fssdd.size=sizeof(cd_file_t);
			cdvdfsv_fssdd.dest=((FSVRpc4Prm *)buf)->addr;
	}

	cdvdfsv_fssdd.attr=0;

	/* Send the results back to the EE via a DMA transfer. */
	do{
		CpuSuspendIntr(&oldstate);
		dma_id=sceSifSetDma(&cdvdfsv_fssdd,1);
		CpuResumeIntr(oldstate);
	}while(dma_id==0);

	/* while(sceSifDmaStat(dma_id)>=0){}; *//* Wait for the DMA transfer to complete */

	DEBUG_PRINTF("PS2ESDL_CDVDFSV: Search complete!\n");

	return &cdvdfsv_srchres;
}

static void NullNCmdHandler(void *buffer, CDVDReadResult* ReadResult){
	ReadResult->result=0;
	DEBUG_PRINTF("PS2ESDL_CDVDFSV: Unknown N-command received.\n");
}

#define NUM_NCMD_HANDLERS 20
static const NcmdHandler NcmdHandlers[]={
	&cdvdfsv_readee,	/* #01 */
	&NullNCmdHandler,	/* #02 */
	&NullNCmdHandler,	/* #03 */
	&cdvdfsv_gettoc,	/* #04 */
	&cdvdfsv_cdSeek,	/* #05 */
	&cdvdfsv_cdStandby,	/* #06 */
	&cdvdfsv_cdStop,	/* #07 */
	&cdvdfsv_cdPause,	/* #08 */
	&cdvdfsv_sceCdStream,	/* #09 */
	&NullNCmdHandler,	/* #10 */
	&NullNCmdHandler,	/* #11 */
	&cdvdfsv_applyncmd,	/* #12 */
	&cdvdfsv_iopmread,	/* #13 */
	&cdvdfsv_cdDiskReady,	/* #14 */
	&cdvdfsv_readchain,	/* #15 */
	&NullNCmdHandler,	/* #16 */
	&NullNCmdHandler,	/* #17 */
	&NullNCmdHandler,	/* #18 */
	&cdvdfsv_readee,	/* #19 */
};

/* RPC #5 callback */
static void *cbrpc_cdvdncmds(int fno, void *buf, int size){
	static CDVDReadResult crr;

	DEBUG_PRINTF("PS2ESDL_CDVDFSV: N-command requested: 0x%x. size: %d.\n", fno, size);

	if(fno>0 && fno<NUM_NCMD_HANDLERS){
		NcmdHandlers[fno-1](buf, &crr);
	}
	else{
		crr.result=0;
		DEBUG_PRINTF("PS2ESDL_CDVDFSV: Unknown N-command received: 0x%02x\n", fno);
	}

	DEBUG_PRINTF("PS2ESDL_CDVDFSV: N-command completed.\n");

 return &crr;
}

/* RPC 80000596: Power-off synchronization (It waits for CDVDMAN to acknowledge the hardware power-off status). */
static void *cbrpc_80000596(int fno, void *buf, int size){
	DEBUG_PRINTF("PS2ESDL_CDVDFSV: RPC 0x80000596 called. fno: %d; buffer: 0x%p; size: %d.\n", fno, buf, size);
	return NULL;
}

/* Exported entry #5 */
int sceCdChangeThreadPriority(int prio) /* Dummy function */
{
	int result;

	DEBUG_PRINTF("PS2ESDL_CDVDFSV: sceCdChangeThreadPriority() called.\n");

	if((prio - 0x9) < 0x73)
	{
		if(prio==0x9) prio = 0xA;

		/* ChangeThreadPriority(0, 0x8); */

		ChangeThreadPriority(cdvdfsv_ths_ids[0], prio - 1);
		ChangeThreadPriority(cdvdfsv_ths_ids[2], prio);
		ChangeThreadPriority(cdvdfsv_ths_ids[1], prio);
		ChangeThreadPriority(cdvdfsv_ths_ids[3], prio);

		result=0;
	}
	else result=KE_ILLEGAL_PRIORITY;

	return result;
}

