EE_BIN = EE_core.elf
EE_OBJS = main.o syscall_hooks.o LoadExecPS2.o ioprstctrl.o modutils.o utils.o patches.o $(EE_IOP_OBJS) #fileio.o

#IOP modules
EE_IOP_OBJS = 
EE_IOP_ASM = 

EE_INCS += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I.
EE_CFLAGS += -Os -mno-gpopt -G0 -Wall
EE_LDFLAGS += -Wl,-Map,EE_core.map -s -L$(PS2SDK)/ee/lib -L$(PS2SDK)/sbv/lib
EE_LIBS += -lpatches -lc -lkernel

%.o : %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

%.o : %.S
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_BIN) : $(EE_OBJS) $(PS2SDK)/ee/startup/crt0.o
	$(EE_CC) -nostartfiles -Tlinkfile $(EE_LDFLAGS) \
		-o $(EE_BIN) $(EE_OBJS) $(PS2SDK)/ee/startup/crt0.o $(EE_LIBS)
	ee-strip -s -d -R .mdebug.eabi64 -R .reginfo -R .comment $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) EE_core.map $(EE_IOP_ASM)

include $(PS2SDK)/samples/Makefile.pref
