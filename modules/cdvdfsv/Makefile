IOP_BIN  = cdvdfsv.irx
IOP_OBJS = cdvdfsv.o cdvdfsv_internal.o cdvdfsv_main.o imports.o exports.o

IOP_CFLAGS += -Wall -fno-builtin -Os -mno-check-zero-division
IOP_LDFLAGS += -s
IOP_INCS += -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

include $(PS2SDK)/Defs.make
include Rules.make
