/*
 * usb_driver.c - USB Mass Storage Driver
 * se  usbmass-ufi10.pdf
 */

#include <sysclib.h>
#include <thsemap.h>
#include <stdio.h>
#include <sysmem.h>
#include "internal_usbd.h"
#include <usbd_macro.h>

#include <loadcore.h>

//#define DEBUG  //comment out this line when not debugging

#include "mass_debug.h"
#include "usbhd_common.h"
#include "mass_stor.h"

#define getBI32(__buf) ((((u8 *) (__buf))[3] << 0) | (((u8 *) (__buf))[2] << 8) | (((u8 *) (__buf))[1] << 16) | (((u8 *) (__buf))[0] << 24))

#define USB_SUBCLASS_MASS_RBC		0x01
#define USB_SUBCLASS_MASS_ATAPI		0x02
#define USB_SUBCLASS_MASS_QIC		0x03
#define USB_SUBCLASS_MASS_UFI		0x04
#define USB_SUBCLASS_MASS_SFF_8070I 	0x05
#define USB_SUBCLASS_MASS_SCSI		0x06

#define USB_PROTOCOL_MASS_CBI		0x00
#define USB_PROTOCOL_MASS_CBI_NO_CCI	0x01
#define USB_PROTOCOL_MASS_BULK_ONLY	0x50

#define TAG_TEST_UNIT_READY     0
#define TAG_REQUEST_SENSE	3
#define TAG_INQUIRY		18
#define TAG_READ_CAPACITY       37
#define TAG_READ		40
#define TAG_START_STOP_UNIT	33
#define TAG_WRITE		42

#define DEVICE_DETECTED		1
#define DEVICE_CONFIGURED	2

#define CBW_TAG 0x43425355
#define CSW_TAG 0x53425355

typedef struct _cbw_packet {
	unsigned int signature;
	unsigned int tag;
	unsigned int dataTransferLength;
	unsigned char flags;            //80->data in,  00->out
	unsigned char lun;
	unsigned char comLength;		//command data length
	unsigned char comData[16];		//command data
} cbw_packet __attribute__((packed));

typedef struct _csw_packet {
	unsigned int signature;
	unsigned int tag;
	unsigned int dataResidue;
	unsigned char status;
} csw_packet __attribute__((packed));

typedef struct _inquiry_data {
    u8 peripheral_device_type;  // 00h - Direct access (Floppy), 1Fh none (no FDD connected)
    u8 removable_media; // 80h - removeable
    u8 iso_ecma_ansi;
    u8 repsonse_data_format;
    u8 additional_length;
    u8 res[3];
    u8 vendor[8];
    u8 product[16];
    u8 revision[4];
} inquiry_data __attribute__((packed));

typedef struct _sense_data {
    u8 error_code;
    u8 res1;
    u8 sense_key;
    u8 information[4];
    u8 add_sense_len;
    u8 res3[4];
    u8 add_sense_code;
    u8 add_sense_qual;
    u8 res4[4];
} sense_data __attribute__((packed));

typedef struct _read_capacity_data {
    u8 last_lba[4];
    u8 block_length[4];
} read_capacity_data __attribute__((packed));

static UsbDriver driver={
    NULL,			/* driver.next */
    NULL,			/* driver.prev */
    "mass-stor",		/* driver.name */
    &mass_stor_probe,		/* driver.probe */
    &mass_stor_connect,		/* driver.connect */
    NULL,			/* driver.disconnect */
};

typedef struct _usb_callback_data {
    int semh;
    int returnCode;
    int returnSize;
} usb_callback_data;

/* Only 1 USB device supported */
static mass_dev g_mass_device;

/* For semaphore creation */
static iop_sema_t sema={
	0,	/* s.initial */
	1,	/* s.max */
	0,	/* s.option */
	0,	/* s.attr */
};

static void usb_callback(int resultCode, int bytes, void *arg)
{
	usb_callback_data* data = (usb_callback_data*) arg;
	data->returnCode = resultCode;
	data->returnSize = bytes;
	XPRINTF("USBHDFSD: callback: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);
	SignalSema(data->semh);
}

static inline void usb_set_configuration(int configNumber){
	usb_callback_data cb_data;

	cb_data.semh = CreateSema(&sema);

	XPRINTF("USBHDFSD: setting configuration controlEp=%i, confNum=%i \n", g_mass_device.controlEp, configNumber);
	UsbSetDeviceConfiguration(g_mass_device.controlEp, configNumber, usb_callback, (void*)&cb_data);

	WaitSema(cb_data.semh);
	DeleteSema(cb_data.semh);
}

static inline void usb_set_interface(int interface, int altSetting) {
	usb_callback_data cb_data;

	cb_data.semh = CreateSema(&sema);

	XPRINTF("USBHDFSD: setting interface controlEp=%i, interface=%i altSetting=%i\n", g_mass_device.controlEp, interface, altSetting);
	UsbSetInterface(g_mass_device.controlEp, interface, altSetting, usb_callback, (void*)&cb_data);

	WaitSema(cb_data.semh);
	DeleteSema(cb_data.semh);
}

static inline int usb_bulk_status(csw_packet* csw, int tag) {
	usb_callback_data cb_data;

	cb_data.semh = CreateSema(&sema);

	csw->signature = CSW_TAG;
	csw->tag = tag;
	csw->dataResidue = 0;
	csw->status = 0;

	UsbBulkTransfer(
		g_mass_device.bulkEpI,	//bulk input pipe
		csw,			//data ptr
		13,			//data length
		usb_callback,
		(void*)&cb_data
        );

	WaitSema(cb_data.semh);
	DeleteSema(cb_data.semh);

    	XPRINTF("USBHDFSD: bulk csw.status: %i\n", csw->status);
    	return csw->status;

}

/* see flow chart in the usbmassbulk_10.pdf doc (page 15) */
static int usb_bulk_manage_status(int tag){
	csw_packet csw;

	//XPRINTF("USBHDFSD: usb_bulk_manage_status 1 ...\n");
	return(usb_bulk_status(&csw, tag)); /* Attempt to read CSW from bulk in endpoint */
}

static void usb_bulk_command(cbw_packet* packet ) {
	usb_callback_data cb_data;
	int result;

	cb_data.semh = CreateSema(&sema);

	result=UsbBulkTransfer(
		g_mass_device.bulkEpO,	//bulk output pipe
		packet,			//data ptr
		31,			//data length
		usb_callback,
		(void*)&cb_data
	);

	WaitSema(cb_data.semh);
	DeleteSema(cb_data.semh);
}

static int usb_bulk_command_transfer(void* buffer, int transferSize, cbw_packet *cbw){
	int blockSize = transferSize;
	int offset = 0, result;
	usb_callback_data cb_data;

	usb_bulk_command(cbw);

	cb_data.semh = CreateSema(&sema);

	while (transferSize > 0) {
		if (transferSize < blockSize) {
			blockSize = transferSize;
		}

		result = UsbBulkTransfer(
			g_mass_device.bulkEpI,	//bulk pipe epI(Read)  epO(Write)
			(buffer + offset),	//data ptr
			blockSize,		//data length
			usb_callback,
			(void*)&cb_data
		);

		if(result==0) WaitSema(cb_data.semh);
		else{
			DeleteSema(cb_data.semh);
			return -1;
		}

		//XPRINTF("USBHDFSD: retCode=%i retSize=%i \n", cb_data.returnCode, cb_data.returnSize);

		if (cb_data.returnCode > 0) break;

		offset += cb_data.returnSize;
		transferSize-= cb_data.returnSize;
	}

	DeleteSema(cb_data.semh);

	return cb_data.returnCode;
}

static inline int cbw_scsi_test_unit_ready(void)
{
    static cbw_packet cbw={
	CBW_TAG, 		/* cbw.signature */
	-TAG_TEST_UNIT_READY,	/* cbw.tag */
	0,			/* cbw.dataTransferLength; TUR_REPLY_LENGTH */
	0x80,			/* cbw.flags; Data is to be received. */
	0,			/* cbw.lun */
	12,			/* cbw.comLength */

	/* scsi command packet */
	{
		0x00,		/* test unit ready operation code; cbw.comData[0] */
		0,		/* lun/reserved; cbw.comData[1] */
		0,		/* reserved; cbw.comData[2] */
		0,		/* reserved; cbw.comData[3] */
		0,		/* reserved; cbw.comData[4] */
		0,		/* reserved; cbw.comData[5] */
		0,		/* reserved; cbw.comData[6] */
		0,		/* reserved; cbw.comData[7] */
		0,		/* reserved; cbw.comData[8] */
		0,		/* reserved; cbw.comData[9] */
		0,		/* reserved; cbw.comData[10] */
		0,		/* reserved; cbw.comData[11] */
	}
    };
    
    XPRINTF("USBHDFSD: cbw_scsi_test_unit_ready\n");

    usb_bulk_command(&cbw);

    return(usb_bulk_manage_status(-TAG_TEST_UNIT_READY));
}

static inline int cbw_scsi_request_sense(void *buffer, int size)
{
    static cbw_packet cbw={
	CBW_TAG, 		/* cbw.signature */
	-TAG_REQUEST_SENSE,	/* cbw.tag */
	0,			/* cbw.dataTransferLength; REQUEST_SENSE_REPLY_LENGTH */
	0x80,			/* cbw.flags; Data is to be received. */
	0,			/* cbw.lun */
	12,			/* cbw.comLength */

	/* scsi command packet */
	{
		0x03,		/* request sense operation code; cbw.comData[0] */
		0,		/* lun/reserved; cbw.comData[1] */
		0,		/* reserved; cbw.comData[2] */
		0,		/* reserved; cbw.comData[3] */
		0,		/* allocation length; cbw.comData[4] */
		0,		/* reserved; cbw.comData[5] */
		0,		/* reserved; cbw.comData[6] */
		0,		/* reserved; cbw.comData[7] */
		0,		/* reserved; cbw.comData[8] */
		0,		/* reserved; cbw.comData[9] */
		0,		/* reserved; cbw.comData[10] */
		0,		/* reserved; cbw.comData[11] */
	}
    };

    /* Configure some non-constant variables... */
    cbw.dataTransferLength = size;	/* cbw.dataTransferLength; REQUEST_SENSE_REPLY_LENGTH */
    cbw.comData[4] = size;		/* allocation length */

    XPRINTF("USBHDFSD: cbw_scsi_request_sense\n");

    usb_bulk_command_transfer(buffer, size, &cbw);

    return(usb_bulk_manage_status(-TAG_REQUEST_SENSE));
}

static inline int cbw_scsi_read_capacity(void *buffer, int size)
{
    int ret;
    int retryCount;

    static cbw_packet cbw={
	CBW_TAG, 		/* cbw.signature */
	-TAG_READ_CAPACITY,	/* cbw.tag */
	0,			/* cbw.dataTransferLength; READ_CAPACITY_REPLY_LENGTH */
	0x80,			/* cbw.flags; Data is to be received. */
	0,			/* cbw.lun */
	12,			/* cbw.comLength */

	/* scsi command packet */
	{
		0x25,		/* read capacity operation code; cbw.comData[0] */
		0,		/* lun/reserved/RelAdr; cbw.comData[1] */
		0,		/* LBA 1; cbw.comData[2] */
		0,		/* LBA 2; cbw.comData[3] */
		0,		/* LBA 3; cbw.comData[4] */
		0,		/* LBA 4; cbw.comData[5] */
		0,		/* reserved; cbw.comData[6] */
		0,		/* reserved; cbw.comData[7] */
		0,		/* reserved/PMI; cbw.comData[8] */
		0,		/* reserved; cbw.comData[9] */
		0,		/* reserved; cbw.comData[10] */
		0,		/* reserved; cbw.comData[11] */
	}
    };

    /* Configure some non-constant variables... */
    cbw.dataTransferLength = size;	/* cbw.dataTransferLength; READ_CAPACITY_REPLY_LENGTH */

    XPRINTF("USBHDFSD: cbw_scsi_read_capacity\n");

	ret = 1;
	retryCount = 6;
	while (ret != 0 && retryCount > 0) {
		usb_bulk_command_transfer(buffer, size, &cbw);

		//HACK HACK HACK !!!
		//according to usb doc we should allways
		//attempt to read the CSW packet. But in some cases
		//reading of CSW packet just freeze ps2....:-(

		ret = usb_bulk_manage_status(-TAG_READ_CAPACITY);
		retryCount--;
	}
    return ret;
}

int cbw_scsi_read_sector(unsigned int lba, void* buffer, unsigned int sectorCount)
{
    static cbw_packet cbw={
	CBW_TAG, 		/* cbw.signature */
	-TAG_READ,		/* cbw.tag */
	0,			/* cbw.dataTransferLength; INQUIRY_REPLY_LENGTH */
	0x80,			/* cbw.flags; Data is to be received. */
	0,			/* cbw.lun */
	12,			/* cbw.comLength */

	/* scsi command packet */
	{
		0x28,		/* Operation code; cbw.comData[0] */
		0,		/* LUN/DPO/FUA/Reserved/Reldr; cbw.comData[1] */
		0,		/* lba 1 (MSB); cbw.comData[2] */
		0,		/* lba 2; cbw.comData[3] */
		0,		/* lba 3; cbw.comData[4] */
		0,		/* lba 4 (LSB); cbw.comData[5] */
		0,		/* reserved; cbw.comData[6] */
		0,		/* Transfer length MSB; cbw.comData[7] */
		0,		/* Transfer length LSB; cbw.comData[8] */
		0,		/* reserved; cbw.comData[9] */
		0,		/* reserved; cbw.comData[10] */
		0,		/* reserved; cbw.comData[11] */
	}
    };
	XPRINTF("USBHDFSD: cbw_scsi_sector_read: LBA= 0x%08x; sector sz= %u; sectors: %u.\n", lba, g_mass_device.sectorSize, sectorCount);

	cbw.dataTransferLength = g_mass_device.sectorSize * sectorCount;

	//scsi command packet
	cbw.comData[2] = (lba & 0xFF000000) >> 24;	//lba 1 (MSB)
	cbw.comData[3] = (lba & 0xFF0000) >> 16;	//lba 2
	cbw.comData[4] = (lba & 0xFF00) >> 8;		//lba 3
	cbw.comData[5] = (lba & 0xFF);			//lba 4 (LSB)
	cbw.comData[7] = (sectorCount & 0xFF00) >> 8;	//Transfer length MSB
	cbw.comData[8] = (sectorCount & 0xFF);		//Transfer length LSB

	usb_bulk_command_transfer(buffer, cbw.dataTransferLength, &cbw);

	return(usb_bulk_manage_status(cbw.tag));
}

static inline mass_dev* mass_stor_findDevice(int devId, int create)
{
    mass_dev* dev = NULL;
    XPRINTF("USBHDFSD: mass_stor_findDevice devId %i\n", devId);

    if (g_mass_device.devId == devId)
    {
        XPRINTF("USBHDFSD: mass_stor_findDevice exists.\n");
        return &g_mass_device;
    }
    else if (create && dev == NULL && g_mass_device.devId == -1)
    {
        dev = &g_mass_device;
    }
    return dev;
}

/* test that endpoint is bulk endpoint and if so, update device info */
static void usb_bulk_probeEndpoint(int devId, mass_dev* dev, UsbEndpointDescriptor* endpoint) {
	if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
		/* out transfer */
		if ((endpoint->bEndpointAddress & 0x80) == 0 && dev->bulkEpO < 0) {
			dev->bulkEpOAddr = endpoint->bEndpointAddress;
			dev->bulkEpO = UsbOpenEndpointAligned(devId, endpoint);
			dev->packetSzO = endpoint->wMaxPacketSizeHB * 256 + endpoint->wMaxPacketSizeLB;
			XPRINTF("USBHDFSD: register Output endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpO,dev->bulkEpOAddr, dev->packetSzO);
		} else
		/* in transfer */
		if ((endpoint->bEndpointAddress & 0x80) != 0 && dev->bulkEpI < 0) {
			dev->bulkEpIAddr = endpoint->bEndpointAddress;
			dev->bulkEpI = UsbOpenEndpointAligned(devId, endpoint);
			dev->packetSzI = endpoint->wMaxPacketSizeHB * 256 + endpoint->wMaxPacketSizeLB;
			XPRINTF("USBHDFSD: register Input endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpI, dev->bulkEpIAddr, dev->packetSzI);
		}
	}
}

int mass_stor_probe(int devId)
{
	UsbDeviceDescriptor *device = NULL;
	UsbConfigDescriptor *config = NULL;
	UsbInterfaceDescriptor *intf = NULL;

	XPRINTF("USBHDFSD: probe: devId=%i\n", devId);

	/* get device descriptor */
	device = (UsbDeviceDescriptor*)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);

	/* Check if the device has at least one configuration */
	if (device->bNumConfigurations < 1) { return 0; }

	/* read configuration */
	config = (UsbConfigDescriptor*)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);

	/* check that at least one interface exists */
	XPRINTF("USBHDFSD: bNumInterfaces %d\n", config->bNumInterfaces);

        /* get interface */
	intf = (UsbInterfaceDescriptor *) ((char *) config + config->bLength); /* Get first interface */
	XPRINTF("USBHDFSD: bInterfaceClass %X bInterfaceSubClass %X bInterfaceProtocol %X\n",
        intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol);

	if ((intf->bInterfaceClass		!= USB_CLASS_MASS_STORAGE) ||
		(intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SCSI  &&
		 intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SFF_8070I) ||
		(intf->bInterfaceProtocol	!= USB_PROTOCOL_MASS_BULK_ONLY) ||
		(intf->bNumEndpoints < 2))    { //one bulk endpoint is not enough because
			return 0;		//we send the CBW to te bulk out endpoint
	}
	return 1;
}

int mass_stor_connect(int devId)
{
	int i;
	int epCount;
	UsbDeviceDescriptor *device;
	UsbConfigDescriptor *config;
	UsbInterfaceDescriptor *interface;
	UsbEndpointDescriptor *endpoint;
	mass_dev* dev;

	XPRINTF("USBHDFSD: connect: devId=%i\n", devId);
	dev = mass_stor_findDevice(devId, 1);

	dev->status = 0;
	dev->sectorSize = 0;

	dev->bulkEpI = -1;
	dev->bulkEpO = -1;

	/* open the config endpoint */
	dev->controlEp = UsbOpenEndpoint(devId, NULL);

	device = (UsbDeviceDescriptor*)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);

	config = (UsbConfigDescriptor*)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);

	interface = (UsbInterfaceDescriptor *) ((char *) config + config->bLength); /* Get first interface */

	// store interface numbers
	dev->interfaceNumber = interface->bInterfaceNumber;
	dev->interfaceAlt    = interface->bAlternateSetting;

	epCount = interface->bNumEndpoints;
	endpoint = (UsbEndpointDescriptor*) UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);
	usb_bulk_probeEndpoint(devId, dev, endpoint);

	for (i = 1; i < epCount; i++)
	{
		endpoint = (UsbEndpointDescriptor*) ((char *) endpoint + endpoint->bLength);
		usb_bulk_probeEndpoint(devId, dev, endpoint);
	}

	/*store current configuration id - can't call set_configuration here */
	dev->devId = devId;
	dev->configId = config->bConfigurationValue;
	dev->status = DEVICE_DETECTED;
	XPRINTF("USBHDFSD: connect ok: epI=%i, epO=%i \n", dev->bulkEpI, dev->bulkEpO);

	return 0;
}

static inline void mass_stor_warmup(void) {
    sense_data sd;
    read_capacity_data rcd;

    XPRINTF("USBHDFSD: mass_stor_warmup\n");

    while(cbw_scsi_test_unit_ready()!=0){} /* Wait for the device to become ready to accept requests. */

    cbw_scsi_request_sense(&sd, sizeof(sense_data));
    cbw_scsi_read_capacity(&rcd, sizeof(read_capacity_data));

    g_mass_device.sectorSize = getBI32(&rcd.block_length);
    g_mass_device.maxLBA     = getBI32(&rcd.last_lba);
    XPRINTF("USBHDFSD: sectorSize %d maxLBA %ld\n", g_mass_device.sectorSize, g_mass_device.maxLBA);
}

u16 mass_stor_configureNextDevice(void)
{
    XPRINTF("USBHDFSD: configuring devices... \n");

    if (g_mass_device.devId != -1 && (g_mass_device.status & DEVICE_DETECTED) && !(g_mass_device.status & DEVICE_CONFIGURED))
    {
        usb_set_configuration(g_mass_device.configId);
        usb_set_interface(g_mass_device.interfaceNumber, g_mass_device.interfaceAlt);
        g_mass_device.status |= DEVICE_CONFIGURED;

        mass_stor_warmup();
        return(g_mass_device.sectorSize);
    }

    return 0;
}

void InitUSBSTOR(void)
{
    g_mass_device.devId = -1;

    XPRINTF("Registering USB device driver...\n");
    UsbRegisterDriver(&driver);

    XPRINTF("USBSTOR initilization complete.\n");
}
