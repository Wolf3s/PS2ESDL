/* Streaming control codes for the "cdrom_stm0:" device */
#define CDIOC_CDSTREAM0             0x4393                      /* IOP STREAM */
#define CDIOC_CDSTREAD0             0x4394                      /* IOP STREAM READ */
#define CDIOC_CDSTREAM1             0x4396                      /* EE STREAM */
#define CDIOC_CDDASTREAM            0x4398                      /* CDDA EE STREAM */

#define STM0_ARG_MODE               0x03

/* Modes for sceCdStream0() */
#define STM0_MODE_START             0x01
#define STM0_MODE_READ              0x02
#define STM0_MODE_STOP              0x03
#define STM0_MODE_SEEK              0x04
#define STM0_MODE_INIT              0x05
#define STM0_MODE_STAT              0x06
#define STM0_MODE_PAUSE             0x07
#define STM0_MODE_RESUME            0x08
#define STM0_MODE_SEEKF             0x09

/* for "cdrom" ioctl2 */
#define CIOCSTREAMPAUSE       0x630D
#define CIOCSTREAMRESUME      0x630E
#define CIOCSTREAMSTAT        0x630F

/* Streaming modes for cdStRead() */
#define CDVD_STREAM_NONBLOCK	0
#define CDVD_STREAM_BLOCK	1

struct RPC_stream_cmd{
 u32 lsn;
 u32 nsectors;
 void *buffer;
 u32 cmd;
 cd_read_mode_t mode;
 u32 pad;
};

/* Stream commands. */
typedef enum {
	CDVD_ST_CMD_START = 1,
	CDVD_ST_CMD_READ,
	CDVD_ST_CMD_STOP,
	CDVD_ST_CMD_SEEK,
	CDVD_ST_CMD_INIT,
	CDVD_ST_CMD_STAT,
	CDVD_ST_CMD_PAUSE,
	CDVD_ST_CMD_RESUME,
	CDVD_ST_CMD_SEEKF
} CdvdStCmd_t;

/* Stream initilization parameters */
struct RPC_stream_init{
 u32 bufmax;
 u32 bankmax;
 void *buffer;
};
