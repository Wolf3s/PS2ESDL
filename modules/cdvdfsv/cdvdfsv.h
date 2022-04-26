//#define DEBUG_TTY_FEEDBACK /* Comment out to disable generation of debug TTY feedback */

#ifdef DEBUG_TTY_FEEDBACK
 #define DEBUG_PRINTF(args...) printf(args)
#else
 #define DEBUG_PRINTF(args...)
#endif

#define ASYNC_CMD_READEE 0x86   /* Used to signal CDVDMAN for a DMA transfer to EE memory */

#define CDVDFSV_VERSION 0x225 /* Version number of this implementation of CDVDFSV */

#define KE_ILLEGAL_PRIORITY   (-403)

#define TOC_BUFFSIZE            21068                           /* Real size unknown */

/* NOTE!! This structure below should be the same as the one used by the RPC callback (When the call to sceCdRead() ends) function on the EE. */
struct _cdvd_read_data /* Total size: 144 bytes; The EE expects the structure to be 144 bytes in size (Even though the client may seem to be able to accomodate 256 bytes of data). */
{
        u32	size1;
        u32	size2;
        void	*dest1;
        void	*dest2;
        unsigned char buffer1[64];
	unsigned char buffer2[64];
};

/* Internal CDVDFSV structures */
struct cdvdfsv_ee_read_params{
  u32 lsn;
  u32 sectors;
  void *buffer;
  cd_read_mode_t *read_mode;

  struct _cdvd_read_data *cb_dataAlignFix;	/* Data containing information on how to "glue back" together the data transfered to the EE (When an unaligned buffer is specified, CDVDFSV is required to read the data into 64-byte aligned buffers. And DMA transfer the parts that are outside the bounaries to the EE for re-assembly with the main bulk of data). */
  u32 *readpos;					/* Current buffer offset, as it would be returned by sceCdGetReadPos(). */
};

struct cdvdfsv_ee_Ncmd_params{
 unsigned char command;
 unsigned short ndlen;
 void *ndata[0];
};

/* size: 0x124 */
typedef struct fsrpc_param
{
       cd_file_t fp;
       char name[0x100];
       void *addr;
} FSVRpc4Prm;

/* size: 0x128 */
typedef struct adv_fsrpc_param
{
       adv_cd_file_t fp;
       char name[0x100];
       void *addr;
} adv_FSVRpc4Prm;

/* size: 0x12C */
typedef struct advl_fsrpc_param
{
       adv_cd_file_t fp;
       char name[0x100];
       void *addr;
       int layer;
} advl_FSVRpc4Prm;

typedef struct _init_io_data
{
 int result;
 int cdvdfsv_ver;
 int cdvdman_ver;
 int debug_mode;
} CDVDInitResult;

typedef struct _dev_ready_data
{
 int result;
 int padding[3];
} CDVDReadyResult;

typedef struct _nonblock_io_data
{
 int result;
 /* union
 { */
  int on_dvd;
 /* } buf; */
} CDVDReadResult;

typedef struct _block_io_data
{
 int result;
 union
 {
   cd_clock_t rtc;
   unsigned int traychk;
   int stat;
   char buffer[8];
  struct
  {
    int on_dual;
    unsigned int layer1_start;
  } dualinfo;
 } buf;
} CDVDCmdResult;

typedef struct _fs_search_data
{
       int result;
       int padding[3];
} CDVDSearchResult;

/* Handler type definitions. */
typedef void (*NcmdHandler)(void *buffer, CDVDReadResult* ReadResult);
typedef void (*ScmdHandler)(void* buffer, int size, CDVDCmdResult* CommandResult);

/* Function prototypes of RPC functions */
void cdvdfsv_rpc1_th();
void cdvdfsv_rpc2_th();
void cdvdfsv_rpc3_th();

/* Function prototypes of exported functions */
int sceCdChangeThreadPriority(int prio);
void dummy_function(void);

/* Function prototypes of internal functions */
void cdvdfsv_main_th(void *args);
void cdvdfsv_remrpcs(void);

/* Internal CDVDFSV RPC #3 functions */
void cdvdfsv_readclock(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_disktype(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_geterror(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_trayreq(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_cdri(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_applyscmd(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_cdstatus(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_cdBreak(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_cdrm(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_poweroff(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_cdmmode(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_cdvdfsv5(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_cdReadGUID(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_cdReadModelID(void* buffer, int size, CDVDCmdResult* rd);
void cdvdfsv_dualinfo(void* buffer, int size, CDVDCmdResult* rd);

/* Internal CDVDFSV RPC #5 functions */
void cdvdfsv_readee(void *,CDVDReadResult *);
void cdvdfsv_gettoc(void *,CDVDReadResult *);
void cdvdfsv_cdSeek(void *buf, CDVDReadResult *rd);
void cdvdfsv_cdStandby(void *buf, CDVDReadResult *rd);
void cdvdfsv_cdStop(void *buf, CDVDReadResult *rd);
void cdvdfsv_cdPause(void *buf, CDVDReadResult *rd);
void cdvdfsv_applyncmd(void *,CDVDReadResult *);
void cdvdfsv_iopmread(void *,CDVDReadResult *);
void cdvdfsv_readchain(void *,CDVDReadResult *);
void cdvdfsv_cdDiskReady(void *buf, CDVDReadResult *rd);
void cdvdfsv_sceCdStream(void *,CDVDReadResult *);
