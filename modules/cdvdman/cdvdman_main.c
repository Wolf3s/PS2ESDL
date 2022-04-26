#include "new_cdvdman.h"
#include <dmacman.h>
#include <intrman.h>
#include <loadcore.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>
#include <thevent.h>

#include "new_ioman.h"

#include "cdvdman_hdr.h"
#include "cdvdstm.h"

/* For internal asynchronous function */
struct cdvdman_async async_config;

/* Data */
unsigned char *cdvdman_fsvrbuf; /* Buffer for processed sector data */

/* Synchronization event flags */
extern int cdvdman_intr_ef;
extern int cdvdman_ncmd_sema;

/* Callback pointers */
void (*cdvdman_user_cb)(int)=NULL; /* set by sceCdCallback */

void (*cdvdman_poff_cb)(void *)=NULL;   /* Power-off callback handler */
void *cdvdman_poffarg=NULL;             /* Pointer to a argument that's to be passed to the Power-off callback handler */

int cdvdman_cmdfunc=0; /* ID of function that sent the N-command */

struct cdvd_cmdstruct cdvdman_cmdstruct;

/* CDVDMAN emulation configuration */
extern struct cdvdman_config cdvdman_conf;

/* Exported entry #4 */
/* Initializes or shuts down the CDVDMAN module. */
int sceCdInit(int init_mode)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdInit called: mode %x\n",init_mode);

	if(init_mode!=SCECdEXIT)
	{
	        DEBUG_PRINTF("PS2ESDL_CDVDMAN: CDVDMAN Initilizing...\n");
		cdvdman_init();
	}
	else
	{
		cdvdman_deinit();
	}

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdInit call completed.\n");
	return 1; /* Return !=0 on success; 0 on failure */
}

/* Exported entry #5 */
/* Makes the C/DVD drive ready to operate. */
int sceCdStandby(void)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStandby called\n");

	return((send_asynccmd(ASYNC_CMD_STANDBY,NULL, 0, SCECdFuncStandby, NULL)<0)?0:1); /* Return !=0 on success; 0 on failure */
}

/* Exported entry #6 */
/* Reads sectors from CD or DVD disc. */
int sceCdRead(unsigned int lsn, unsigned int sectors, void *buf, cd_read_mode_t *mode)
{
	int result;
	iop_event_info_t evf_info;
	struct read_n_command_struct read_command;

	sceCdSync(0);

	refer_ef_status(cdvdman_intr_ef, &evf_info);
	if(!(evf_info.currBits & 1))	/* In case sceCdSync() doesn't prevent the IOP from reaching here while CDVDMAN is still reading data... */
	{
		DEBUG_PRINTF("PS2ESL_CDVDMAN: Warning! Attempted sceCdRead() call while system is busy!\n");
		return 0;
	}

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdRead0 called. LSN: %u, Sectors: %u\n", lsn, sectors);

	read_command.lsn=lsn;
	read_command.sectors=sectors;

	result=send_asynccmd(ASYNC_CMD_READ, (int *)&read_command, sizeof(struct read_n_command_struct), SCECdFuncRead, buf)<0?0:sectors;

	return((result>0)?1:0);
}

/* Exported entry #7 */
/* The "seek" N-Command tells to the C/DVD to move to specified sector of disc. */
int sceCdSeek(unsigned int lsn)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdSeek called; lsn: %u\n",lsn);

	return((send_asynccmd(ASYNC_CMD_SEEK, (int *)&lsn, 4, SCECdFuncSeek, NULL)<0)?0:1); /* Return !=0 on success; 0 on failure */
}

/* Exported entry #8 */
/* Returns the last error code. */
int sceCdGetError(void)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdGetError called. Last error code: 0x%x\n",cdvdman_cmdstruct.cdvdman_cderror);
	return(cdvdman_cmdstruct.cdvdman_cderror);
}

/* Exported entry #10 */
/* Retrieves basic information about a file on CD or DVD media. */
int sceCdSearchFile(cd_file_t *fp,const char *name)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdSearchFile called: filename: %s. Redirecting to sceCdLayerSearchFile().\n",name);

	return(sceCdLayerSearchFile((adv_cd_file_t *)fp, name, 0)); /* Return !=0 on success; 0 on failure */
}

/* Exported entry #11 */
int sceCdSync(int mode)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdSync called. mode: 0x%x\n",mode);

	if(cdvdman_conf.operation_mode&SYS_FORCESYN) mode=0;

	switch(mode)
	{
		case 17:	/* Mode 17 seems to only be used during streaming, and few games use it. */
		case 1:
			if(!cdvdman_cmdstruct.cdvdman_waf) return 1;
			break;

	/* Other than the 2 cases above, it seems like all other synhronization modes
		will wait for the CD/DVD hardware to become ready before returning.

		*Cases 3 and 6 are the same
		*Case 3 and 6 will issue a break() N-command if a fixed timeout expired.
			Since the CD/DVD sytem is emulated, there *won't* be any read timeouts
		*Case 4 would do the same as case 3 or 6, but with a different timeout value
		*Case 5 is the same as case 0, but does not check the "cdvdman_read2_flg" flag
			(This check is quite redundant as PS2ESDL's sending of N-commands is almost instantaneous due to the way it emulates the CD/DVD hardware)
		*Case 16 and 17 are the same, but case 16 will wait for the drive's activity to complete, blocking the command flow.
		*Cases 16 and 17 are the same as cases 0 and 1, but both check for cdvdman_EE_ncmd and cdvdman_cmdstruct.cdvdman_strm_id
		*Case 32 waits for end-of-N-command or DMA CH.3 transfer, then tampers with the CD/DVD interrupt event flag depending on it's current state

		But because of the way PS2ESDL emulates the CD/DVD hardware, the end-of-DMA flag is usually set at the same time as the completion-of-N-command flag
	*/
	default: /* Case 0 is the default. */
		while(!cdvdman_cmdstruct.cdvdman_waf) WaitEventFlag(cdvdman_intr_ef,1,WEF_AND,NULL);
 }

 /* Returns 0 if previously sent N-Command is completed, non-zero if the execution of N-Command is in progress.*/
 return 0;
}

/* Exported entry #12 */
int sceCdGetDiskType(void)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdGetDiskType called: Disc type: 0x%x\n",cdvdman_conf.cdvdman_mmode);
	return cdvdman_conf.cdvdman_mmode;
}

/* Exported entry #13 */
int sceCdDiskReady(int mode)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdGetDiskReady called; Mode: %d, Current CD/DVD system status: %d\n", mode, async_config.status);

	sceCdSync(mode);

	return(async_config.status);
}

/* Exported entry #14 */
int sceCdTrayReq(int mode, unsigned int *traycnt)
{
	static int TrayState=1;	/* The first call to sceCdTrayReq() will always return 1 as the tray status on the first call, and then the correct state on the 2nd call. */

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdTrayReq called: mode: %d\n",mode);

	if(mode==SCECdTrayCheck){
		*traycnt=TrayState;	/* NOTE!! Return 0 if the tray is retracted, and 1 if it's ejected */
		TrayState=0;		/* Set the tray state to "tray retracted" (See the first comment for this function). */
	}
	else { /* mode!=SCECdTrayCheck */
		*traycnt=mode; /* same as "*traycnt=(mode==SCECdTrayOpen)?SCECdTrayOpen:SCECdTrayClose;" */
	}

	return 1;
}

/* Exported entry #15 */
int sceCdStop(void)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStop called!\n");

	return((send_asynccmd(ASYNC_CMD_STOP, NULL, 0, SCECdFuncStop, NULL)<0)?0:1); /* Returns 0 on failure, otherwise non-zero */
}

/* Exported entry #16 */
int sceCdPosToInt(cd_location_t *p)
{
	register int result;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdPosToInt called!\n");

	result = ((unsigned int)p->minute >> 16) *  10 + ((unsigned int)p->minute & 0xF);
	result *= 60;
	result += ((unsigned int)p->second >> 16) * 10 + ((unsigned int)p->second & 0xF);
	result *= 75;
	result += ((unsigned int)p->sector >> 16) *  10 + ((unsigned int)p->sector & 0xF);
	result -= 150;

	return result;
}

/* Exported entry #17 */
cd_location_t *sceCdIntToPos(int i,cd_location_t *p)
{
	register int sc, se, mi;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdIntToPos called!\n");
	i += 150;
	se = i / 75;
	sc = i - se * 75;
	mi = se / 60;
	se = se - mi * 60;
	p->sector = (sc - (sc / 10) * 10) + (sc / 10) * 16;
	p->second = (se / 10) * 16 + se - (se / 10) * 10;
	p->minute = (mi / 10) * 16 + mi - (mi / 10) * 10;

	return p;
}

/* Exported entry #21 */
int sceCdCheckCmd(void)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdCheckCmd called! WAF: %d\n",cdvdman_cmdstruct.cdvdman_waf);
	return(cdvdman_cmdstruct.cdvdman_waf);
}

/* Exported entry #22 */
int sceCdRI(u8 *buf, int *stat)
{
	int r;
	unsigned char rdata[9];

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: scesceCdRI called!\n");

	r=cdvdman_send_scmd(0x12, NULL, 0, rdata, 9);
	*stat=rdata[0];

	/* copy rdata 1-8 to arg1 0-7 */
	memcpy(buf,&rdata[1],8);

	return r;
}

/* Exported entry #24 */
int sceCdReadClock(cd_clock_t *rtc)
{
	int ret;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdReadClock called!\n");

	/* There was some code after sending the S-command, to return a dummy clock value if there was a problem reading the clock from the Mechacon. */
	ret=cdvdman_send_scmd(8, NULL, 0, (unsigned char*)rtc, 8);
	rtc->month&=0x7F;
	rtc->padding=0;

	return ret;
}

/* Exported entry #28 */
int sceCdStatus(void)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStatus called. Drive status: 0x%02x\n", async_config.drivestate);

	if(cdvdman_conf.operation_mode&SYS_FORCESYN) sceCdSync(0);

	return async_config.drivestate;
}

/* Exported entry #29 */
int sceCdApplySCmd(u8 cmd,void *wdata,unsigned int wdlen,void *rdata)
{
	register unsigned int i;
	int result;

	for(i=0;i<=2500;i++) /* Retry a maximum of 2500 times... */
	{
		result=cdvdman_send_scmd(cmd, wdata, wdlen, rdata, 0x10);
		DelayThread(2000);

		if(result!=0) return 1;
	}
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Warning! S-command send timeout!\n"); 

	return 0; /* Return 0 on failure */
}

/* Exported entry #37 */
void *sceCdCallback(void(*func)(int))
{
	void *oldcbptr;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdCallback called!\n");

	if(sceCdSync(1)) return 0;
	WaitSema(cdvdman_ncmd_sema);

	oldcbptr=cdvdman_user_cb;
	cdvdman_user_cb=func;
	SignalSema(cdvdman_ncmd_sema);

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdCallback request completed.\n");
	return oldcbptr;
}

/* Exported entry #38 */
int sceCdPause(void)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdPause called!\n");

	return(((send_asynccmd(ASYNC_CMD_PAUSE, NULL, 0, SCECdFuncPause, NULL))<0)?0:1);
}

/* Exported entry #39 */
int sceCdBreak(void)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdBreak called!\n");

	WaitSema(cdvdman_ncmd_sema);

	async_config.asyncCmd=ASYNC_CMD_ABORT;
	cdvdman_cmdstruct.cdvdman_cderror=SCECdErABRT;

	SignalSema(cdvdman_ncmd_sema);

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdBreak request completed.\n");
	return 1;
}

/* Exported entry #41 */
int sceCdReadConsoleID(u64 *id,unsigned int *stat)
{
	unsigned char rdata[9], sdata[4];
	int r;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdReadConsoleID called!\n");

	sdata[0]=45;

	r=cdvdman_send_scmd(3, sdata, 1, rdata, 9);

	*stat = rdata[0];
	memcpy((unsigned char*)&id,&rdata[1],8); /* Copy 64-bit console ID from rdata[1] */

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdReadConsoleID request completed.\n");
	return r;
}

/* Exported entry #44 */
unsigned int sceCdGetReadPos(void)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdGetReadPos called: Current read buffer offset: %u\n",async_config.rbuffoffset);
	return async_config.rbuffoffset;
}

/* Exported entry #45 */
int sceCdCtrlADout(int param,int *stat)
{
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdCtrlADout called: Param: 0x%x\n",param);

	*stat = 0;
	return(cdvdman_send_scmd(0x14, (unsigned char*)&param, 1, (unsigned char*)stat, 1));
}

/* Exported entry #50 */
/* Used by cdvdfsv & cdvdstm */
int sceCdSC(int code,int *param){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdSc() called. Code: 0x%x\n",code);

	switch(code){
		case 0xFFFFFFF7:		/* Returns version of CDVDMAN */
			return CDVDMAN_VERSION;	/* Used by cdvdfsv */
		default:
			return 0;
	}
}

/* Exported entry #54 */
int sceCdApplyNCmd(int ncmd,void *ndata, int ndlen){
	int rc;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: N-command request=0x%x, waiting for previous N-command to complete...\n",ncmd);

	rc=(send_asynccmd(ncmd, ndata, ndlen, 0, NULL)<0)?0:1;

	sceCdSync(4);

	return rc;
}

/* Exported entry #56 */
int sceCdStInit(unsigned int bufmax,unsigned int bankmax,void* iop_bufaddr){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStInit called. bufmax: %u, bankmax: %u; Buffer address: 0x%p\n", bufmax, bankmax, iop_bufaddr);

	async_config.sectorsstreamed=bufmax;
	async_config.max_bank_sectors=bufmax/bankmax;

	async_config.stbufmax=async_config.max_bank_sectors*bankmax; /* By right we should just take specified bufmax value, but some games provide a bufmax value that can't be divided by the bankmax value. :( */
	async_config.stbankmax=bankmax;
	async_config.stReadBuffer=iop_bufaddr;

	stInit(0); /* The LSN to start streaming from is currently unknown. */
	return 1;
}

/* Exported entry #57 */
int sceCdStRead(unsigned int sectors, void *buf, unsigned int mode, unsigned int *err){
	unsigned int sectorstoread, sectors_left_to_read, bytesToRead, sectorsToRefill, bytesPerBank;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStRead called. Sectors: %u; Buffer address: 0x%p; Mode: 0x%x\n",sectors,buf,mode);

	if(err!=NULL) *err=sceCdGetError();

	sectors_left_to_read=sectors;
	bytesPerBank=async_config.max_bank_sectors*2048;

	while(sectors_left_to_read>0){
		sectorstoread=(sectors_left_to_read>(async_config.max_bank_sectors-async_config.ringbuff_bank_sector_offset))?(async_config.max_bank_sectors-async_config.ringbuff_bank_sector_offset):sectors_left_to_read;
		bytesToRead=sectorstoread*2048;

		if(async_config.stbankmax<2) sceCdSync(0); /* Synchronize reading when the ring buffer "warps" around, if there is only one bank. */

		if(mode&ASYNC_CMD_READEE) EE_memcpy(buf, &async_config.ringbuff_bank_ptr[async_config.ringbuff_bank_sector_offset*2048], bytesToRead);
		else memcpy(buf, &async_config.ringbuff_bank_ptr[async_config.ringbuff_bank_sector_offset*2048], bytesToRead);

		async_config.ringbuff_bank_sector_offset+=sectorstoread;
		sectors_left_to_read-=sectorstoread;
		buf+=bytesToRead;

		/* If the ring buffer becomes empty (All sectors have been read), refill it. */
		if(async_config.ringbuff_bank_sector_offset>=async_config.max_bank_sectors){
			/* if(mode&ASYNC_CMD_READEE) EE_DMASync(); *//* We'll probably not need to wait for the DMA transfer to end... */

			/******************************************************************************************************************/
			/* !!!!!!REMEMBER!!!: We're reading ahead (By about 1 whole ring buffer worth of sectors: async_config.stbufmax). */
			/*  async_config.stLSN has already been incremented by sceCdStStart().						  */
			/******************************************************************************************************************/

			if(async_config.bank_ID&1){ /* Read 2 banks worth of sectors, so skip every alternate refill. */
				sectorsToRefill=async_config.max_bank_sectors<<1; /* async_config.max_bank_sectors*2 */

				/* Read 2 banks worth of data, only when 2 banks empty out. */
				/* We want to refill the current bank and the previous one together (So shift the pointer back by one bank). */
				stReadRingBuffer(async_config.stLSN, sectorsToRefill, async_config.ringbuff_bank_ptr-bytesPerBank);

				/* DEBUG_PRINTF("Refilling 2 banks at bank %d.\n", async_config.bank_ID); *//* For debugging this function only. */
			}
			else{
				if((async_config.stbankmax-async_config.bank_ID)==1){
					/* Only refill a single bank worth of sectors if we're at the end of the ring buffer (So that we don't write past the end of it). */
					sectorsToRefill=async_config.max_bank_sectors;
					stReadRingBuffer(async_config.stLSN, sectorsToRefill, async_config.ringbuff_bank_ptr); /* Otherwise only read 1 bank worth of data. */
					/* DEBUG_PRINTF("Refilling 1 bank at bank %d.\n", async_config.bank_ID); *//* For debugging this function only. */
				}
				else{
					sectorsToRefill=0;
					/* DEBUG_PRINTF("Not refilling any banks at bank %d.\n", async_config.bank_ID); *//* For debugging this function only. */
				}
			}

			async_config.bank_ID++;
			async_config.stLSN+=sectorsToRefill;
			if(async_config.bank_ID>=async_config.stbankmax) async_config.bank_ID=0;

			async_config.ringbuff_bank_ptr=&((unsigned char *)async_config.stReadBuffer)[async_config.bank_ID*bytesPerBank];
			async_config.ringbuff_bank_sector_offset=0;
		}
	}

	/* EE_DMASync(); *//* We'll probably not need to wait for the DMA transfer to end... */

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStRead() call ended.\n");
	return sectors;
}

/* Exported entry #59 */
int sceCdStStart(unsigned int lsn,cd_read_mode_t *mode){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStStart called: LSN: 0x%x.\n",lsn);

	stInit(lsn);
	stReadRingBuffer(lsn, async_config.stbufmax, async_config.stReadBuffer);
	async_config.stLSN+=async_config.stbufmax;
	sceCdSync(0);

	return 1;
}

/* Exported entry #60 */
int sceCdStStat(void){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN sceCdStStat called! Sectors streamed: %u\n",async_config.sectorsstreamed);
	return async_config.sectorsstreamed;
}

/* Exported entry #61 */
int sceCdStStop(void){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStStop called!\n");

	sceCdSync(0);
	return 1;
}

/* Exported entry #66 */
int sceCdReadChain(CdvdChain_t *read_tag, cd_read_mode_t *mode)
{
	struct cdvdfsv_ee_read_params EE_rd_cmd;
	unsigned int i;

	/* Missing from CDVDMAN in rom0: and IOPRP images, but available in rom0:XCDVDMAN and rom1:CDVDMAN */
	/* But since CDVDFSV has to have this functionality, and it's currently located in CDVDMAN, why not just leave it here? */

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdReadChain called!\n");

	/* NOTE!! There SHOULD be error checking code in case something goes wrong while doing chain reads.
		But in reality, sceCdReadChain() will be unable to determine whether the CD/DVD drive failed to read sectors, only whether the sending of the read N-command had succeeded. */
	for(i=0; ((read_tag[i].sectorLoc!=0xFFFFFFFF)&&(read_tag[i].numSectors!=0xFFFFFFFF)&&(read_tag[i].buffer!=0xFFFFFFFF)); i++){
		if(read_tag[i].buffer&1){ /* IOP RAM; The last bit is set, and must be filed off first. */
			cdvdman_read_noCB(read_tag[i].sectorLoc, read_tag[i].numSectors, (void*)((unsigned int)read_tag[i].buffer&(unsigned int)0xFFFFFFFE), 0);
		}
		else{ /* EE RAM */
			EE_rd_cmd.lsn=read_tag[i].sectorLoc;
	       	 	EE_rd_cmd.sectors=read_tag[i].numSectors;
			EE_rd_cmd.buffer=(void *)read_tag[i].buffer;

			send_asynccmd(ASYNC_CMD_READEE, (int *)&EE_rd_cmd, sizeof(struct cdvdfsv_ee_read_params), 0, NULL);
		}

		sceCdSync(0); /* Synchronize reading. */
	}

	return 1; /* Returns 1 in most IOP images */
}

/* Exported entry #67 */
int sceCdStPause(void){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStPause called!\n");
	return 1;
}

/* Exported entry #68 */
int sceCdStResume(void){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdResume called!\n");
	return 1;
}

/* Exported entry #73 (Available in XCDVDMAN) */
int sceCdCancelPOffRdy(int *stat){
	unsigned char dummy;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdCancelPOffRdy called!\n");

	return(cdvdman_send_scmd(0x1B, &dummy, 0, (unsigned char*)stat, 1));
}

/* Exported entry #74 */
int sceCdPowerOff(int *stat){
	unsigned char dummy;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdPowerOff Called.\n");

	return(cdvdman_send_scmd(0xF, &dummy, 0, (unsigned char*)stat, 1));
}

/* Exported entry #77 */
int sceCdStSeekF(unsigned int lsn){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStSeekF called: LSN: 0x%x\n",lsn);

	stInit(lsn);
	sceCdStStart(lsn, NULL);

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdStSeekF request completed.\n");

	return 1;
}

/* Exported entry #78 */
void *sceCdPOffCallback(void(*func)(void *), void *addr){
	void *old_cb;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdPOffCallback called!\n");

	old_cb=cdvdman_poff_cb;
	cdvdman_poff_cb = func;
	cdvdman_poffarg = addr;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdPOffCallback request completed.\n");

	return old_cb;
}

/* Exported entry #79 DNAS */
int sceCdReadDiskID(unsigned int *id){
	memcpy(id, cdvdman_conf.dnas_key, 5); /* Copy the DNAS ID */
	return 1;
}

/* Exported entry #80 DNAS */
int sceCdReadGUID(u64 *guid){
	return(cdvdman_readConID(guid, NULL));
}

/* Exported entry #82 DNAS */
int sceCdReadModelID(unsigned int *id){
	return(cdvdman_readConID(NULL, id));
}

/* Exported entry #83 */
int sceCdReadDvdDualInfo(int *on_dual, unsigned int *layer1_start){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdReadDvdDualInfo() called.\n");

	*on_dual=DvdDual_infochk();
	*layer1_start=cdvdman_cmdstruct.cdvdman_layer1;
	return 1;
}

/* Exported entry #84 */
int sceCdLayerSearchFile(adv_cd_file_t *fp, const char *path, int layer){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: sceCdLayerSearchFile called! Path: %s, Layer: %d.\n",path,layer);
	return cdvdman_CdSearchFile((cd_file_t *)fp, path, layer, NULL);
}

