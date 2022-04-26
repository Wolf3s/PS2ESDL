#define MAX_DEVICES		3
#define MAX_MODULES		5

#define DRIVER_TYPE_BUILT_IN	0
#define DRIVER_TYPE_PLUGIN	1

#define BUILT_IN_DRIVERS	1	/* Number of built-in drivers. */

#define PLUGIN_VERSION 0x110

struct PluginHeader{
        unsigned char signature[4];	/* PPLG */
        unsigned short int version;
}  __attribute__((packed));

struct PluginData{
	char DevName[8];
	char DevDisplayName[32];

	unsigned int nModules;		/* Number of modules to load to gain access to the device (Not inclusive of UDNL). */
	unsigned int SizeOfUDNL;

	/* After this there will be the sizes of each module (32-bit integers each, excluding an entry for UDNL), followed by UDNL itself, and then by each module. */
}  __attribute__((packed));

struct ModListEntry{
        unsigned int Size;
        void *Module;
};

struct DeviceDriver{
	char DevName[8];
	char DevDispName[32];
	unsigned char DriverType;

	struct ModListEntry UDNL_module;

	unsigned int nModules;
	struct ModListEntry *Modules;
};

/* Function prototypes. */
void InitBuiltInDrivers(void);
int LoadPlugins(char *CWD);
void FreeAllDrivers(void);
int ExecFreeDrivers(struct DeviceDriver *Device);
int LoadUDNL(struct DeviceDriver *Device, void **buffer);
