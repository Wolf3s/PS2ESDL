IOP_BIN  = cdvdman.irx
IOP_OBJS = cdvdman.o device.o cdvdman_internal.o cdvdman_main.o imports.o exports.o sector_io.o $(USBSTOR_OBJS) $(USBD_OBJS) $(dummy_IRX)

USBSTOR_OBJS = USBSTOR/usb_driver.o
USBD_OBJS = USBD/hcd.o USBD/hub.o USBD/interface.o USBD/mem.o USBD/usbd.o USBD/usbio.o USBD/driver.o USBD/exports.o
dummy_IRX = dummy_IRX_asm.o

IOP_CFLAGS  += -Wall -fno-builtin -Os -mno-check-zero-division
IOP_LDFLAGS += -s
IOP_INCS += -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS) dummy_IRX_asm.s
	make clean -C dummy_IRX

dummy_IRX_asm.s:
	make -C dummy_IRX
	bin2s dummy_IRX/dummy_IRX.irx dummy_IRX_asm.s dummy_irx

include $(PS2SDK)/Defs.make
include Rules.make
