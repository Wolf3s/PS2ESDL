#include "new_cdvdman.h"
#include <intrman.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <sifcmd.h>
#include <sifman.h>

#include "cdvdfsv.h"
#include "cdvdstm.h"

extern int cdvdfsv_def_pri;
extern int cdvdfsv_ths_ids[4];

/* unsigned char cdvdfsv_rtocbuf[TOC_BUFFSIZE]; *//* Buffer to hold disc TOC */

/* Common thread creation data */
extern iop_thread_t thread_data;

/* internal routine */
void cdvdfsv_main_th(void *args){
	if(!sceSifCheckInit()) sceSifInit();
	sceSifInitRpc(0);

	/* *** NOTE!! *** */
	/* The stack sizes for RPC thread 1 and 2 are the same, but thread 3 has a smaller stack size (Because it has less functions bound to it).
	* Piorities are the same for all RPC threads.
	*/

	thread_data.thread=&cdvdfsv_rpc1_th;
	thread_data.priority=cdvdfsv_def_pri;
	thread_data.stacksize=0x1800;
	cdvdfsv_ths_ids[1]=CreateThread(&thread_data);
	StartThread(cdvdfsv_ths_ids[1],0);

	/* Piority, option and stack size are already set correctly */
	thread_data.thread=&cdvdfsv_rpc2_th;
	cdvdfsv_ths_ids[2]=CreateThread(&thread_data);
	StartThread(cdvdfsv_ths_ids[2],0);

	/* Option and piority are now the only properties set correctly */
	thread_data.thread=&cdvdfsv_rpc3_th;
	thread_data.stacksize=0x800;
	cdvdfsv_ths_ids[3]=CreateThread(&thread_data);
	StartThread(cdvdfsv_ths_ids[3],0);

	/* cdvdfsv_poffloop(); */

	ExitDeleteThread();
}

/* internal */
void cdvdfsv_readclock(void* buffer, int size, CDVDCmdResult* rd){
	while(!(rd->result=sceCdReadClock(&rd->buf.rtc))){};
}

/* internal */
void cdvdfsv_disktype(void* buffer, int size, CDVDCmdResult* rd){
	rd->result=sceCdGetDiskType();
}

/* internal */
void cdvdfsv_geterror(void* buffer, int size, CDVDCmdResult* rd){
	rd->result=sceCdGetError();
}

/* internal */
void cdvdfsv_trayreq(void* buffer, int size, CDVDCmdResult* rd){
	while(!(rd->result=sceCdTrayReq(*(int*)buffer, (u32*)&rd->buf.traychk))){};
}

/* internal */
void cdvdfsv_cdri(void* buffer, int size, CDVDCmdResult* rd){
	while(!(rd->result=sceCdRI(&rd->buf.buffer[4], (int*)&rd->buf.buffer[0]))){};
}

/* internal */
void cdvdfsv_applyscmd(void* buffer, int size, CDVDCmdResult* rd){
	sceCdApplySCmd(((unsigned short*)buffer)[0], &((unsigned short*)buffer)[2], ((unsigned short*)buffer)[1], (unsigned char*)rd);
}

/* internal */
void cdvdfsv_cdstatus(void* buffer, int size, CDVDCmdResult* rd){
	rd->result=sceCdStatus();
}

/* internal */
void cdvdfsv_cdBreak(void* buffer, int size, CDVDCmdResult* rd){
	rd->result=sceCdBreak();
}

/* internal */
void cdvdfsv_cdrm(void* buffer, int size, CDVDCmdResult* rd){
	while(!(rd->result=sceCdRM(&rd->buf.buffer[4],(u32*)&rd->buf.buffer[0]))){};
}

/* internal */
void cdvdfsv_poweroff(void* buffer, int size, CDVDCmdResult* rd){
	while(!(rd->result=sceCdPowerOff(&rd->buf.stat))){};
}

/* internal */
void cdvdfsv_cdmmode(void* buffer, int size, CDVDCmdResult* rd){
	if(size!=4 && ((char*)buffer)[4]) return;

	rd->result=sceCdMmode(*(int*)buffer);
}

/* internal */
void cdvdfsv_cdvdfsv5(void* buffer, int size, CDVDCmdResult* rd){
	rd->result=sceCdChangeThreadPriority(*(int*)buffer);
}

/* internal */
void cdvdfsv_cdReadGUID(void* buffer, int size, CDVDCmdResult* rd){
	rd->result=sceCdReadGUID((u64 *)rd->buf.buffer);
}

/* internal */
void cdvdfsv_cdReadModelID(void* buffer, int size, CDVDCmdResult* rd){
	rd->result=sceCdReadModelID((u32 *)rd->buf.buffer);
}

/* internal */
void cdvdfsv_dualinfo(void* buffer, int size, CDVDCmdResult* rd){
	rd->result=sceCdReadDvdDualInfo(&rd->buf.dualinfo.on_dual,(u32 *)&rd->buf.dualinfo.layer1_start);
}

/* internal routine */
void cdvdfsv_readee(void *buf, CDVDReadResult *rd){
	/* CDVDFSV actually does a read of 8 sectors MAX, then does a DMA transfer */
	/* However, I've shifted this functionality into CDVDMAN for performance */

	/* !! Note: sceCdApplyNCmd() will perform a mode 4 synchronization after the N-command is sent. */

	rd->result=sceCdApplyNCmd(ASYNC_CMD_READEE, buf, sizeof(struct cdvdfsv_ee_read_params));
}

/* internal */
void cdvdfsv_gettoc(void *buf, CDVDReadResult *rd)
{
	/* int oldstate;
	register int id; */

	DEBUG_PRINTF("CDVDFSV: GetToc call requested\n");
	/* rd->result=sceCdGetToc(cdvdfsv_rtocbuf); */
	rd->result=1; /* CDVDMAN does not support returning a dummy TOC entry yet. Even most IOPRP images just return a value */

	rd->on_dvd=(sceCdGetDiskType()==0x14)?1:0; /* Set "on_dvd" to 1 if the disc type is PS2DVD */
}

/* internal routine */
void cdvdfsv_cdSeek(void *buf, CDVDReadResult *ReadResult){
	ReadResult->result = sceCdSeek(*(int*)buf);
	sceCdSync(6);
}

/* internal routine */
void cdvdfsv_cdStandby(void *buf, CDVDReadResult *ReadResult){
	ReadResult->result = sceCdStandby();
	sceCdSync(4);
}

/* internal routine */
void cdvdfsv_cdStop(void *buf, CDVDReadResult *ReadResult){
	ReadResult->result = sceCdStop();
	sceCdSync(4);
}

/* internal routine */
void cdvdfsv_cdPause(void *buf, CDVDReadResult *ReadResult){
	ReadResult->result = sceCdPause();
	sceCdSync(6);
}

/* internal */
void cdvdfsv_applyncmd(void *buf, CDVDReadResult *rd){
	rd->result=sceCdApplyNCmd(((struct cdvdfsv_ee_Ncmd_params *)buf)->command, ((struct cdvdfsv_ee_Ncmd_params *)buf)->ndata, ((struct cdvdfsv_ee_Ncmd_params *)buf)->ndlen);
	/* sceCdSync(2); *//* CDVDMAN will call sceCdSync() */
}

/* internal routine */
void cdvdfsv_iopmread(void *buf, CDVDReadResult *rd){
	DEBUG_PRINTF("PS2ESDL_CDVDFSV scdCdReadIOPm called. Read address= 0x%p; LSN= %lu; Sectors: %lu\n", ((struct cdvdfsv_ee_read_params *)buf)->buffer, ((struct cdvdfsv_ee_read_params *)buf)->lsn, ((struct cdvdfsv_ee_read_params *)buf)->sectors);

	rd->result=(sceCdReadIOPm(((struct cdvdfsv_ee_read_params *)buf)->lsn, ((struct cdvdfsv_ee_read_params *)buf)->sectors, ((struct cdvdfsv_ee_read_params *)buf)->buffer, ((struct cdvdfsv_ee_read_params *)buf)->read_mode)==0)?0:1;
	sceCdSync(0);
}

/* internal routine */
void cdvdfsv_readchain(void *buf, CDVDReadResult *rd)
{
	DEBUG_PRINTF("PS2ESDL_CDVDFSV: sceCdReadChain() called. buffer: 0x%p\n", buf);
	rd->result=sceCdReadChain(buf, NULL);
}

/* internal routine */
void cdvdfsv_cdDiskReady(void *buf, CDVDReadResult *ReadResult){
	ReadResult->result = sceCdDiskReady(1);
}

/* internal routine */
void cdvdfsv_sceCdStream(void *buf, CDVDReadResult *rd){
	DEBUG_PRINTF("PS2ESDL_CDVDFSV: sceCdStream() called. Command: 0x%lx; arg1: 0x%lx; arg2: 0x%lx; Buffer address 0x%p\n",((struct RPC_stream_cmd*)buf)->cmd,((struct RPC_stream_cmd*)buf)->lsn,((struct RPC_stream_cmd*)buf)->nsectors,((struct RPC_stream_cmd*)buf)->buffer);

	switch(((struct RPC_stream_cmd*)buf)->cmd){
		case CDVD_ST_CMD_START:
			rd->result=sceCdStStart(((struct RPC_stream_cmd*)buf)->lsn,&((struct RPC_stream_cmd*)buf)->mode);
			break;
		case CDVD_ST_CMD_READ:
			rd->result=sceCdStRead(((struct RPC_stream_cmd*)buf)->nsectors, ((struct RPC_stream_cmd*)buf)->buffer, CDVD_STREAM_NONBLOCK|ASYNC_CMD_READEE, NULL);
			break;
		case CDVD_ST_CMD_STOP:
			rd->result=sceCdStStop();
			break;
		case CDVD_ST_CMD_INIT:
			rd->result=sceCdStInit(((struct RPC_stream_init*)buf)->bufmax,((struct RPC_stream_init*)buf)->bankmax,((struct RPC_stream_init*)buf)->buffer);
			break;
		case CDVD_ST_CMD_STAT:
			rd->result=sceCdStStat();
			break;
		case CDVD_ST_CMD_PAUSE:
			rd->result=sceCdStPause();
			break;
		case CDVD_ST_CMD_RESUME:
			rd->result=sceCdStResume();
			break;
		case CDVD_ST_CMD_SEEK:
		case CDVD_ST_CMD_SEEKF:
			rd->result=sceCdStSeek(((struct RPC_stream_cmd*)buf)->lsn);
			break;
		default:
			DEBUG_PRINTF("PS2ESDL_CDVDFSV: Warning! Unsupported streaming command!.\n");
			rd->result=0;
	}
}

/* Dummy function for the export table */
void dummy_function(void){}

