#include "new_cdvdman.h"
#include <errno.h>
#include "new_ioman.h"
#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <sys/stat.h>

#include "cdvdman_hdr.h"
#include "cdvdstm.h"
#include "sector_io.h"

/* For internal asynchronous function */
extern struct cdvdman_async async_config;

extern char startup_init; /* Whether the module has already been started-up(0) or is now being started-up(1) */

extern struct cdvdman_config cdvdman_conf; /* CDVDMAN emulation configuration */

/* The data below is probably a some very big (0xC0 bytes or even bigger) internal structure */
extern struct cdvd_cmdstruct cdvdman_cmdstruct;

/* CDVDMAN event flags */
static int cdvdman_fio_sema;
extern int cdvdman_intr_ef;

/* For blocking certain modules which may either conflict with PS2ESDL, or which will just consume valuable memory */
/* !!NOTE!! As per the ISO9660 specification, there should be a ";1" at the end of the file's name, but some games have omitted it (bug). */
static unsigned char bannedModules[][18]={ /* Maximum length of any file on a disc with the ISO9660 filesystem: 8 characters name + '.' + 3 characters extension + ";1" = 14 */
	/* USB drivers */
	"USBD.IRX", "USB_driver",

	/* CD/DVD related */
	"CDVDSTM.IRX", "cdvd_st_driver",

	/* Others */

	"\0", /* NULL terminator */
};

/* Dummy IRX which will be use substitute a banned module */
extern unsigned char dummy_irx[];
extern unsigned int size_dummy_irx;

/* Device functions */
int cdrom_init(iop_device_t *data){
	iop_sema_t SemaData;

	SemaData.attr=0;
	SemaData.option=0;
	SemaData.initial=1;
	SemaData.max=1;
	cdvdman_fio_sema=CreateSema(&SemaData);

	cdvdman_init();
	return 0;
}

int cdrom_deinit(iop_device_t *data){
	cdvdman_deinit();
	DeleteSema(cdvdman_fio_sema);

	return 0;
}

int cdrom_open(iop_file_t *f, const char *name, int mode){
	int i, ind, result;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: cdrom0 device open() request received. Path: %s; Flags: %u; layer: %d.\n",name,mode, f->unit);

	WaitSema(cdvdman_fio_sema);

	/* Note!! f->unit stores the layer which the file resides on. */
	if((result=cdvdman_open(f, name, mode))>=0){ /* Only if the file was found. */
		f->mode=O_RDONLY; /* Force file open flags to O_RDONLY. */

		for(i=0; bannedModules[i][0]!='\0'; i+=2){
			if(strstr(name, bannedModules[i])){
				((struct cdvdman_fhandle *)f->privdata)->flags=FILE_MOD_BANNED;
				((struct cdvdman_fhandle *)f->privdata)->size=size_dummy_irx;
				((struct cdvdman_fhandle *)f->privdata)->lsn=(unsigned int)malloc(size_dummy_irx);
				memcpy((void *)((struct cdvdman_fhandle *)f->privdata)->lsn, dummy_irx, size_dummy_irx);

				for(ind=0; ind<size_dummy_irx; ind++){
					if(!memcmp(&((u8 *)((struct cdvdman_fhandle *)f->privdata)->lsn)[ind], "cdvd_dm_driver", 14)){
						strcpy(&((u8 *)((struct cdvdman_fhandle *)f->privdata)->lsn)[ind], bannedModules[i+1]);
						break;
					}
				}

				DEBUG_PRINTF("PS2ESDL_CDVDMAN: cdrom0 device open() redirecting all I/O requests for banned module: %s.\n", &bannedModules[i][0]);
				break;
			}
		}

		/* if(mode&SCE_CdSTREAM) sceCdStStart(((struct cdvdman_fhandle *)f->privdata)->lsn,NULL); *//* Start file streaming */
	}

	SignalSema(cdvdman_fio_sema);
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: cdrom0 device open() function ended. File's LSN: 0x%x, file's size: %u\n", ((struct cdvdman_fhandle *)f->privdata)->lsn, ((struct cdvdman_fhandle *)f->privdata)->size);
	return result; /* Return the result */
}

int cdrom_close(iop_file_t *f){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: cdrom0 device closing handle.\n");

	WaitSema(cdvdman_fio_sema);

	if(((struct cdvdman_fhandle *)f->privdata)->flags&FILE_MOD_BANNED){
		free((void *)((struct cdvdman_fhandle *)f->privdata)->lsn);
	}

	free(f->privdata); /* Invalidate handle */

	SignalSema(cdvdman_fio_sema);

	return 0;
}

/* NOTE!! LOADCORE will only read the dummy IRX in 1 read, since it's so small.
        If you *somehow* need offset support, then you need to implement a more advanced version of this function */
inline void cdrom_dummy_read(void *buf, void *src, int nbyte){
	memcpy(buf, src, nbyte);
}

int cdrom_read(iop_file_t *f,void *buf,int nbyte){
	unsigned int sectorIDofs,lsnID,dataoffset,sectorstoread,sectordatoffs;
	static unsigned char sectorbuf[2048];
	int bytesread,temp;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: cdrom0 device: read requested. Buffer: %p; bytes: %d\n", buf, nbyte);

	/* if(nbyte<1) return 0; *//* Exit if there isn't any data to read */

	if(((struct cdvdman_fhandle *)f->privdata)->flags==FILE_MOD_BANNED){
		cdrom_dummy_read(buf, (void *)((struct cdvdman_fhandle *)f->privdata)->lsn, nbyte);
		return nbyte;
	}

	WaitSema(cdvdman_fio_sema);

	dataoffset=((struct cdvdman_fhandle *)f->privdata)->offset;
	sectordatoffs=dataoffset%2048;	/* Offset in last sector before the data to be read, if any. */
	sectorIDofs=(dataoffset-sectordatoffs)>>11;	/* Starting sector offset, which is the offset in bytes divided by 2048. */
	lsnID=((struct cdvdman_fhandle *)f->privdata)->lsn;

	if((((struct cdvdman_fhandle *)f->privdata)->offset+nbyte)>((struct cdvdman_fhandle *)f->privdata)->size) nbyte=((struct cdvdman_fhandle *)f->privdata)->size-((struct cdvdman_fhandle *)f->privdata)->offset; /* Check for reading past the end of file */
	bytesread=nbyte;	/* Number of bytes read; Increment now for efficiency... this value will still be achieved even if it was incremented real-time */

	((struct cdvdman_fhandle *)f->privdata)->offset+=nbyte;        /* Increment the current file offset to it's future value now. */
                                                                /* nbyte will become 0 at the end of this operation, and we would then be unable to increment the file position indicator anymore */

	/* Read 1st Sector, if the current offset of file is not divisible by 2048 */
	if(sectordatoffs>0){
		cdvdman_read_noCB(lsnID+sectorIDofs, 1, sectorbuf, 1);

		sectorIDofs++; /* 1 sector read */

		if((sectordatoffs+nbyte)>2048){
        		temp=(2048-sectordatoffs); /* Number of bytes to copy */
			memcpy(buf,&sectorbuf[sectordatoffs],temp);
			buf+=temp;   /* Increment pointer */
			nbyte-=temp; /* Calculate remaining bytes to read */
		}
		else{
			memcpy(buf,&sectorbuf[sectordatoffs],nbyte); /* Requested less than 2048 bytes, and all bytes are within the 1st sector */
			goto read_end; /* No more bytes to read. Exit; nbyte=nbyte-nbyte */
		}
	}

	/* Read consecutive sectors */
	sectorstoread=(nbyte-(nbyte%2048))>>11; /* Number of consecutive, complete(2048 bytes) sectors to read */
	if(sectorstoread==0) goto readremaining; /* No "middle" sectors to read */

	cdvdman_read_noCB(lsnID+sectorIDofs, sectorstoread, buf, 1);

	temp=(sectorstoread<<11);	/* Get the number of bytes to read (Number of sectors * 2048). */
	buf+=temp; /* Increment pointer */
	nbyte-=temp; /* Decrease remaining bytes to read */
	sectorIDofs+=sectorstoread; /* Increase sector number pointer */

readremaining: /* Read the "end" sectors */
	if(nbyte<1) goto read_end; /* No more data to read */
	cdvdman_read_noCB(lsnID+sectorIDofs, 1, sectorbuf, 1);

	memcpy(buf,sectorbuf,nbyte);

read_end: /* End of file read operation */
	SignalSema(cdvdman_fio_sema);

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: cdrom0 device: read request completed. Bytes read: %u, Current file offset: %u\n",bytesread,((struct cdvdman_fhandle *)f->privdata)->offset);

	return bytesread; /* Return number of bytes read */
}

int cdrom_lseek(iop_file_t *f, unsigned long offset, int pos){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN cdrom0 device: lseek requested. offset: %lu, Command: %d\n",offset,pos);

	if(offset<0){
		DEBUG_PRINTF("PS2ESDL_CDVDMAN cdrom0 device: Warning! lseek request beyond start of file!n");
		return -EINVAL;
	}

	if(offset>((struct cdvdman_fhandle *)f->privdata)->size) offset=((struct cdvdman_fhandle *)f->privdata)->size;

	WaitSema(cdvdman_fio_sema);

	switch(pos){
		case SEEK_SET:
			((struct cdvdman_fhandle *)f->privdata)->offset=offset; /* Start of file/SEEK_SET */
			break;
		case SEEK_CUR:
			((struct cdvdman_fhandle *)f->privdata)->offset+=offset; /* From current offset/SEEK_CUR */
			break;
		case SEEK_END:
			((struct cdvdman_fhandle *)f->privdata)->offset=((struct cdvdman_fhandle *)f->privdata)->size-offset; /* From end of file/SEEK_END */
	}

	SignalSema(cdvdman_fio_sema);

	DEBUG_PRINTF("PS2ESDL_CDVDMAN cdrom0 device: LSEEK request completed. Current file offset: %u\n",((struct cdvdman_fhandle *)f->privdata)->offset);
	return ((struct cdvdman_fhandle *)f->privdata)->offset; /* Return new current offset of file position indicator */
}

int cdrom_ioctl(iop_file_t *f,unsigned long arg,void *param){
	int r;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN cdrom0 device: IOCTL requested. Arg=0x%lx\n",arg);

	switch(arg){
		case 0x10010:
			r=sceCdGetError();
			break;
		case 0x10030:
			r=sceCdGetDiskType();
			break;
		/* Any IOCTL call not processed here is unsupported and emulated. */
		default:
			r=0;
			DEBUG_PRINTF("PS2ESDL_CDVDMAN: Warning! Unsupported IOCTL call\n");
	}

	return r;
}

int cdrom_dopen(iop_file_t *f, const char *dirname){
	int result;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN cdrom0 device: DOPEN requested. Path: %s\n",dirname);

	WaitSema(cdvdman_fio_sema);

	result=cdvdman_open(f, dirname, O_RDONLY);

	SignalSema(cdvdman_fio_sema);

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: cdrom0 device dopen() function ended. Allocated handle index: %d.\n",result);
	return result; /* Return file descriptor/result */
}

int cdrom_dread(iop_file_t *f, fio_dirent_t *buf){
	cd_file_t fileinfo;
	int result;
	unsigned char fileFlags;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN cdrom0 device: DREAD requested\n");

	WaitSema(cdvdman_fio_sema);

	result=cdvdman_CdReturnFileInfo(&fileinfo, NULL, ((struct cdvdman_fhandle *)f->privdata)->lsn, ((struct cdvdman_fhandle *)f->privdata)->offset|0x80000000, f->unit, &fileFlags);

	if(result>=0){
		cdvdman_getstat(&buf->stat, &fileinfo, fileFlags);
		strcpy(buf->name, fileinfo.name);

		((struct cdvdman_fhandle *)f->privdata)->offset++;
		result=strlen(buf->name); /* Return the length of the specified file. */
	}
	else{
		DEBUG_PRINTF("PS2ESDL_CDVDMAN: Unable to get statistics for file.\n");
		result=0;	/* There were either no more entries in directory or an error had occurred. */
	}

	SignalSema(cdvdman_fio_sema);

	DEBUG_PRINTF("PS2ESDL_CDVDMAN cdrom0 device: DREAD request completed.\n");

	return result;
}

int cdrom_getstat(iop_file_t *f, const char *name, fio_stat_t *buf){
	cd_file_t fileinfo;
	unsigned char fileFlags;
	int result;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN cdrom0 device: GETSTAT requested for file %s.\n",name);

	WaitSema(cdvdman_fio_sema);

	if(cdvdman_CdSearchFile(&fileinfo,name, f->unit, &fileFlags)!=0){
		cdvdman_getstat(buf, &fileinfo, fileFlags);
		result=0;
	}
	else{
		DEBUG_PRINTF("PS2ESDL_CDVDMAN: Unable to get statistics for file.\n");
		result=-ENOENT;	/* Invalid path */
	}

	SignalSema(cdvdman_fio_sema);

	return 0;
}

int cdrom_devctl(iop_file_t *f,const char *arg2,int cmd,void *argp,unsigned int arglen,void *bufp,unsigned int buflen){
	int rc=0, value;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: cdrom0 device: DEVCTL request received. cmd=0x0%x, arglen=0x0%x,buflen=0x0%x\n",cmd,arglen,buflen);

	switch(cmd){
		case CDIOC_READCLOCK:
			rc=sceCdReadClock((cd_clock_t*)bufp);

			if(rc!=1) rc=-EIO;
			break;
		case CDIOC_READDISKID:
			rc=sceCdReadDiskID((unsigned int *)bufp);
			break;
		case CDIOC_GETDISKTYPE:
			*(int*)bufp=sceCdGetDiskType();
			break;
			case CDIOC_GETERROR:
			*(int*)bufp=sceCdGetError();
			break;
		case CDIOC_TRAYREQ:
			rc=sceCdTrayReq(*(int*)argp,(unsigned int*)bufp)!=1?-EIO:0;
			break;
		case CDIOC_STATUS:
			*(int*)bufp=sceCdStatus();
			break;
		case CDIOC_POWEROFF:
			rc=sceCdPowerOff((int*)bufp);
			if(rc!=1) rc=-EIO;
			break;
		case CDIOC_DISKRDY:
			*(int*)bufp=sceCdDiskReady(*(int*)argp);
			break;
		case CDIOC_READMODELID:
			rc=sceCdReadModelID((unsigned int *)bufp);
			break;
		case CDIOC_STREAMINIT:
			rc=sceCdStInit(((int*)argp)[0],((int*)argp)[1],(void*)((int*)argp)[2]); 
			break;
		case CDIOC_BREAK:
			rc=sceCdBreak()==0?(-EIO):0;
			sceCdSync(4);
			break;
		case CDIOC_SEEK:
			rc=sceCdSeek(*(unsigned int*)argp)!=1?-EIO:0;
			sceCdSync(6);
			break;
		case CDIOC_STANDBY:
			rc=sceCdStandby()!=1?-EIO:0;
			sceCdSync(4);
			break;
		case CDIOC_STOP:
			rc=sceCdStop()!=1?-EIO:0;
			sceCdSync(4);
			break;
		case CDIOC_PAUSE:
			rc=sceCdPause()!=1?-EIO:0;
			sceCdSync(6);
			break;
		case CDIOC_READDVDDUALINFO:
			rc=sceCdReadDvdDualInfo(&value,(unsigned int*)bufp)==0?(-EIO):0;
			break;
		case CDIOC_INIT:
			rc=sceCdInit(*(int*)argp)==0?(-EIO):0;
			break;
		case CDIOC_GETINTREVENTFLG:
			*(int*)bufp=cdvdman_intr_ef;	//Bug: Why does removing this cause some weird crash during IOP resets? Sony didn't document this function, so surely it can be removed? :/
			break;
		case CDIOC_APPLYSCMD:
			rc=sceCdApplySCmd(*(unsigned char*)argp,&((int*)argp)[1],arglen-4,bufp)?-EIO:0;
			break;
		/* Any DEVCTL call here is not processed here is unsupported and emulated. Do not return an error code. */
		default:
			DEBUG_PRINTF("PS2ESDL_CDVDMAN: WARNING! Unsupported devctl call code: 0x%x\n",cmd);
	}

	return rc;
}

int cdrom_ioctl2(iop_file_t *f,int command,void *argp,unsigned int arglen,void *bufp,unsigned int buflen){
	int result;

	DEBUG_PRINTF("PS2ESDL_CDVDMAN: cdrom0 device: IOPCTL2 request received. Command=0x%x\n",command);
	result=0;

	if(!(((struct cdvdman_fhandle *)f->privdata)->flags&8)){
		result=-EIO;
		goto ioctl2_end;
	}

	switch(command){
		case CIOCSTREAMPAUSE:
			result=sceCdStPause();
			break;
		case CIOCSTREAMRESUME:
			result=sceCdStResume();
			break;
		case CIOCSTREAMSTAT:
			result=sceCdStStat();
			break;
		default:
		DEBUG_PRINTF("PS2ESDL_CDVDMAN: Unsupported IOCTL2 call.\n");
	}

ioctl2_end:
	return result;
}

/* Dummy IOP device call entry */
int cdrom_nulldev(){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Invalid cdrom0 device function call.\n");
	return -EIO;
}

/* Dummy IOP device call entry */
s64 cdrom_nulldev64(void){
	DEBUG_PRINTF("PS2ESDL_CDVDMAN: Invalid cdrom0 device 64 function call.\n");
	return -EIO;
}

