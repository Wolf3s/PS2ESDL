//#define DEBUG_TTY_FEEDBACK /* Comment out to disable generation of debug TTY messages */

#ifdef DEBUG_TTY_FEEDBACK
 #define DEBUG_PRINTF(args...) printf(args)
 #define iDEBUG_PRINTF(args...) Kprintf(args)
#else
 #define DEBUG_PRINTF(args...)
 #define iDEBUG_PRINTF(args...)
#endif

#define CDVDMAN_VERSION 0x225	/* Version number of this implementation of CDVDMAN, used by CDVDFSV */
#define PHYSECSIZE 2048		/* Physical size(Not only the data area) of sectors on disc */

/* Asynchronous thread function from cdvdmanc.c */
void cdvd_asyncthread();

/* For sceCdInit() */
#define SCECdINIT             0x0
#define SCECdEXIT             0x5

/* Bits for CDVDreg_SDATAIN register (read) */
/*
 0x20 - set when there is no data to read in CDVDreg_SDATAOUT register (?) (s-cmd send #2)
 0x40 - set when there is no data to read in CDVDreg_SDATAOUT register (?) (standard)
 0x80 - set when processing S-Command (?)
 */

#define CDL_DATA_RDY          0x1
#define CDL_CD_COMPLETE       0x2
#define CDL_DATA_END          0x4

/* PS2 C/DVD hardware registers */
#define CDVDreg_NCOMMAND      (*(volatile unsigned char *)0xBF402004)
#define CDVDreg_READY         (*(volatile unsigned char *)0xBF402005)
#define CDVDreg_NDATAIN       (*(volatile unsigned char *)0xBF402005)
#define CDVDreg_ERROR         (*(volatile unsigned char *)0xBF402006)
#define CDVDreg_HOWTO         (*(volatile unsigned char *)0xBF402006)
#define CDVDreg_ABORT         (*(volatile unsigned char *)0xBF402007)
#define CDVDreg_PWOFF         (*(volatile unsigned char *)0xBF402008)
#define CDVDreg_9             (*(volatile unsigned char *)0xBF402008)
#define CDVDreg_STATUS        (*(volatile unsigned char *)0xBF40200A)
#define CDVDreg_B             (*(volatile unsigned char *)0xBF40200B)
#define CDVDreg_C             (*(volatile unsigned char *)0xBF40200C)
#define CDVDreg_D             (*(volatile unsigned char *)0xBF40200D)
#define CDVDreg_E             (*(volatile unsigned char *)0xBF40200E)
#define CDVDreg_TYPE          (*(volatile unsigned char *)0xBF40200F)
#define CDVDreg_13            (*(volatile unsigned char *)0xBF402013)
#define CDVDreg_SCOMMAND      (*(volatile unsigned char *)0xBF402016)
#define CDVDreg_SDATAIN       (*(volatile unsigned char *)0xBF402017)
#define CDVDreg_SDATAOUT      (*(volatile unsigned char *)0xBF402018)
#define CDVDreg_KEYSTATE      (*(volatile unsigned char *)0xBF402038)
#define CDVDreg_KEYXOR        (*(volatile unsigned char *)0xBF402039)
#define CDVDreg_DEC           (*(volatile unsigned char *)0xBF40203A)

/* CDVDMAN Error Codes */
#define SCECdErNO             0x00
#define SCECdErABRT           0x01
#define SCECdErCMD            0x10
#define SCECdErOPENS          0x11
#define SCECdErNODISC         0x12
#define SCECdErNORDY          0x13
#define SCECdErCUD            0x14
#define SCECdErIPI            0x20
#define SCECdErILI            0x21
#define SCECdErPRM            0x22
#define SCECdErREAD           0x30
#define SCECdErTRMOPN         0x31
#define SCECdErEOM            0x32
#define SCECdErSFRMTNG        0x38
#define SCECdErREADCF         0xFD
#define SCECdErREADCFR        0xFE

#define SCECdPSCD       0x10
#define SCECdPSCDDA     0x11
#define SCECdPS2CD      0x12
#define SCECdPS2CDDA    0x13
#define SCECdPS2DVD     0x14

#define SCECdComplete 0x02
#define SCECdNotReady 0x06

#define SCECdTrayOpen 0x00
#define SCECdTrayClose 0x01
#define SCECdTrayCheck 0x02

#define SCECdStatStop 0x00
#define SCECdStatShellOpen 0x01
#define SCECdStatSpin 0x02
#define SCECdStatRead 0x06
#define SCECdStatPause 0x0A
#define SCECdStatSeek 0x12
#define SCECdStatEmg 0x20

#define SCECdFuncRead 0x01
#define SCECdFuncSeek 0x04
#define SCECdFuncStandby 0x05
#define SCECdFuncStop 0x06
#define SCECdFuncPause 0x07
#define SCECdFuncBreak 0x08

/* File open flags */
#define FILE_MOD_BANNED (-1)

/* Various CDVDMAN system flags */
#define SYS_EE_RAM_DEST         1 /* Destination is in EE memory */
#define SYS_MEMALIGN_FIX        2

/* CDVDMAN operational modes */
#define SYS_FORCESYN		0x02

/* CD/DVD N-Commands */
#define ASYNC_CMD_NOP_SYNC      0x00
#define ASYNC_CMD_NOP           0x01
#define ASYNC_CMD_STANDBY       0x02
#define ASYNC_CMD_STOP          0x03
#define ASYNC_CMD_PAUSE         0x04
#define ASYNC_CMD_SEEK          0x05
#define ASYNC_CMD_READ          0x06
#define ASYNC_CMD_READFULL      0x07
#define ASYNC_CMD_READDVD       0x08
#define ASYNC_CMD_GETTOC        0x09
#define ASYNC_CMD_READKEY       0x0C
#define ASYNC_CMD_READFULL2     0x0E

/* Special N-commands used by CDVDFSV for PS2ESDL. */
#define ASYNC_CMD_ABORT         0x21

/* Again, this is also for PS2ESDL, but 0x80 is meant to signify that the call originates from the EE */
#define ASYNC_CMD_READEE        0x86

/* Sector sizes, and number of sector in a image file definations */
#define LEGACY_MAX_SECTORS      524288  /* Maximum number of sectors in each disc image slice for legacy (USBAdvance) disc image sets */
#define MAX_SECTORS             913045 /* 913045 sectors of 2352-bytes MAX in each 2GB image slice. */

/* IOP interrupt numbers */
#define INUM_CDROM            0x2

/* Status codes */
#define READY 1
#define BUSY  0

/* Commands for "cdrom" device devctl function */
#define CDIOC_READCLOCK		0x430C
#define CDIOC_READDISKID		0x431D
#define CDIOC_GETDISKTYPE		0x431F
#define CDIOC_GETERROR		0x4320
#define CDIOC_TRAYREQ		0x4321
#define CDIOC_STATUS			0x4322
#define CDIOC_POWEROFF      	0x4323
#define CDIOC_MMODE			0x4324
#define CDIOC_DISKRDY		0x4325
#define CDIOC_READMODELID		0x4326
#define CDIOC_STREAMINIT		0x4327
#define CDIOC_BREAK			0x4328
#define CDIOC_SPINNOM		0x4380
#define CDIOC_SPINSTM		0x4381
#define CDIOC_TRYCNT			0x4382
#define CDIOC_SEEK			0x4383
#define CDIOC_STANDBY		0x4384
#define CDIOC_STOP			0x4385
#define CDIOC_PAUSE			0x4386
#define CDIOC_GETTOC			0x4387
#define CDIOC_SETTIMEOUT		0x4388
#define CDIOC_READDVDDUALINFO	0x4389
#define CDIOC_INIT			0x438A
#define CDIOC_GETINTREVENTFLG	0x4391
#define CDIOC_APPLYSCMD		0x4392
#define CDIOC_FSCACHEINIT		0x4395
#define CDIOC_FSCACHEDELETE		0x4397

struct read_n_command_struct{
        unsigned int     lsn;
        unsigned int     sectors;
        u8      attempts;
        u8      rotateSpd;
        u8      dataPattern;
};

/* NOTE!! This structure below should be identical to the one used by the RPC callback (When the call to sceCdRead() ends) function on the EE. */
struct _cdvd_read_data /* Total size: 144 bytes; The EE expects the structure to be 144 bytes in size (Even though the client may seem to be able to accomodata 256 bytes of data). */
{
        u32     size1;
        u32     size2;
        void    *dest1;
        void    *dest2;
        u8	buffer1[64];
        u8	buffer2[64];
};

struct cdvdfsv_ee_read_params{
  unsigned int lsn;
  unsigned int sectors;
  void *buffer;
  cd_read_mode_t read_mode;

  /* These pointers are pointers to data on the EE!! */
  struct _cdvd_read_data *cb_dataAlignFix; /* Data containing information on how to "glue back" together the data transfered to the EE (When an unaligned buffer is specified, CDVDFSV is required to read the data into 64-byte aligned buffers. And DMA transfer the parts that are outside the bounaries to the EE for re-assembly with the main bulk of data). */
  unsigned int *readpos;         /* Current buffer offset, as it would be returned by sceCdGetReadPos(). */
};


/* Function protytypes for "cdrom0" device */
/* Function prototypes */
int cdrom_init(iop_device_t *);
int cdrom_deinit(iop_device_t *);
int cdrom_open(iop_file_t *,const char *,int);
int cdrom_close(iop_file_t *);
inline void cdrom_dummy_read(void *buf, void *src, int nbyte);
int cdrom_read(iop_file_t *,void *,int);
int cdrom_lseek(iop_file_t *,unsigned long,int);
int cdrom_ioctl(iop_file_t *,unsigned long,void *);
int cdrom_dopen(iop_file_t *,const char *);
int cdrom_dread(iop_file_t *,fio_dirent_t *);
int cdrom_getstat(iop_file_t *,const char *,fio_stat_t *);
int cdrom_devctl(iop_file_t *,const char *,int,void *,unsigned int,void *,unsigned int);
int cdrom_ioctl2(iop_file_t *,int,void *,unsigned int,void *,unsigned int);

int invalid_export();
int cdrom_nulldev();
s64 cdrom_nulldev64();

/* ISO9660 filesystem directory record */
struct iso9660_dirRec{
	unsigned char lenDR;
	unsigned char lenExAttr;

	unsigned long extentLoc_LE;
	unsigned long extentLoc_BE;

	unsigned long dataLen_LE;
	unsigned long dataLen_BE;

	unsigned char recDateTime[7];

	unsigned char fileFlags;
	unsigned char fileUnitSz;
	unsigned char interleaveGapSz;

	unsigned short volumeSeqNum_LE;
	unsigned short volumeSeqNum_BE;

	unsigned char lenFI;
	unsigned char fileIdentifier; /* The actual length is specified by lenFI */

	/* There are more fields after this. */
	/* Padding field; Only present if the length of the file identifier is an even number. */
	/* Field for system use; up to lenDR */
} __attribute__((packed));

/* ISO9660 filesystem file path table */
struct iso9660_pathtable{
	unsigned char lenDI;
	unsigned char lenExRec;

	unsigned long extLoc;

	unsigned short parentDirNum;

	unsigned char dirIdentifier; /* The actual length is specified by lenDI. */

	/* There are more fields after this. */
	/* Padding field; Only present if the length of the directory identifier is an even number. */
} __attribute__((packed));

/* Stores the configuration of the asynchronous thread */
struct cdvdman_async{
  volatile int status;		/* Drive status */
  int drivestate;		/* Drive state (Rotor spinning, drive is seeking or reading data etc). */
  unsigned char asyncCmd;	/* Command that's to be given to the asynchronous thread */
  int asyncthreadID;

  unsigned char ncmd_buf[24];	/* Buffer containing N-command data */

  unsigned char flags;		/* Various system flags */
  int EE_dmatrans_id;		/* DMA transfer ID */

  unsigned int rbuffoffset;		/* Read buffer position indicator */

  /* Streaming support */
  unsigned int stbufmax;			/* Holds the max. buffer size provided by sceCdStInit() */
  unsigned int stbankmax;		/* Holds the number of banks provided by sceCdStInit() */
  u16 max_bank_sectors;		/* Holds the number of sectors that can be held in the ring buffer */
  u8 bank_ID;			/* Holds the current bank number */
  void *stReadBuffer;		/* Read buffer for streaming */
  unsigned int sectorsstreamed;	/* Number of sectors streamed */
  unsigned int stLSN;			/* LSN to start reading/streaming from */

  unsigned int ringbuff_bank_sector_offset;	/* Current offset of the current bank in the streaming ring buffer. */
  unsigned char *ringbuff_bank_ptr;	/* Pointer to the current streaming ring buffer bank (bank 0 or 1) */
};

/* CDVDMAN file handle structure */
struct cdvdman_fhandle{
        unsigned int lsn;        /* LSN of the file */
        unsigned int size;       /* Size of the file */
        unsigned int offset;     /* Current offset position indicator */
        unsigned int flags;      /* File open flags */
};

/* Information on the disc/layer. */
struct discInfo{
	unsigned int   ptableLSN;	/* LSN of the type-L path table. */
	unsigned int   ptable_sz;	/* Size of the type-L path table. */
	unsigned int   rdirdsc;		/* LSN of the root directory record. */
	unsigned int   sectorcount;	/* Total number of sectors on the disc. */
};

/* Stores the CDVDMAN configuration */
struct cdvdman_config{
 int            cdvdman_mmode;		/* Media Mode set by sceCdMmode() call */
 unsigned int            operation_mode;		/* Mode of CDVDMAN operation (Normal or legacy) */

 struct discInfo discInfo[2];		/* Information on the disc/individual layers. */
 unsigned int	layerBreak;			/* Layer break for DVD9 discs only. It's set to 0 when the disc image represents a DVD5 disc. */
 unsigned int	sectorsPerSlice;		/* Maximum number of sectors per disc image slice */

 u8     read_buffer_sz;	/* Size of the read buffer (In 2048-byte sectors). */
 u8     dnas_key[5];	/* DNAS key */
};

struct cdvd_cmdstruct{ /* PS2ESDL's "Lite" version (Look at romz's CDVDMAN psuedo code). */
	unsigned char   cdvdman_cderror;   /* next byte after the cdvdman_command */
	volatile int    cdvdman_waf;       /* +4 */
	unsigned int    cdvdman_readlsn; /* Current Logical Sector Number(LSN) */
	unsigned int    cdvdman_dvdflag;
	void            *cdvdman_rd_buf;

	/* s-cmd related stuff & some unknown variables here*/
	unsigned int    cdvdman_layer1;
	unsigned char   cdvdman_dldvd;
};

/* CDVDMAN's internal functions */
void cdvdman_init(void);
void cdvdman_deinit(void);
void cdvdman_initcfg(void);
unsigned int sceCdLsnDualChg(unsigned int);
int DvdDual_infochk(void);
int send_asynccmd(int ncmd, int *ndata, int ndlen, int cmdfunc, void *buffer);
void cdvdman_read_noCB(unsigned int lsn, unsigned int sectors, void *buf, u8 mode);
unsigned int intr_cb(void *params);
int intrh_cdrom(void *common);
void EE_memcpy(void *dest,void *src,unsigned int nbytes);
int cdvdman_send_scmd(unsigned char cmd, unsigned char *sdata, unsigned int sdlen, unsigned char *rdata, unsigned int rdlen);
int clear_ev_flag(int evfid,unsigned int bitpattern);
int refer_ef_status(int evfid,iop_event_info_t *info);
int wakeup_thread(int thid);
int cdvdman_CdSearchFile(cd_file_t *fp, const char *path, int layer, u8 *fileFlags);
int cdvdman_CdReturnFileInfo(cd_file_t *fp, const char *fname, unsigned int dirRecLsn, unsigned int mode, unsigned int layer, u8 *fileFlags);
void cdvdman_getstat(fio_stat_t *stat, cd_file_t *fileinfo, u8 fileFlags);
int cdvdman_open(iop_file_t *f, const char *name, int mode);
void stReadRingBuffer(unsigned int lsn, unsigned int sectors, void *buffer);
void stInit(unsigned int lsn);
void EE_DMASync(void);
int cdvdman_readConID(u64 *guid, unsigned int *modelID);
void *malloc(unsigned int NumBytes);
void free(void *buffer);

/* Asynchronous thread functions */
inline void async_lseek(void);
inline void async_read(void);
