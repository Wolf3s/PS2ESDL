#include "new_cdvdman.h"
#include <errno.h>
#include <intrman.h>
#include "new_ioman.h"
#include <loadcore.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <sys/stat.h>

#include "cdvdman_hdr.h"
#include "sector_io.h"

/* For internal asynchronous function */
extern struct cdvdman_async async_config;

/* Variables declared in "cdvdmanc.c" */
extern unsigned char *cdvdman_fsvrbuf;

/* Local variables */
char startup_init=1; /* Whether the module has already been started-up(0) or is now being started-up(1) */

/* CDVDMAN event flags */
int cdvdman_intr_ef;
int cdvdman_scmd_sema;
int cdvdman_ncmd_sema;
int cdvdman_srch_sema;

/* Function prototypes */
static inline void init_DVD9(void);
static int isLSNValid(unsigned int lsn);
static int isfilename(const char *path);
static void stripdirfilename(const char *path,unsigned char *target,unsigned int *extern_ptr);

extern void (*cdvdman_user_cb)(int);	/* set by sceCdCallback */

extern void (*cdvdman_poff_cb)(void *); /* Power-off callback handler */
extern void *cdvdman_poffarg;           /* Pointer to a argument that's to be passed to the Power-off callback handler */

extern volatile int cdvdman_cmdfunc;

extern struct cdvd_cmdstruct cdvdman_cmdstruct;

/* CDVDMAN emulation configuration */
extern struct cdvdman_config cdvdman_conf;

/* Thread creation data for the asynchronous disc events thread */
static iop_thread_t asyncthread={
	TH_C,			/* attr */
	0,			/* option */
	&cdvd_asyncthread,	/* thread */
	0x300,			/* stacksize */
	1			/* priority */
};

/*******************************************************************
Initilizes CDVDMAN and all related data structures.
********************************************************************/
void cdvdman_init(void){
	async_config.status=SCECdNotReady;

	DEBUG_PRINTF(	"PS2ESDL_CDVDMAN: Initializing...\n"
			"PS2ESDL_CDVDMAN: ===============\n\n");

	cdvdman_initcfg();

	if(startup_init){
		DEBUG_PRINTF("PS2ESDL_CDVDMAN: Performing cold startup.\n");

		/* Create asynchronous thread */
		async_config.asyncthreadID=CreateThread(&asyncthread);
		StartThread(async_config.asyncthreadID, NULL);

		DEBUG_PRINTF("PS2ESDL_CDVDMAN: Asynchronous thread created and started.\n");

		device_init();

		startup_init=0;
	}

	init_DVD9();

	/* Set all synchronisation flags */
	SetEventFlag(cdvdman_intr_ef, 1);
	SignalSema(cdvdman_scmd_sema);
	SignalSema(cdvdman_ncmd_sema);

	async_config.drivestate=SCECdStatPause;
	async_config.status=SCECdComplete; /* Initialization operation completed successfully */

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Initialization complete!\n");
}

/*******************************************************************
De-initilizes CDVDMAN and closes all open files
********************************************************************/
void cdvdman_deinit(void){
	/* register unsigned char i; */

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Exit request received.\n");

	if(startup_init==0) sceCdSync(0); /* Wait for all drive operations to end */

	/* async_config.status=SCECdNotReady; *//* Drive is now "off-line" */
	/* async_config.drivestate=SCECdStatStop; *//* Disc is not spinning */

	/* if(async_config.asyncthreadID>=0){
		DEBUG_PRINTF("PS2ESDL_CDVDMAN: Stopping asynchronous thread.\n");

		TerminateThread(async_config.asyncthreadID);
		DeleteThread(async_config.asyncthreadID); *//* Stop the asynchronous thread, if it exists */
		/* async_config.asyncthreadID=-1;
	}

	if(startup_init==0){
		startup_init=1;
		device_deinit();
	} */

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Shut down complete.\n");
}

/*******************************************************************
Initilizes all of CDVDMAN's data structures and event flags.
********************************************************************/
void cdvdman_initcfg(void){
	if(startup_init==0){
		DEBUG_PRINTF("PS2ESDL_CDVDMAN: Synchronizing virtual CD/DVD hardware...\n");
		sceCdSync(0); /* Wait for all CD/DVD processes to end. */

		WaitSema(cdvdman_ncmd_sema); /* Indicate that the virtual CD/DVD hardware is busy */
	}

	/* Clear all pointers to callback functions */
	cdvdman_user_cb=NULL;
	cdvdman_poff_cb=NULL;
	cdvdman_cmdfunc=0;

	/* Initilize CDVDMAN's shared data structure */
	memset(&cdvdman_cmdstruct, 0, sizeof(struct cdvd_cmdstruct));	/* Flood-fill with 0s */
	memset(async_config.ncmd_buf, 0, sizeof(async_config.ncmd_buf));

	cdvdman_cmdstruct.cdvdman_cderror=SCECdErNO; /* No error has occcured */
	cdvdman_cmdstruct.cdvdman_waf=1;
	cdvdman_cmdstruct.cdvdman_dvdflag=(cdvdman_conf.cdvdman_mmode==SCECdPS2DVD)?1:0; /* Set to 1 only if the disc is a DVD */
	/* cdvdman_cmdstruct.cdvdman_scmd_flg=1; */

	/* Set other internal variables */
	async_config.asyncCmd=ASYNC_CMD_NOP;
	async_config.drivestate=SCECdStatPause;        /* No ongoing reading activity. */
	async_config.EE_dmatrans_id=0;                 /* No ongoing DMA transfers. */
	async_config.status=SCECdNotReady; 		/* The drive is not ready to receive commands. */
}

/*******************************************************************
DVD9 emulation functions
********************************************************************/
/* Translates "lsn" for DVD9 emulation (internal) */
unsigned int sceCdLsnDualChg(unsigned int lsn) /* Recalculates LSN of a specified sector that exists on layer 1 of a DL disc, for "flat" addressing. */
{
	return(cdvdman_conf.layerBreak+lsn);
}

/* Returns 1 if DVD is a DL DVD (Either a OTP or PTP) */
int DvdDual_infochk(void)
{
	return((cdvdman_cmdstruct.cdvdman_dldvd)?1:0);
}

/* Initilizes the 2nd layer's information (If that layer exists). */
static inline void init_DVD9(void){
	/* 0xFF->Uninitilized; 0x00->Not a DL DVD; 0x01->OTP DL DVD; 0x02->PTP DL DVD */
	/* PS2 DVD9 discs are PTP DVD-DL discs. */

	cdvdman_cmdstruct.cdvdman_dldvd=(cdvdman_conf.layerBreak!=0)?2:0;
	cdvdman_cmdstruct.cdvdman_layer1=cdvdman_conf.layerBreak;
}

/*******************************************************************
Common sector read() function (Blocking).
Called by all blocking CDVDMAN functions
********************************************************************/
void cdvdman_read_noCB(unsigned int lsn, unsigned int sectors, void *buf, unsigned char mode){
	struct read_n_command_struct read_command;

	read_command.lsn=lsn;
	read_command.sectors=sectors;

	sceCdSync(0);
	send_asynccmd(ASYNC_CMD_READ, (int *)&read_command, sizeof(struct read_n_command_struct), 0, buf);

	if(mode) sceCdSync(0);
}

/*******************************************************************
Asynchronous thread command function.
Formats, and sends commands to the asynchronous thread.
Returns 0 on success, negative on failure.
********************************************************************/
int send_asynccmd(int ncmd, int *ndata, int ndlen, int cmdfunc, void *buffer){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC C&C: ASYNC command received. Command: 0x%x; length: %d; cmdfunc: 0x%x\n", ncmd, ndlen, cmdfunc);

	if(startup_init==1){	/* In case some game, or the PS2 BIOS attempts to access the CD/DVD drive without initilizing it. */
		cdvdman_init();
	}

	WaitSema(cdvdman_ncmd_sema);
#ifdef DEBUG_TTY_FEEDBACK 
	if(cdvdman_cmdstruct.cdvdman_waf==0) DEBUG_PRINTF("Warning! It's possible that the interrupt callback has not completed!\n");
#endif

	cdvdman_cmdfunc=cmdfunc; /* Mark which was the calling function, if specified */
	cdvdman_cmdstruct.cdvdman_cderror=SCECdErNO;
	cdvdman_cmdstruct.cdvdman_waf=0;
	cdvdman_cmdstruct.cdvdman_rd_buf=buffer;

	memcpy(async_config.ncmd_buf, ndata, ndlen); 

	async_config.asyncCmd=ncmd; /* Set the asynchronous thread to read */
	async_config.flags=0;          /* Clear all flags */

	switch(ncmd){
		case ASYNC_CMD_NOP_SYNC:
		case ASYNC_CMD_NOP:
			break;
		case ASYNC_CMD_STANDBY:
		case ASYNC_CMD_STOP:
		case ASYNC_CMD_PAUSE:
		case ASYNC_CMD_SEEK:
			break;
		case ASYNC_CMD_READEE:   /* This is a special N-command that is unique to PS2ESDL. */
			async_config.flags=SYS_EE_RAM_DEST;

			cdvdman_cmdstruct.cdvdman_rd_buf=((struct cdvdfsv_ee_read_params*)ndata)->buffer; /* Buffer to receive data in EE memory */
		/* *** Fall through. *** */
		case ASYNC_CMD_READ:
	case ASYNC_CMD_READDVD:
		clear_ev_flag(cdvdman_intr_ef,0xFFFFFFFE);
		break;
	default:      /* NOTE!! Anything unlisted goes here as "unsupported" */
		DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC C&C: Warning! Unsupported N-command: 0x%x\n", ncmd);
		return -1;
	}

	wakeup_thread(async_config.asyncthreadID);

	SignalSema(cdvdman_ncmd_sema);

	return 0;
}

/*******************************************************************
Asynchronous thread.
Emulates the real CD/DVD drive.
********************************************************************/
void cdvd_asyncthread(void *args){ /* For asynchronous CD/DVD hardware operations */
	static iop_sys_clock_t callback_intr_entry={
		100,    /* callback_intr_entry.lo */
		0,      /* callback_intr_entry.hi */
	};

	DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Initilizing asynchronous thread...\n");

	async_config.asyncCmd=ASYNC_CMD_NOP;
	DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Initilization complete. CDVDMAN ASYNC is now awaiting further commands.\n");
	while(1){ /* Loop forever */
		if((async_config.drivestate==SCECdStatStop)){
			DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: CDVDMAN NOP.\n");
			goto async_nop; /* Loop while waiting for new command */
		}

		switch((async_config.asyncCmd&0x7F)){
			case ASYNC_CMD_READ:
			case ASYNC_CMD_READDVD:
				async_read();
				async_config.rbuffoffset=0; /* Reset the current buffer offset indicator */
				break;
			case ASYNC_CMD_SEEK: /* If the current operation is to seek to the specified sector */
				async_lseek();
				break;
			case ASYNC_CMD_PAUSE:
				async_config.drivestate=SCECdStatPause;
				break;
			case ASYNC_CMD_STANDBY:
				async_config.drivestate=SCECdStatSpin;
				break;
			case ASYNC_CMD_STOP:
				async_config.drivestate=SCECdStatStop;
				break;
		} /* End of "switch" */

		DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: End of CD/DVD or DMA interrupt cycle. Performing cleanup...\n");

		/* Check if there is a need to emulate the CD/DVD hardware interrupt. Don't emulate it if not necessary (Emulation with this system causes ~100us delays!!) */
		if((!cdvdman_cmdfunc)||(!cdvdman_user_cb)){
			SetEventFlag(cdvdman_intr_ef, 1);
			cdvdman_cmdstruct.cdvdman_waf=1;
		}
		else{
			/* DEBUG_PRINTF("Emulating interrupts. cmd: 0x%02x; read2: %d, usetoc: %d\n", cdvdman_cmdfunc, cdvdman_cmdstruct.cdvdman_read2_flg, cdvdman_cmdstruct.cdvdman_usetoc); */
			DEBUG_PRINTF("Emulating interrupts. cmd: 0x%02x.\n", cdvdman_cmdfunc);
			SetAlarm(&callback_intr_entry, &intr_cb, &cdvdman_cmdstruct);
		}

		DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Cleanup complete.\n");

async_nop:
		async_config.status=SCECdComplete;
		async_config.drivestate=SCECdStatPause;
		async_config.asyncCmd=ASYNC_CMD_NOP;

		SleepThread();
	} /* End of main loop */
} /* End of cdvd_asyncthread function */

/*******************************************************************
End-of asynchronous operation routine.
Does the following:
        1. Emulates the end-of-DMA event flag setting.
        2. Calls registered callback routines at the correct moments.
********************************************************************/
unsigned int intr_cb(void *common){ /* End of CD/DVD or DMA interrupt(Emulated) routine */
	iSetEventFlag(cdvdman_intr_ef, 1);
	cdvdman_cmdstruct.cdvdman_waf=1;

	if((cdvdman_cmdfunc)&&(cdvdman_user_cb)){
#ifdef DEBUG_TTY_FEEDBACK
		iDEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Calling interrupt user callback; Reason: %d.\n",cdvdman_cmdfunc);
#endif

		if(cdvdman_cmdfunc==0xE||cdvdman_cmdfunc==0x9) cdvdman_cmdfunc=SCECdFuncRead;
		cdvdman_user_cb(cdvdman_cmdfunc);
	}
	if(cdvdman_cmdstruct.cdvdman_waf!=0) cdvdman_cmdfunc=0; /* !! NOTE !! Some games will call a non-blocking function WITHIN this interrupt callback function... this is a workaround to prevent a race condition. :( */

	return 0;
}

/*******************************************************************
CD/DVD interrupt handler (Real CD/DVD interrupt handling)
********************************************************************/
int intrh_cdrom(void *common){
	if(CDVDreg_PWOFF&CDL_DATA_RDY) CDVDreg_PWOFF=CDL_DATA_RDY;

	if(CDVDreg_PWOFF&CDL_DATA_END){
		CDVDreg_PWOFF=CDL_DATA_END;	/* Acknowledge power-off request */
		if(cdvdman_poff_cb) cdvdman_poff_cb(cdvdman_poffarg); /* Call Power-off callback */
	}
	else{
		CDVDreg_PWOFF=CDL_CD_COMPLETE;  /* Acknowledge interrupt */
	}

	return 1;
}

/**************** FUNCTIONS USED BY cdvdman_CdSearchFile() ****************/
/*******************************************************************
Returns 1 when the segment of the path given is a filename.
Else, it returns 0 when it's a directory.
********************************************************************/
static int isfilename(const char *path){
	unsigned char ptr;

	for(ptr=0;path[ptr]!='\\';ptr++) if((path[ptr]=='.')||(path[ptr]=='\0')) return 1;
	return 0;
}

/*******************************************************************
Strips the name of the first directory or file out of a path as the output.
e.g.:

Input: "folder\file.txt;1"
Output: "folder"
********************************************************************/
static void stripdirfilename(const char *path,unsigned char *target,unsigned int *extern_ptr){
	void *path_ptr;
	register unsigned int name_segment_len;

	path_ptr=strchr(path,'\\');

	if(path_ptr==NULL){ /* A file's name */
		name_segment_len=strlen(path);
		strcpy(target,path);
		if(extern_ptr!=NULL){
			while(path[name_segment_len]=='\\') name_segment_len++;
			*extern_ptr+=name_segment_len; /* Include any '\' characters present after it in the index */
		}
	}
	else{ /* A folder's name */
		name_segment_len=(unsigned long)path_ptr-(unsigned long)path;
		strncpy(target,path,name_segment_len);
		target[name_segment_len]='\0'; /* NULL terminate */
		if(extern_ptr!=NULL){
			while(path[name_segment_len]=='\\') name_segment_len++;
			*extern_ptr+=name_segment_len; /* Include any '\' characters present after it in the index */
		}
	}
#ifdef DEBUG_TTY_FEEDBACK
	DEBUG_PRINTF("Stripped path: %s, External index: 0x%x\n",target,*extern_ptr);
#endif
}

/**************** FUNCTIONS USED BY THE ASYNCHRONOUS THREAD ****************/
/*******************************************************************
Checks whether the specified LSN is valid.
********************************************************************/
static int isLSNValid(unsigned int lsn){
	if(lsn>(cdvdman_conf.discInfo[0].sectorcount+cdvdman_conf.discInfo[1].sectorcount)){
		DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Warning! Invalid LSN read request! LSN: %u; Maximum sectors (Layer 0): %u; Maximum sectors (Layer 1): %u.\n", lsn, cdvdman_conf.discInfo[0].sectorcount, cdvdman_conf.discInfo[1].sectorcount);
		cdvdman_cmdstruct.cdvdman_cderror=SCECdErIPI;   /* Invalid LSN */
		/* cdvdman_cmdfunc=0;	*/			/* Invalidate CD/DVD operation request. */
		return 0;
	}

	return 1;
}

/*******************************************************************
Asynchronous sector lseek() function.
********************************************************************/
inline void async_lseek(void){
	unsigned int lsn=((struct read_n_command_struct *)async_config.ncmd_buf)->lsn;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Seek N-command received. LSN: 0x%x\n", lsn);

	if(!isLSNValid(lsn)) return;

	async_config.status=SCECdNotReady;
	cdvdman_cmdstruct.cdvdman_cderror=SCECdErNO; /* No error */
	async_config.drivestate=SCECdStatSeek;

	if(cdvdman_cmdstruct.cdvdman_readlsn!=lsn){ /* Seek (If required) to the specified LSN */
		/* seek_sector(lsn); *//* Nothing to be done. */
		cdvdman_cmdstruct.cdvdman_readlsn=lsn; /* Update the internal LSN position indicator */
	}

	DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Seek N-command completed.\n");
}

/*******************************************************************
Asynchronous sector read/streaming function.
********************************************************************/
inline void async_read(void){
	unsigned int bytestoread, bytesremaining, lsn=((struct read_n_command_struct *)async_config.ncmd_buf)->lsn, sectors=((struct read_n_command_struct *)async_config.ncmd_buf)->sectors, src_buffer_offset;
	unsigned int sectorsToRead, max_cache_sz;
	static struct _cdvd_read_data EE_read_fix_data;
	void *destination, *dest_buffer;
	unsigned int *dest_buffer_offset_ptr;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: read N-command received. LSN: 0x%x Sectors: 0x%x; buffer: %p\n", lsn, sectors, cdvdman_cmdstruct.cdvdman_rd_buf);

	if(!isLSNValid(lsn)) return;

	cdvdman_cmdstruct.cdvdman_readlsn=lsn; /* Update the internal LSN position indicator */

	async_config.status=SCECdNotReady;
	cdvdman_cmdstruct.cdvdman_cderror=SCECdErNO; /* No error */
	async_config.drivestate=SCECdStatRead;

	/*	Global variables used:
	*
	*	cdvdman_cmdstruct.cdvdman_rd_buf	-> Pointer to the buffer that will receive data
	*	async_config.rbuffoffset		-> Buffer offset
	*
	*	Local variables used:
	*
	*	lsn		-> Logical Sector Number to start reading from
	*	sectors		-> Number of consecutive sectors to read.
	*	sectorsToRead	-> Number of sectors to read in that read cycle.
	*	bytestoread	-> Number of bytes to read in that read cycle.
	*	max_cache_sz	-> Maximum number of sectors that can be read into memory (Either directly into the destination, or the DMA cache).
	*	destination	-> Local mirror of cdvdman_cmdstruct.cdvdman_rd_buf. The pointer may be altered to correct certain "faults" like the use of unaligned buffers.
	*	src_buffer_offset	-> Offset of the source buffer.
	*	dest_buffer_offset_ptr	-> A pointer to a variable that'll receive the current offset of the destination buffer, as it's read realtime from the device
	*					(It'll also indicate that async_config.rbuffoffset should be incremented manually when set to NULL).
	*/

	destination=cdvdman_cmdstruct.cdvdman_rd_buf;
	bytestoread=0;
	src_buffer_offset=0;
	bytesremaining=sectors*2048;
	dest_buffer=cdvdman_fsvrbuf; /* Pointer to the buffer which data is to be read into (Before being copied to it's destination). Otherwise, it simply points directly to the destination buffer. */

	if(async_config.flags&SYS_EE_RAM_DEST) memset(&EE_read_fix_data, 0, sizeof(struct _cdvd_read_data));

        if(((async_config.flags&SYS_EE_RAM_DEST)&&(((unsigned int)cdvdman_cmdstruct.cdvdman_rd_buf)&0xF))||(((unsigned int)cdvdman_cmdstruct.cdvdman_rd_buf)&3)){
                DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Warning! Buffer address is not aligned!\n");
                async_config.flags|=SYS_MEMALIGN_FIX; /* For IOP memory reads: Read sectors into the internal buffer, and then copy them to the destination. Do this only if the game specified a buffer which is not aligned. */

		if(async_config.flags&SYS_EE_RAM_DEST){
			EE_read_fix_data.dest1=destination;
			EE_read_fix_data.size1=((((unsigned int)destination+16)&0xFFFFFFF0)-(unsigned int)destination);

			EE_read_fix_data.dest2=(void *)(((unsigned int)destination+bytesremaining)&0xFFFFFFF0);
			EE_read_fix_data.size2=16-EE_read_fix_data.size1; /* Total number of bytes that need to be copied manually to achieve 16-byte alignment is always 16. */
			bytesremaining-=16; /* EE_read_fix_data.size1+EE_read_fix_data.size2 */
			DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Final xfer sz: %ld\n", EE_read_fix_data.size2);
		}
	}


	/* !!NOTE!!
		When data is being read to ALIGNED buffers in IOP memory: async_config.rbuffoffset is incremented realtime!
		When data is being read to ALIGNED/UNALIGNED buffers in EE memory, or to UNALIGNED Buffers in IOP memory: async_config.rbuffoffset is manually incremented (Hence dest_buffer_offset_ptr will be set to NULL)!
	*/

	if((async_config.flags&SYS_EE_RAM_DEST)||(async_config.flags&SYS_MEMALIGN_FIX)){
		dest_buffer_offset_ptr=NULL; /* We may read more than what's actually needed when we try to fix DMA transfers to unalign buffers, or we need the offset of the target buffer to be incremented only AFTER we copy the data
						(Usually when reading data into buffers in EE RAM, or when reading data into unaligned buffers in IOP memory. This is needed because the data is read into a temporary buffer before being copied into their destination). */
	}
	else dest_buffer_offset_ptr=&async_config.rbuffoffset; /* For data reads to PROPERLY ALIGNED buffers in IOP and EE memory, increment async_config.rbuffoffset realtime. */

	max_cache_sz=((!(async_config.flags&SYS_EE_RAM_DEST))&&(!(async_config.flags&SYS_MEMALIGN_FIX)))?sectors:cdvdman_conf.read_buffer_sz; /* Read the maximum number of sectors that can be held in the cache, if not transferring data to EE RAM  */

	while(bytesremaining>0){
			sectorsToRead=(sectors>=max_cache_sz)?max_cache_sz:sectors; /* Determine number of sectors to read this read cycle */
			if((!(async_config.flags&SYS_EE_RAM_DEST))&&(!(async_config.flags&SYS_MEMALIGN_FIX))) dest_buffer=(void *)(destination+async_config.rbuffoffset);

			if((async_config.flags&SYS_EE_RAM_DEST)&&(async_config.flags&SYS_MEMALIGN_FIX)){
				if(sectorsToRead!=max_cache_sz) sectorsToRead++; /* Read 1 more sector into memory than what is actually copied out. */
			}

			/* EE_DMASync(); *//* We'll probably not need to wait for the DMA transfer to end since USB transfer rates are a lot slower than the PS2's DMAC's speed. */
			read_sector(lsn, sectorsToRead, dest_buffer, dest_buffer_offset_ptr); /* Read sectors into either CDVDMAN's buffer (Reading to the EE's RAM, or because of the use of an unaligned buffer), or directly to the destination. */

			if((async_config.flags&SYS_EE_RAM_DEST)&&(async_config.flags&SYS_MEMALIGN_FIX)){
				sectorsToRead--; /* For unaligned buffers in EE RAM: Read 1 extra sector than the number of sectors to be copied to the destination. */
			}

			cdvdman_cmdstruct.cdvdman_readlsn+=sectorsToRead; /* This is actually increment during the DMA Channel 3 callback. */
			bytestoread=sectorsToRead*2048;

			if(bytestoread>bytesremaining) bytestoread=bytesremaining;

			if(async_config.flags&SYS_EE_RAM_DEST){		/* Fixes for non-aligned buffers in EE RAM (If applicable). */
				if(async_config.flags&SYS_MEMALIGN_FIX){ /* Note: Almost all of the code blocks within this IF statement will only be executed in the 1st read in any request. */
					if(async_config.rbuffoffset==0){ /* !! NOTE !! async_config.rbuffoffset is reset automatically at the end of every read operation (And hence will always be 0 at the start of every read operation). */
						DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: 1. Clipping %ld bytes. Src: %p; Dest: %p.\n", EE_read_fix_data.size1, cdvdman_fsvrbuf, (void *)EE_read_fix_data.dest1);
						async_config.rbuffoffset=EE_read_fix_data.size1;
						memcpy(EE_read_fix_data.buffer1, cdvdman_fsvrbuf, EE_read_fix_data.size1);
						src_buffer_offset=EE_read_fix_data.size1;
					}
				}

				EE_memcpy((void *)(destination+async_config.rbuffoffset), (void *)(cdvdman_fsvrbuf+src_buffer_offset), bytestoread); /* Send the data to EE RAM via DMA */
			}
			else{	/* The destination is in IOP memory. */
				if(async_config.flags&SYS_MEMALIGN_FIX) memcpy((void *)(destination+async_config.rbuffoffset), cdvdman_fsvrbuf, bytestoread);
			}

			if(dest_buffer_offset_ptr==NULL) async_config.rbuffoffset+=bytestoread; /* Increment async_config.rbuffoffset indirectly if we're reading data into unaligned buffers in IOP memory, or into buffers in EE memory. */

			lsn+=sectorsToRead;
			bytesremaining-=bytestoread;
			sectors-=sectorsToRead;

			/* if(async_config.flags&SYS_EE_RAM_DEST) EE_memcpy(((struct cdvdfsv_ee_read_params *)async_config.ncmd_buf)->readpos, &async_config.rbuffoffset, 4); */

			if(async_config.asyncCmd==ASYNC_CMD_ABORT){ /* Check if the "abort" N-command was sent */
				DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: Abort N-command received!\n");
				cdvdman_cmdstruct.cdvdman_cderror=SCECdErABRT;
				return; /* Stop reading */
			}

		/* EE_DMASync(); *//* We'll probably not need to wait for the DMA transfer to end... */
	}

	if(async_config.flags&SYS_EE_RAM_DEST){
		if(async_config.flags&SYS_MEMALIGN_FIX){
			DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: 2. Clipping %ld bytes. Src: %p; Dest: %p.\n", EE_read_fix_data.size2, (void *)(cdvdman_fsvrbuf+src_buffer_offset+bytestoread), (void *)EE_read_fix_data.dest2);
			memcpy(EE_read_fix_data.buffer2, (void *)(cdvdman_fsvrbuf+src_buffer_offset+bytestoread), EE_read_fix_data.size2);
			async_config.rbuffoffset+=EE_read_fix_data.size2;
		}

		EE_memcpy(((struct cdvdfsv_ee_read_params *)async_config.ncmd_buf)->cb_dataAlignFix, &EE_read_fix_data, sizeof(struct _cdvd_read_data));
		EE_memcpy(((struct cdvdfsv_ee_read_params *)async_config.ncmd_buf)->readpos, &async_config.rbuffoffset, 4);
	}

	/* EE_DMASync(); *//* We'll probably not need to wait for the DMA transfer to end... */

	DEBUG_PRINTF("PS2ESDL_CDVDMAN ASYNC: read N-command completed.\n");
}

/*******************************************************************
Sends nbytes of data from src to a buffer ptr in EE memory.
********************************************************************/
void EE_memcpy(void *dest,void *src,unsigned int nbytes){
	SifDmaTransfer_t dmatransfer;
	int oldstate;

	dmatransfer.src=src;
	dmatransfer.dest=dest;
	dmatransfer.size=nbytes;
	dmatransfer.attr=0;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Transferring %d bytes from 0x%p to EE RAM: 0x%p\n",nbytes,src,dest);

	EE_DMASync();

	do{
		CpuSuspendIntr(&oldstate);
		async_config.EE_dmatrans_id=sceSifSetDma(&dmatransfer, 1);
		CpuResumeIntr(oldstate);
	}while(async_config.EE_dmatrans_id==0);
}

/**************** FUNCTIONS USED TO INTERFACE WITH REAL CD/DVD HARDWARE ****************/
/*******************************************************************
Sends an S-command to the PS2's Mechacon
********************************************************************/
int cdvdman_send_scmd(unsigned char cmd,unsigned char *sdata,unsigned int sdlen,unsigned char *rdata,unsigned int rdlen){
	register unsigned char i;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Writing S-command to the Mechacon. Cmd: 0x%x; sdlen: %u; rdlen: %u.\n",cmd,(unsigned int)sdlen,(unsigned int)rdlen); 

	WaitSema(cdvdman_scmd_sema);

	if(CDVDreg_SDATAIN&0x80){
		SignalSema(cdvdman_scmd_sema);
		DEBUG_PRINTF("PS2ESDL_CDVDMAN: Warning! S-command write request received while the MECHACON is busy!\n");
		return 0;
	}

	while(!(CDVDreg_SDATAIN&0x40)) i=CDVDreg_SDATAOUT;

	for(i=0;i<sdlen;i++) CDVDreg_SDATAIN=sdata[i];

	CDVDreg_SCOMMAND=cmd;
	i=CDVDreg_SCOMMAND;

	while(CDVDreg_SDATAIN&0x80) DelayThread(100);

	for(i=0;(!(CDVDreg_SDATAIN&0x40));i++){
#ifdef DEBUG_TTY_FEEDBACK
		if(i>=rdlen) DEBUG_PRINTF("PS2ESDL_CDVDMAN: Warning! Rx data overflow!\n");
#endif
		rdata[i]=CDVDreg_SDATAOUT;
	}

	/* There was some overflow error handling code here, but was removed. */
	/* Errors should not occur, unless the game has a serious bug, or the PS2's hardware has a problem */

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: S-command write complete.\n");

	SignalSema(cdvdman_scmd_sema);

	return 1;
}

/**************** MISCELLANOUS FUNCTIONS ****************/
/*******************************************************************
Calls the interrupt-safe varient of each function if the function in question was called within an interrupt
********************************************************************/
int clear_ev_flag(int evfid,unsigned int bitpattern)
{
	return(QueryIntrContext()?iClearEventFlag(evfid, bitpattern):ClearEventFlag(evfid, bitpattern));
}

int refer_ef_status(int evfid,iop_event_info_t *info)
{
	return(QueryIntrContext()?iReferEventFlagStatus(evfid,info):ReferEventFlagStatus(evfid,info));
}

int wakeup_thread(int thid){
	return(QueryIntrContext()?iWakeupThread(thid):WakeupThread(thid));
}

static unsigned char databuf[2048]; /* Data buffer for disc information extraction */

/*******************************************************************
Searches the disc image for a specified file,
and return it's properties in "fp".
********************************************************************/
int cdvdman_CdSearchFile(cd_file_t *fp,const char *path, int layer, u8 *fileFlags){
	char tgtfname[16];
	unsigned int pathptr;

	WaitSema(cdvdman_srch_sema);

	struct iso9660_pathtable *ptable;
	struct iso9660_dirRec *dirRec;

	unsigned char IDlen, name_len;		/* The length of the file entry (On the ISO9660 disc image), and the length of the name of the file/directory that's being searched for. */
	unsigned int recLSN, recGrp_offset, recGrpSz;   /* Size of an entire directory record group. */

	/* When debugging is not enabled, the specialised error handlers in this function are not created. */
#ifndef DEBUG_TTY_FEEDBACK
	#define searchdirrecords_error srch_error_common
	#define searchptable_error srch_error_common
#endif

	if(((cdvdman_conf.layerBreak==0)&&(layer>0))||(layer>1)) return 0; /* Invalid layer specified. */

	ptable=(struct iso9660_pathtable*)databuf;

	/* Initilize all data fields */
	pathptr=0;

	/* 1. Seek to the path table.
	   2. begin tracing.
	   3. Return LSN of file when found. */

	while(path[pathptr]=='\\') pathptr++; /* Skip any '\' characters present */ 
	if(!isfilename(&path[pathptr])) goto searchptable; /* Continue tracing at path table if the file is not @ the root folder */

	fp->lsn=cdvdman_conf.discInfo[layer].rdirdsc; /* Start in the root directory record */

	goto searchdirrecords;

searchptable:
	recLSN=cdvdman_conf.discInfo[layer].ptableLSN;
	recGrp_offset=0;
	cdvdman_read_noCB(recLSN, 1, databuf, 1); /* Read the path table. */

	stripdirfilename(&path[pathptr], tgtfname, &pathptr); /* "Strip and search..." */

	recGrpSz=cdvdman_conf.discInfo[layer].ptable_sz;
	name_len=strlen(tgtfname);
	DEBUG_PRINTF("Path table grp sz: 0x%x\n", recGrpSz); /* Get the size of the entire path table group */
	do{
		if(recGrp_offset>recGrpSz) goto searchptable_error; /* Prevent overflow when file is not found */

		IDlen=ptable->lenDI; /* Get LEN_DI(Length of directory identifier) */
		DEBUG_PRINTF("Path table entry ID length: 0x%x\n", IDlen);

		memcpy((unsigned char*)&fp->lsn, &ptable->extLoc, sizeof(fp->lsn)); /* Copy the location of the extent */
		if(layer) fp->lsn=sceCdLsnDualChg(fp->lsn);

		DEBUG_PRINTF("Extent LSN: 0x%x\n", fp->lsn);

		fp->name[IDlen]='\0';
		memcpy(fp->name, &ptable->dirIdentifier, IDlen); /* name + extension must be at least 1 character long + ";" (e.g. "n.bin;1") */
		DEBUG_PRINTF("Current directory name: %s\n\n",fp->name);

		if((ptable->lenDI%2)!=0) ptable->lenDI=ptable->lenDI+1; /* There will be a padding field if the directory identifier is an odd number */

		if(IDlen>0){
			recGrp_offset=recGrp_offset+(unsigned int)8+(unsigned int)ptable->lenDI;
			ptable=(void *)((unsigned int)ptable+(unsigned int)8+(unsigned int)ptable->lenDI);

			if(name_len>IDlen) IDlen=name_len; /* Compare both names, up to the length of the longer string. */
		}
		else{ /* The last path table entry (It's a blank one) in this sector has been reached. Continue searching in the next sector. */
			recLSN++;
			cdvdman_read_noCB(recLSN, 1, databuf, 1); /* Read the sector containing the next extent. */
			ptable=(struct iso9660_pathtable*)databuf; /* Reset sector pointer */
		}
	}while((!IDlen)||memcmp(fp->name, tgtfname, IDlen)); /* Break out of searching loop if match is found */

searchdirrecords:
	while(!isfilename(&path[pathptr])){  /* While the current segment is not the file's name */
		recLSN=fp->lsn;
		cdvdman_read_noCB(recLSN, 1, databuf, 1); /* Read the sector containing the next extent. */
		dirRec=(struct iso9660_dirRec*)databuf; /* Reset sector pointer */
		recGrp_offset=0;

		memcpy((unsigned char*)&recGrpSz, &dirRec->dataLen_LE, 4);
		DEBUG_PRINTF("Directory record grp sz: 0x%x\n", recGrpSz); /* Get the size of the entire directory record group */

		stripdirfilename(&path[pathptr], tgtfname, &pathptr); /* "Strip and search..." */
		name_len=strlen(tgtfname);

		do{
			if(recGrp_offset>recGrpSz) goto searchdirrecords_error; /* Prevent overflow when file is not found */

			DEBUG_PRINTF("Directory record length: 0x%x\n", dirRec->lenDR); /* Get length of directory record */
			memcpy((unsigned char*)&fp->lsn, &dirRec->extentLoc_LE , sizeof(fp->lsn)); /* Read extent LSN address */
			if(layer) fp->lsn=sceCdLsnDualChg(fp->lsn);

			IDlen=dirRec->lenFI; /* Get file ID length */

			DEBUG_PRINTF("Extent LSN: 0x%x\nFile ID length: 0x%x\n", fp->lsn, IDlen);

			fp->name[IDlen]='\0';
			memcpy(fp->name, &dirRec->fileIdentifier, IDlen); /* name + extension must be at least 1 character long + ";" (e.g. "n.bin;1") */
			if((IDlen>0)&&(dirRec->lenDR>0)){
				recGrp_offset=recGrp_offset+(unsigned int)dirRec->lenDR;
				dirRec=(void *)((unsigned int)dirRec+(unsigned int)dirRec->lenDR);

				if(name_len>IDlen) IDlen=name_len; /* This is to force a comparison of both names up to the length of the longer string. */
			}
			else{ /* The last directory record (It's a blank one) in this sector has been reached. Continue searching in the next sector. */
				recLSN++;
				cdvdman_read_noCB(recLSN, 1, databuf, 1); /* Read the sector containing the next extent. */
				dirRec=(struct iso9660_dirRec*)databuf; /* Reset sector pointer */
				IDlen=0;
			}

			DEBUG_PRINTF("Current directory name: %s\n\n", fp->name);
		}while((!IDlen)||memcmp(fp->name, tgtfname, IDlen)); /* Break out of searching loop if match is found */
	}

	/* Sector was read into buffer earlier while tracing through directory structure */
	stripdirfilename(&path[pathptr], tgtfname, &pathptr); /* "Strip and search..." */

	SignalSema(cdvdman_srch_sema);

	if(cdvdman_CdReturnFileInfo(fp, tgtfname, fp->lsn, 0, layer, fileFlags)<0) goto searchdirrecords_error;

	/* Put code for returning file's LSN here */
	DEBUG_PRINTF(	"\n------------------------\n"
			"LSN of target file: 0x%x\n", fp->lsn);

	return 1;

/* Specialised error handlers */
#ifdef DEBUG_TTY_FEEDBACK
searchdirrecords_error:
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Error: Invalid path: %s\n", path);
	goto srch_error_common;

searchptable_error:
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Error: Directory not found in path table.\n");
	goto srch_error_common;
#endif

/* Common error handler */
srch_error_common:
	return 0;
}

int cdvdman_CdReturnFileInfo(cd_file_t *fp, const char *fname, unsigned int dirRecLsn, unsigned int mode, unsigned int layer, u8 *fileFlags){
	int fileid;
	unsigned char IDlen, name_len;		/* The length of the file entry (On the ISO9660 disc image), and the length of the name of the file/directory that's being searched for. */
	unsigned short yearStamp;		/* Contains the year the file was created in */
	struct iso9660_dirRec *dirRec;
	unsigned int dirRecGrp_offset, dirRecGrpSz;     /* Size of an entire directory record group. */
	void *extent;
	char target_fname[16];

	WaitSema(cdvdman_srch_sema);

	name_len=0;

	if(fname!=NULL){ /* This section of codes here will try to force any input filename to confront to the ISO9660 standards. */
		if(strlen(fname)>0){
			strcpy(target_fname, fname);
			if((extent=strchr(target_fname, '.'))!=NULL){ /* Add a ';' character (I intended this to be part of the ";1" suffix) to the 3rd character after the '.'. */
				*(char *)(extent+4)=';';
			}
			else goto dont_change_dir_names;

			if((extent=strchr(target_fname, ';'))!=NULL){ /* Search for the ';', which should be part of the ";1" suffix. Replace it will a NULL (This will terminate the string there). */
				*(char *)extent='\0';
			}

			strcat(target_fname, ";1"); /* Add the ";1" suffix to the end of the file's name. */
		}
		else target_fname[0]='\0'; /* Root directory. */

/* DOPEN uses this function too (via cdvdman_open()), and directory names are not meant to have the ";1" suffix. */
dont_change_dir_names:
		name_len=strlen(target_fname);
	}

	/* pathptr is set to 0 when doing normal searching; Set to get the below code to return the nth file's information otherwise (nth = mode with the last bit filed off) */
	/* Otherwise, mode will specify the layer the file is on. */

	fileid=(mode&0x80000000)?((mode&0x7FFFFFFF)+3):0;
	dirRec=extent=databuf;
	dirRecGrp_offset=0;

	cdvdman_read_noCB(dirRecLsn, 1, extent, 1); /* Read the sector containing the next extent. */

	memcpy((unsigned char*)&dirRecGrpSz, &dirRec->dataLen_LE, 4);
	DEBUG_PRINTF("Directory record grp sz: 0x%x\n", dirRecGrpSz); /* Get the size of the entire directory record group */
	do{
		if(dirRecGrp_offset>=dirRecGrpSz) return -1; /* Prevent overflow when file is not found */

		DEBUG_PRINTF("Directory record length: 0x%x\n", dirRec->lenDR);

		memcpy((unsigned char*)&fp->lsn, &dirRec->extentLoc_LE, 4); /* Read extent LSN address */
		if(layer) fp->lsn=sceCdLsnDualChg(fp->lsn);
		DEBUG_PRINTF("Extent LSN: 0x%x\n",fp->lsn);

		memcpy((unsigned char*)&fp->size, &dirRec->dataLen_LE, 4);
		DEBUG_PRINTF("Data size: %u\n",fp->size);

		/* *** !! NOTE !! *** */
		/*	The date and time format field of the cd_file_t structure is as follows:
			u8 date[8]:
					1st byte: Seconds
					2nd byte: Minutes
					3rd byte: Hours
					4th byte: Date
					5th byte: Month
					6th - 7th byte: Year (4 HEX digits)
			*** !! BYTE 0 IS RESERVED !! ***
		*/

		yearStamp=1900+dirRec->recDateTime[0];    /* According to the ECMA-119 standard, the year byte is the number of years since 1900 */
		memcpy(&fp->date[6], &yearStamp, 2);

		fp->date[5]=dirRec->recDateTime[1];             /* Read the month */
		fp->date[4]=dirRec->recDateTime[2];             /* Read the date */
		fp->date[3]=dirRec->recDateTime[3];             /* Read the hour */
		fp->date[2]=dirRec->recDateTime[4];             /* Read the minute */
		fp->date[1]=dirRec->recDateTime[5];             /* Read the second */

		if(fileFlags) *fileFlags=dirRec->fileFlags; /* Read the file's flags */

		IDlen=dirRec->lenFI; /* Get file ID length */
		DEBUG_PRINTF("File ID length: 0x%x\n", IDlen);

		fp->name[IDlen]='\0';
		memcpy(fp->name, &dirRec->fileIdentifier, IDlen); /* name + extension must be at least 1 character long + ";" (e.g. "n.bin;1") */

		DEBUG_PRINTF("Current filename: %s\n\n", fp->name);

		if((IDlen>0)&&(dirRec->lenDR>0)){
			dirRecGrp_offset=dirRecGrp_offset+(unsigned int)dirRec->lenDR;
			dirRec=(void *)((unsigned int)dirRec+(unsigned int)dirRec->lenDR); /* Seek to next record */

			if(name_len>IDlen) IDlen=name_len; /* This is to force a comparison of both names up to the length of the longer string. */
		}
		else{ /* The last directory record (It's a blank one) in this sector has been reached. Continue searching in the next sector. */
			dirRecLsn++;
			cdvdman_read_noCB(dirRecLsn, 1, extent, 1); /* Read the sector containing the next extent. */
			dirRec=extent; /* Reset sector pointer */
		}

		if(mode&0x80000000) fileid--; /* Decrease fileid by 1 if the nth file's information is to be returned. */

		/* !!NOTE!! HACK!! Some *buggy* games try to search for files without the ";1" suffix... :(
		*      The workaound here is to compare the file names based on the specified filename's length instead of using the length of the current file entry (On the disc).
		*/
	}while(((fname!=NULL)&&((!IDlen)||memcmp(fp->name, target_fname, IDlen)))||((mode&0x80000000)&&(fileid))); /* Break out of searching loop if match is found (And not searching according to file index ID) */

	SignalSema(cdvdman_srch_sema);

	return 0;
}

/*******************************************************************
Opens the file/folder specified as name.
********************************************************************/
void cdvdman_getstat(fio_stat_t *stat, cd_file_t *fileinfo, u8 fileFlags){
	stat->attr=0;		/* Device access attributes */
	memset(stat->private, 0, sizeof(stat->private));
	stat->hisize=0;	/* File's size; Upper 32-bits */

	stat->mode=O_RDONLY;				/* File attributes (Without the flags that state whether it's a directory or just a regular file) */
	stat->size=(unsigned int)fileinfo->size;	/* File's size; Lower 32-bits */
	memcpy(stat->ctime, fileinfo->date, 8);	/* File's creation time stamp */
	memcpy(stat->atime, fileinfo->date, 8);	/* File's last access time stamp */
	memcpy(stat->mtime, fileinfo->date, 8);	/* File's last modification time stamp */

	/* NOTE!! According to the ECMA-119 standard, the file flags field will have the following flag which we're interested in here:
	        Bit 1 - If set to ZERO, shall mean that the directory record does not identify a directory.
	                If set to ONE, shall mean that the directory record identifies a directory.
	*/
					/* 0x1049				0x2000			0x124 */
	stat->mode=((fileFlags&2)?(FIO_S_IFDIR|FIO_S_IXUSR|FIO_S_IXGRP|FIO_S_IXOTH):(FIO_S_IFREG)) | (FIO_S_IRUSR|FIO_S_IRGRP|FIO_S_IROTH);
}

/*******************************************************************
Opens the file/folder specified as name.
********************************************************************/
int cdvdman_open(iop_file_t *f, const char *name, int mode){
	cd_file_t fileinfo;
	unsigned char fileFlags;

	if(cdvdman_CdSearchFile(&fileinfo, name, f->unit, &fileFlags)==0){
		DEBUG_PRINTF("PS2ESDL_CDVDMAN: SceCdSearchFile(): Invalid path.\n");
		return -ENOENT;	/* Invalid path */
	}

	f->privdata=malloc(sizeof(struct cdvdman_fhandle));	/* Allocate a handle, and store a pointer to it. */

	((struct cdvdman_fhandle *)f->privdata)->lsn=fileinfo.lsn;	/* LSN of the file */
	((struct cdvdman_fhandle *)f->privdata)->size=fileinfo.size;	/* Size of the file */
	((struct cdvdman_fhandle *)f->privdata)->offset=0;		/* Set file offset to 0 */
	((struct cdvdman_fhandle *)f->privdata)->flags=(fileFlags&2)?(mode|FIO_S_IFDIR):(mode|FIO_S_IFREG); /* Set file access attribute flags, and file type. */

	return 0;
}

/*******************************************************************
 Internal routine to fill the streaming ring buffer
********************************************************************/
void stReadRingBuffer(unsigned int lsn, unsigned int sectors, void *buffer){
	cdvdman_read_noCB(lsn, sectors, buffer, 0);
}

/*******************************************************************
 Internal routine to set up streaming parameters
********************************************************************/
void stInit(unsigned int lsn){
	async_config.stLSN=lsn;
	async_config.ringbuff_bank_sector_offset=0;
	async_config.ringbuff_bank_ptr=async_config.stReadBuffer;
	async_config.bank_ID=0;
}

/*******************************************************************
Waits for the DMA transfer to EE memory to end. 
********************************************************************/
void EE_DMASync(void){
	if(async_config.EE_dmatrans_id!=0){
		while(sceSifDmaStat(async_config.EE_dmatrans_id)>=0){}; /* Wait for the end of any ongoing DMA transfers */
		async_config.EE_dmatrans_id=0;
	}
}

/*******************************************************************
Extracts required information from a console's ILINK ID for DNAS. 
********************************************************************/
int cdvdman_readConID(u64 *guid, unsigned int *modelID)
{
	u64 ilink_id;
	int stat;

	sceCdRI((u8 *)&ilink_id, &stat);

	if(guid!=NULL) *guid=(ilink_id&0xFFFFFFFF00000000) | ((ilink_id&0xFF)|0x08004600);
	if(modelID!=NULL) *modelID = ilink_id >> 8;

	return 1;
}

void *malloc(unsigned int NumBytes){
	int OldState;
	void *buffer;

	CpuSuspendIntr(&OldState);
	buffer=AllocSysMemory(ALLOC_FIRST, NumBytes, NULL);
	CpuResumeIntr(OldState);

	return buffer;
}

void free(void *buffer){
	int OldState;

	CpuSuspendIntr(&OldState);
	FreeSysMemory(buffer);
	CpuResumeIntr(OldState);
}

