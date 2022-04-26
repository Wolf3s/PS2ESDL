IOP_BIN  = udnl.irx
IOP_OBJS = udnl.o udnl_asm.o imports.o ioprp_img.o

IOP_CFLAGS  += -Wall -fno-builtin -Os -G65536 -mgpopt -mno-check-zero-division
IOP_LDFLAGS += -s
IOP_INCS += -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include -I./include

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS) ioprp_img.c

ioprp_img.c:
	bin2c ioprp.img ioprp_img.c IOPRP_img

include $(PS2SDK)/Defs.make
include Rules.make
