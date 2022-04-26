//#define IO_DEBUG_TTY_FEEDBACK /* Comment out to disable generation of debug TTY messages */

#ifdef IO_DEBUG_TTY_FEEDBACK
 #define IO_DEBUG_PRINTF(args...) printf(args)
#else
 #define IO_DEBUG_PRINTF(args...)
#endif

/* External device I/O functions */
inline void read_sector(unsigned int lbn, unsigned int sectors, void *buffer, unsigned int *bufferOffset_out);
inline int seek_sector(unsigned int lbn);
inline void device_init(void);
/* void device_deinit(void); */

/* Device control function prototypes */
int cbw_scsi_read_sector(unsigned int lba, void* buffer, unsigned int sectorCount);
u16 mass_stor_configureNextDevice(void);
void InitUSBSTOR(void);
void initUSBD(void); /* Initilze USBD */

#define SYSTEM_PS2ESDL          0
#define SYSTEM_USBADVANCE       1
#define SYSTEM_INVALID          0xFF

struct ext_dev_conf{
	u32	part_LBA[9];

	/* USB device related */
	u16	sector_sz;
	u16	xfer_blk_sz; /* Maximum number of bytes requested from device at any 1 time */

	/* Emulation standard related */
	u32 sectorsPerSlice; /* Number of sectors in 1 slice */

	void *dev_driver_cfg;
};
