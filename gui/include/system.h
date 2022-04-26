#define SCECdPSCD	0x10
#define SCECdPSCDDA	0x11
#define SCECdPS2CD	0x12
#define SCECdPS2CDDA	0x13
#define SCECdPS2DVD	0x14

/* ELF-loading stuff */
#define ELF_MAGIC	0x464c457f
#define ELF_PT_LOAD	1

typedef struct
{
	u8	ident[16];                      /* Structure of a ELF header */
	u16	type;
	u16	machine;
	u32	version;
	u32	entry;
	u32	phoff;
	u32	shoff;
	u32	flags;
	u16	ehsize;
	u16	phentsize;
	u16	phnum;
	u16	shentsize;
	u16	shnum;
	u16	shstrndx;
} elf_header_t;

typedef struct
{
	u32	type;                           /* Structure of a header a sections in an ELF */
	u32	offset;
	void	*vaddr;
	u32	paddr;
	u32	filesz;
	u32	memsz;
	u32	flags;
	u32	align;
} elf_pheader_t;

/* Information on the disc/layer. */
struct discInfo{
	u32   ptableLSN;	/* LSN of the type-L path table. */
	u32   ptable_sz;	/* Size of the type-L path table. */
	u32   rdirdsc;		/* LSN of the root directory record. */
	u32   sectorcount;	/* Total number of sectors on the disc. */
};

/* Stores the CDVDMAN configuration */
struct cdvdman_config{
 int            cdvdman_mmode;		/* Media Mode set by sceCdMmode() call */
 u32            operation_mode;		/* Mode of CDVDMAN operation (Normal or legacy) */

 struct discInfo discInfo[2];		/* Information on the disc/individual layers. */
 u32	layerBreak;			/* Layer break for DVD9 discs only. It's set to 0 when the disc image represents a DVD5 disc. */
 u32	sectorsPerSlice;		/* Maximum number of sectors per disc image slice */

 u8     read_buffer_sz;	/* Size of the read buffer (In 2048-byte sectors). */
 u8     dnas_key[5];	/* DNAS key */
};

struct ext_dev_conf{
        u32     part_LBA[9];

        /* Device related */
        u16     sector_sz;
        u16     xfer_blk_sz; /* Minimum number of bytes requested from device at any 1 time */

        /* Emulation standard related */
        u32 sectorsPerSlice; /* Number of sectors in 1 slice */

        void *dev_driver_cfg;
};

/* The structure that holds the data on the game's slices and the media. */
struct DeviceDriver_config{
	u16 sectorSize;
	u32 sliceLBAs[10];
};

/* EE core operational modes */
#define HDL_MODE_3	0x01
#define SYS_FORCESYN	0x02

/* EE ELF patching modes (Some modes can be combined) */
#define SEARCH_SCAN		0 /* Default */
#define FIXED_PATCH		1
#define TERMINATE_ON_FIND	2 /* Don't continue patching after the 1st offset which matches the specified data is found and patched. */

#define BUILT_IN_PATCH		0x80000000

/* Structures */
struct EE_ELF_patch_data{
	u32 mode;		/* How the patch should be applied. */
	u32 src[4];		/* Offset/sample of the original data that needs to be patched, and is to be search for. */
	u32 mask[4];		/* Source pattern Mask (Optional; If unneeded, just fill it with 0xFF bytes). */
	u32 patchData[4];	/* Data that is to replace the original data in the game's ELF that is loaded into memory. If the patch is one of the built-in patch types, then the first element of this array will contain the relative address to the dword that should be patched. */
};

struct PS2ESDL_EE_conf{ /* EE core operation mode flags */
	u32 flags;

	/* Patches that need to be applied to the game's ELF once it's loaded into memory. */
	struct EE_ELF_patch_data patch;
};

void LoadSkinFromCWD(const char *CWD);
void init_gs(void);
void init_sys(void);
void getCWD(char *argv[], char **CWD_path);
unsigned int calculate_str_crc32(const char *string, int maxLength);
int LoadGameListsFromDevices(void);
int load_game_lists(const char *path);
void clearGameLists(void);
int load_PS2ESDL_patch_list(void);
void waitPad1Clear(void);
u32 read_pad1_status(void);
int init_PS2ESDL_sys(const char *ELF_fname, const char *CDVDMAN_disc_img_path, unsigned char sys_type, unsigned char disc_type, u32 flags, unsigned int cache_sz, u8 *dnas_key);
inline void LaunchGame(const unsigned char *disc_img_fname, const char *filename);
void delay(u32 ms);
void LoadDebugModules(void);

int InitConsoleRegionData(void);
const char *GetSystemDataPath(void);
char GetSystemFolderLetter(void);

#define IOCTL_GETLBA		0x80000000 /* Return the LBA of the opened file. */
#define IOCTL_GETSECTORSZ	0x80000001 /* Returns the sector size of the disk. */
#define IOCTL_FRAGCHECK		0x80000002 /* Checks the file for fragmentation. */

#define LEGACY_MAX_SECTORS      524288 /* Maximum number of sectors in each disc image slice for legacy (USBExtreme) disc image sets */
#define MAX_SECTORS             913045 /* 913045 sectors of 2352-bytes MAX in each 2GB image slice. */
