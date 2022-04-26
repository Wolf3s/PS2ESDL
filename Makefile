.SILENT:

all:
	@echo Building EE core...
	$(MAKE) -C EE_core
	@echo

	@echo Building GUI...
	$(MAKE) -C gui

	$(RM) -fr bin
	$(MKDIR) bin
	mv gui/PS2ESDL.elf bin

run:
	ps2client execee host:bin/PS2ESDL.elf

clean:
	@echo Cleaning up...

	$(RM) -f IOPRP/PS2ESDL_IOPRP.s
	$(MAKE) -C EE_core clean
	$(MAKE) -C gui clean
	$(RM) -fr bin

include $(PS2SDK)/Defs.make
