#include "new_cdvdman.h"
#include <errno.h>
#include "new_ioman.h"
#include <irx.h>
#include <loadcore.h>
#include <sifcmd.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <types.h>
#include <thbase.h>
#include <thevent.h>

#include "cdvdman_hdr.h"
#include "sector_io.h"

static struct ext_dev_conf usb_devConfig={
	/* part_LBA[9] */
	{
		0xAAAAAAAA,
		0xBBBBBBBB,
		0xCCCCCCCC,
		0xDDDDDDDD,
		0xEEEEEEEE,
		0xFFFFFFFF,
		0x00000000,
		0x11111111,
		0x22222222,
	},

	512,		/* sector_sz */
	4096,		/* xfer_blk_sz */

	MAX_SECTORS,	/* sectorsPerSlice */

	NULL,		/* dev_driver_cfg */
};

static void *PartialReadBuffer=NULL;

inline void read_sector(unsigned int lbn, unsigned int sectors, void *buffer, unsigned int *bufferOffset_out){
	unsigned int sectorsLeft=sectors, bytesToRead;
	void *destination;
	u8 slice=0, DiskSectorsToRead, SectorsToRead;	/* Note: This might not be very ideal, but these sector counters are declared as 8-bit variables because they will never amount to exceed a size of 4096 bytes. */
	s64 temp;	/* This must remain as a signed 64-bit integer, or overflows might occur if usb_devConfig.sectorsPerSlice is large enough. */

	IO_DEBUG_PRINTF("read LBN: 0x%08lx; sectors: %lu; buffer: 0x%p\n", lbn, sectors, buffer);

	while(sectorsLeft>0){ 
		bytesToRead=((sectorsLeft<<11)>usb_devConfig.xfer_blk_sz)?(usb_devConfig.xfer_blk_sz):(sectorsLeft<<11);
		SectorsToRead=bytesToRead>>11;
		slice=lbn/usb_devConfig.sectorsPerSlice;

		if(bytesToRead<usb_devConfig.sector_sz){ /* If the disk's sector size is greater than the size of the number of bytes being read this round. */
			destination=PartialReadBuffer;
			DiskSectorsToRead=1;
		}
		else{
			destination=buffer;
			DiskSectorsToRead=bytesToRead/usb_devConfig.sector_sz;

			/* Do not read beyond the end of the slice. */
			temp=((s64)(lbn%usb_devConfig.sectorsPerSlice)+SectorsToRead)-usb_devConfig.sectorsPerSlice;	/* Use 64-bit calculations. */
			if(temp>0) DiskSectorsToRead-=temp;
		}

		IO_DEBUG_PRINTF("Slice: %u; LBA (USB Dev): 0x%08lx, LBA: 0x%08lx; Sector sz: %d; Sectors (USB Dev): %lu; sectors: %u; Buffer: 0x%p. bytestoread: %lu.\n", slice, usb_devConfig.part_LBA[slice] + ((lbn-slice*usb_devConfig.sectorsPerSlice)<<11)/usb_devConfig.sector_sz, lbn, usb_devConfig.sector_sz, bytesToRead/usb_devConfig.sector_sz, SectorsToRead, buffer, bytesToRead);

		/* Assume that the read request does not fail. */
		cbw_scsi_read_sector(usb_devConfig.part_LBA[slice] + ((lbn-slice*usb_devConfig.sectorsPerSlice)<<11)/usb_devConfig.sector_sz, destination, DiskSectorsToRead);

		if(bytesToRead<usb_devConfig.sector_sz){
			memcpy(buffer, destination, bytesToRead);
		}

		lbn+=SectorsToRead;
		sectorsLeft-=SectorsToRead;
		buffer+=bytesToRead;
		if(bufferOffset_out!=NULL) (*bufferOffset_out)+=bytesToRead;
	}

	IO_DEBUG_PRINTF("Reading complete.\n");
}

#if 0
/* Nothing to be done. */
inline int seek_sector(unsigned int lbn){
	return lbn;
}
#endif

inline void device_init(void){
	if(usb_devConfig.sector_sz>PHYSECSIZE){
		PartialReadBuffer=malloc(usb_devConfig.sector_sz);
	}

	initUSBD();
	InitUSBSTOR();
	while((usb_devConfig.sector_sz=mass_stor_configureNextDevice())==0) DelayThread(200); /* Wait for a USB device to be connected, configured and ready */
}

#if 0
/* Nothing to deinitilize. */
void device_deinit(void){
	if(PartialReadBuffer!=NULL) free(PartialReadBuffer);
}
#endif
