# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: Makefile 577 2004-09-14 14:41:46Z pixel $


IOP_OBJS_DIR = ./
IOP_BIN_DIR = ./
IOP_SRC_DIR = ./
IOP_INC_DIR = ./

IOP_BIN  = eesync.irx
IOP_OBJS = eesync.o imports.o

IOP_CFLAGS  += -Wall -fno-builtin -G0 -Os
IOP_LDFLAGS += -s

all: $(IOP_OBJS_DIR) $(IOP_BIN_DIR) $(IOP_BIN)

clean:
	rm -f *.o *.bak *.irx

include $(PS2SDK)/Defs.make
include Rules.make
