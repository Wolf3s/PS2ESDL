//FIXME 2014/04/11: As the functions in fileio.o are no longer required, fileio.o has to be added to the objects list for TTY output to work.
//#define DEBUG_TTY_FEEDBACK /* Comment out to disable generation of debug TTY messages */
#define DISABLE_FINAL_PRINTFS
                                /* Disables the last printf() that print a message stating the IOP being reset fully with the patched IOPRP image.
                                        Some games come with IOPRP images with FILEIO modules that crash when accessed with PS2SDK's file I/O RPc client. :(
                                        printf() is affected because it actually writes messages to tty0: with file I/O routines. */

#ifdef DEBUG_TTY_FEEDBACK
 #define DEBUG_PRINTF(args...) printf(args)
#else
 #define DEBUG_PRINTF(args...)
#endif

void InitPointers(void);

struct ModuleEntry{
	void *buffer;
	unsigned int size;
};

#define GS_BGCOLOUR *((volatile unsigned long int*)0x120000E0)

/* EE core operational modes */
#define HDL_MODE_3      0x02

/* EE ELF patching modes (Some of them can be combined to achieved the desired effect). */
#define PATCH_END		0 /* Blank/unused patch section (Default). */
#define SEARCH_SCAN		1
#define FIXED_PATCH		2
#define TERMINATE_ON_FIND	4 /* Don't continue patching after the 1st offset which matches the specified data is found and patched. */

#define BUILT_IN_PATCH		0x80000000
#define DELAYED_READ_PATCH	0x00 /* Delayed reading patch. */
#define BLOCKING_MODULE_PATCH	0x01

/* Some macros used for patching. */
#define JAL(addr)	(0x0c000000|(0x3ffffff&((addr)>>2)))
#define GETJADDR(addr)	((addr&0x03FFFFFF)<<2)

/* Structures */
struct EE_ELF_patch_data{
	u32 mode;		/* How the patch should be applied. */
	u32 src[4];		/* Offset (Only the 1st element is used, the other 3 are ignored)/sample of the original data that needs to be patched, and is to be search for. */
	u32 mask[4];		/* Source pattern Mask (Optional; If unneeded, just fill it with 0xFF bytes). */
	u32 patchData[4];	/* Data that is to replace the original data in the game's ELF that is loaded into memory. */
};

struct PS2ESDL_EE_conf{ /* EE core operation mode flags */
        u32 flags;

	/* Patches that need to be applied to the game's ELF once it's loaded into memory. */
	struct EE_ELF_patch_data patch;
};

/* main.c */
inline void Install_Kernel_Hooks(void);

/* modutils.c */
void InitPointers(void);
void SifExecModuleBufferAsync(void *ptr, unsigned int size, unsigned int arg_len, const char *args);

/* ioprstctrl.c */
void New_SifIopReset(char *arg, unsigned int arglen);

/* utils.c */
void LoadDebugModules(void); /* For debugging purposes only */

/* patches.c */
inline void patch_ELF(void *buffer, struct EE_ELF_patch_data *patch);

/* LoadExecPS2.c */
inline void New_LoadExecPS2(const char *, int argc, char *argv[]);

/* syscall_hooks.c */
u32 Hook_SifSetDma(SifDmaTransfer_t *,s32);
u32 New_SifSetDma(SifDmaTransfer_t *sdd, s32 len);
int Hook_SifSetReg(u32 register_num,int);
void Hook_LoadExecPS2(const char *,s32, char **);

