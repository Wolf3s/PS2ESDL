EE_BIN = PS2ESDL.elf

# FOR DEBUGGING: Replace "GUI.o" in the line below with "loader.o" to do a "blind" test... (Without a screen)
EE_OBJS = GUI.o system.o graphics.o krom.o configure.o OSDHistory.o $(EE_IOP_OBJS) $(EE_MEDIA_OBJS) EE_core_elf.o USBExtreme_format.o PS2ESDL_format.o ISO9660.o plugin.o sort.o skin.o crc16.o

#IOP modules
EE_IOP_OBJS = UDNL_irx.o

#Media resources
EE_MEDIA_OBJS = graphics/mbox.o graphics/icon_sys_J.o graphics/icon_sys_A.o

EE_INCS := -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include
EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -s
EE_LIBS = -lpatches -lcdvd -lpad -lgskit -lgskit_toolkit -ldmakit -lpng -lz -lm -lmc -ltiff -lm
EE_CFLAGS += -G16 -mgpopt

all:
	$(MAKE) $(EE_BIN)
	ee-strip -s -d -R .mdebug.eabi64 -R .reginfo -R .comment $(EE_BIN)

EE_core_elf.s:
	mkdir -p ../EE_core/bin
	bin2s ../EE_core/EE_core.elf ../EE_core/bin/EE_core.s EE_core_elf
	cp ../EE_core/bin/EE_core.s ./EE_core_elf.s

graphics/mbox.s:
	bin2s graphics/mbox.bmp graphics/mbox.s mbox

graphics/icon_sys_J.s:
	bin2s graphics/icon_J.sys graphics/icon_sys_J.s icon_sys_J

graphics/icon_sys_A.s:
	bin2s graphics/icon_A.sys graphics/icon_sys_A.s icon_sys_A

clean:
	$(MAKE) -C ../EE_core clean
	rm -f $(EE_BIN) $(EE_OBJS) *.s graphics/*.s
	rm -f ../modules/bin/*.s
	rm -fr ../EE_core/bin

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
