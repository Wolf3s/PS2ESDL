#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <loadfile.h>
#include <osd_config.h>
#include <sbv_patches.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>
#include <libpad.h>
#include <limits.h>

#include <dmaKit.h>
#include <gsKit.h>
#include <gsToolkit.h>

#include "include/skin.h"
#include "include/GUI.h"
#include "include/system.h"
#include "include/config.h"
#include "include/krom.h"
#include "include/PatchEngine.h"
#include "include/plugin.h"
#include "include/sort.h"
#include "include/crc16.h"
#include "include/OSDHistory.h"

/* Include externally compiled code */
#ifdef DEBUG_TTY_FEEDBACK
#include "../modules/POWEROFF_irx.c"
#include "../modules/PS2DEV9_irx.c"
#include "../modules/UDPTTY_irx.c"
#endif

extern unsigned char EE_core_elf[];
extern unsigned int size_EE_core_elf;

extern struct GameList GameLists[SUPPORTED_GAME_FORMATS];

/* Include externally compiled media resources. */
extern unsigned char mbox[];
extern unsigned int size_mbox;

/* Global configuration. */
extern struct PS2ESDLConfiguration GlobalConfiguration;

extern unsigned int nDeviceDrivers;
extern struct DeviceDriver Devices[MAX_DEVICES];

extern char PathToGameList[];

extern struct ScreenDisplayData DisplayData;

/* The Current Working Directory */
extern char *currentWorkingDirectory;

/* Patches meant for troublesome games... */
struct PS2ESDL_patchType *PS2ESDL_patch_list=NULL;
unsigned int PS2ESDL_patches=0;

struct PS2ESDL_gameToBePatched *PS2ESDL_gamesToPatch_list=NULL;
unsigned int PS2ESDL_gamesToPatch=0;

/* Data arrays/structures. */
GSGLOBAL *gsGlobal_settings;
GSFONTM *gsFontM;

struct ModuleEntry{
	void *buffer;
	unsigned int size;
};

/* Graphics */
GSTEXTURE mbox_graphic;
extern GSTEXTURE background;
extern GSTEXTURE UI_Skin;

/* Colors */
u64 gsBlack,gsWhiteF,gsWhite,gsGray,gsDBlue,gsBlueTransSel;

/* Function prototypes of local functions. */
static inline void configure_cdvdman(unsigned char* CDVDMAN_module, unsigned int size_CDVDMAN, const unsigned char *disc_img_fname, unsigned char sys_type, unsigned char disc_type, u32 flags, unsigned int cache_sz, u8 *dnas_key, u32 nSlicesLoaded, u32 MaxSectorsPerSlice);
static inline int initDevAndGameImgFiles(const char *disc_img_fname, struct DeviceDriver_config *DevGameConfig, unsigned char sys_type);
static inline void configure_SectorIO(unsigned char* CDVDMAN_module, unsigned int size_CDVDMAN, u32 MaxSectorsPerSlice, struct DeviceDriver_config *DevGameConfig);
static inline void configure_EE_core(const char *ELF_fname, u32 flags);
static inline void patchLoadExecPS2(void *new_eeload);

void init_gs(void){
	/* Initilize DMAKit */
	dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

	/* Initialize the DMAC */
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	/* Initilize the GS */
	gsGlobal_settings=gsKit_init_global();

	/* Apply options from the settings recorded in the loaded configuration file. */
	if(GlobalConfiguration.displayMode!=0) gsGlobal_settings->Mode = GlobalConfiguration.displayMode;
	else GlobalConfiguration.displayMode=gsGlobal_settings->Mode;		/* Otherwise, use the automatically detected video mode. */

	setScreenRes(gsGlobal_settings);

	gsGlobal_settings->DoubleBuffering = GS_SETTING_OFF;	/* Disable double buffering to get rid of the "Out of VRAM" error */
	gsGlobal_settings->ZBuffering = GS_SETTING_OFF;
	gsGlobal_settings->PrimAlphaEnable = GS_SETTING_ON;	/* Enable alpha blending for primitives. */

	gsKit_init_screen(gsGlobal_settings);	/* Apply settings. */
	setScreenPos(gsGlobal_settings, GlobalConfiguration.X_offset, GlobalConfiguration.Y_offset); /* Apply screen image correction. */

	gsBlack=GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00);
	gsWhite=GS_SETREG_RGBAQ(0xFF,0xFF,0xFF,0x00,0x00);
	gsGray=GS_SETREG_RGBAQ(0x30,0x30,0x30,0x00,0x00);
	gsDBlue=GS_SETREG_RGBAQ(0x00,0x00,0x50,0x00,0x00);
	gsBlueTransSel=GS_SETREG_RGBAQ(0x00,0x00,0xFF,0x60,0x00);

	gsWhiteF=GS_SETREG_RGBAQ(0xA0,0xA0,0xA0,0xFF,0x00);

	gsKit_clear(gsGlobal_settings, gsBlack);
}

void init_sys(void){
	u32 pad1_status;

	gsFontM = gsKit_init_fontm();
	gsKit_fontm_upload(gsGlobal_settings, gsFontM);

	/* Initialize and load the Japanese font file, rom0:KROM. */
	gsKit_init_krom(gsGlobal_settings);

	mbox_graphic.Delayed=GS_SETTING_ON;
	load_embeddedGraphics(gsGlobal_settings, &mbox_graphic, mbox, size_mbox);
	UploadTexture(gsGlobal_settings, &mbox_graphic);

	/* Final check: If the user is trying to override the video mode (L1 = PAL, R1 = NTSC)... */
	if(!(GlobalConfiguration.Services&DISABLE_VIDEO_MODE_OVERRIDE)){ /* If the video mode override function is NOT disabled. */
		pad1_status=read_pad1_status();
		if((pad1_status&PAD_L1)||(pad1_status&PAD_R1)){
			GlobalConfiguration.displayMode=(pad1_status&PAD_L1)?GS_MODE_PAL:GS_MODE_NTSC;
			ApplyVideoOptions();
		}
	}

	/* Try to load a skin from "mc0:PS2ESDL/UI.png" */
	if(!(DisplayData.ResourcesLoaded&UI_RESOURCE_SKIN)) if(LoadSkin("mc0:PS2ESDL/", &DisplayData)) DisplayData.ResourcesLoaded|=UI_RESOURCE_SKIN;

	/* Try to load a skin from "mc1:PS2ESDL/UI.png", if one has not been loaded. */
	if(!(DisplayData.ResourcesLoaded&UI_RESOURCE_SKIN)) if(LoadSkin("mc1:PS2ESDL/", &DisplayData)) DisplayData.ResourcesLoaded|=UI_RESOURCE_SKIN;

	/* No skin could be loaded. */
	if(!(DisplayData.ResourcesLoaded&UI_RESOURCE_SKIN)){
		DEBUG_PRINTF("Warning: No skin could be loaded.\n");
		gsKit_mode_switch(gsGlobal_settings, GS_ONESHOT);

		do{
			messageBox(&mbox_graphic, "Warning: No skin could be loaded.\n\nSTART: Continue\nCROSS: Abort load.", "!!Warning!!");
			update_screen();

			pad1_status=read_pad1_status();
		}while((pad1_status!=PAD_START)&&(pad1_status!=PAD_CROSS));
		waitPad1Clear();
		if(pad1_status==PAD_CROSS) ExecOSD(0, NULL);
	}
}

void getCWD(char *argv[], char **CWD_path){
	int i;

	for(i=strlen(argv[0]); i>0; i--){ /* Try to seperate the CWD from the path to this ELF. */
		if((argv[0][i]=='\\')||(argv[0][i]==':')||(argv[0][i]=='/')){
			*CWD_path=malloc(i+2);		/* Allocate one more space for the NULL terminator too. Remember that i contains the offset, not the number of elements!! */
			strncpy(*CWD_path, argv[0], i+1);
			(*CWD_path)[i+1]='\0';
			break;
		}
	}
}

unsigned int calculate_str_crc32(const char *string, int maxLength)
{
	unsigned int crctab[0x400];
	int crc, table, count, byte;

	for (table=0; table<256; table++)
	{
		crc = table << 24;

		for (count=8; count>0; count--)
		{
			if (crc < 0) crc = crc << 1;
			else crc = (crc << 1) ^ 0x04C11DB7;
		}
		crctab[255-table] = crc;
	}

	do {
		byte = string[count++];
		crc = crctab[byte ^ ((crc >> 24) & 0xFF)] ^ ((crc << 8) & 0xFFFFFF00);
	} while((string[count-1]!=0)&&(count<=maxLength));

	return crc;
}


int LoadGameListsFromDevices(void){
	unsigned int i=0, device;
	int devicesInitialized;

	clearGameLists();
	devicesInitialized=0;

	for(device=0; device<nDeviceDrivers; device++){
		DEBUG_PRINTF("Scanning %s for game lists.\n", Devices[device].DevName);

		devicesInitialized++;

		for(i=0; i<10; i++){
			sprintf(PathToGameList, "%s%u:", Devices[device].DevName, i);
			if(load_game_lists(PathToGameList)>0){
				DisplayData.SourceDevice=device;
				goto end; /* Stop attempting to load game lists once at least one list has been sucessfully loaded. */
			}
		}
	}

end:
	return(devicesInitialized);
}

int load_game_lists(const char *path){
	int lists_loaded=0;
	unsigned int i;

	if(load_USBExtreme_list(path)!=0) lists_loaded++;
	if(load_PS2ESDL_list(path)!=0) lists_loaded++;
	if(load_ISO9600_CD_disc_image_list(path)!=0) lists_loaded++;
	if(load_ISO9600_DVD_disc_image_list(path)!=0) lists_loaded++;

	if(GlobalConfiguration.Services&ENABLE_TITLE_SORTING){
		for(i=0; i<SUPPORTED_GAME_FORMATS; i++){
			if(GameLists[i].nGames>0){
				SortGameList(GameLists[i].GameList, GameLists[i].nGames);
			}
		}
	}


	return(lists_loaded);
}

void clearGameLists(void){
	unsigned char i;

	for(i=0; i<SUPPORTED_GAME_FORMATS; i++){
		if(GameLists[i].nGames>0){
			free(GameLists[i].GameList);
			GameLists[i].nGames=0;
			GameLists[i].CurrentlySelectedIndexInList=0;
			GameLists[i].CurrentlySelectedIndexOnScreen=0;
			GameLists[i].old_list_bottom_ID=0;
		}
	}
}

static const struct PS2ESDL_ptch_conf_test_hdr dummy_PS2ESDL_ptch_conf_hdr={
	"PPI\0",
	PS2ESDL_PATCH_VERSION,
};

int load_PS2ESDL_patch_list(void){
	int list_fd;
	static struct PS2ESDL_ptch_conf_hdr sample_PS2ESDL_ptch_conf_hdr;
	char *PPI_path;

	PPI_path=malloc(strlen(currentWorkingDirectory)+11); /* The length of <path>/patches.ppi + 1 NULL terminator. */
	sprintf(PPI_path, "%spatches.ppi", currentWorkingDirectory);

	list_fd=fioOpen(PPI_path, O_RDONLY);

	free(PPI_path);
	if(list_fd<0){
		DEBUG_PRINTF("No PS2ESDL patch file found.\n");

        	return 0; /* Failure loading PS2ESDL_patch.cfg */
	}

	fioRead(list_fd, &sample_PS2ESDL_ptch_conf_hdr, sizeof(struct PS2ESDL_ptch_conf_hdr));

	/* Verify the PS2ESDL configuration file's version (Compare using the shorter structure that only has the signature and version fields). */
	if(memcmp(&dummy_PS2ESDL_ptch_conf_hdr, &sample_PS2ESDL_ptch_conf_hdr, sizeof(struct PS2ESDL_ptch_conf_test_hdr))!=0){
		DEBUG_PRINTF("Invalid PS2ESDL patch configuration file\n");

		fioClose(list_fd);
		return 0; /* If the PS2ESDL patch configuration file is invalid/unsupported. */
	}

	PS2ESDL_patches=sample_PS2ESDL_ptch_conf_hdr.patches;
	PS2ESDL_gamesToPatch=sample_PS2ESDL_ptch_conf_hdr.gamesListed;

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Patch file loaded. Patches: %d; Games indexed: %d.\n", PS2ESDL_patches, PS2ESDL_gamesToPatch);

	if(PS2ESDL_patches>0){
		PS2ESDL_patch_list=malloc(PS2ESDL_patches*sizeof(struct PS2ESDL_patchType));
		fioRead(list_fd, PS2ESDL_patch_list, PS2ESDL_patches*sizeof(struct PS2ESDL_patchType));
	}

	if(PS2ESDL_gamesToPatch>0){
		PS2ESDL_gamesToPatch_list=malloc(PS2ESDL_gamesToPatch*sizeof(struct PS2ESDL_gameToBePatched));
		fioRead(list_fd, PS2ESDL_gamesToPatch_list, PS2ESDL_gamesToPatch*sizeof(struct PS2ESDL_gameToBePatched));
	}

	fioClose(list_fd);

	return 1;
}

void waitPad1Clear(void){
	while(read_pad1_status()!=0){}; /* Keep waiting until the user does not press any buttons. */
}

u32 read_pad1_status(void){
        int pad_state_ret;
        u32 paddata;
        u32 old_pad = 0;
        u32 new_pad;
        struct padButtonStatus buttons;

        pad_state_ret=padGetState(PAD_PORT, PAD_SLOT);
        while((pad_state_ret!=PAD_STATE_STABLE)&&(pad_state_ret!=PAD_STATE_FINDCTP1)) {
                pad_state_ret=padGetState(PAD_PORT, PAD_SLOT);
        }

        pad_state_ret=padRead(PAD_PORT, PAD_SLOT, &buttons);

        if(pad_state_ret!=0) {
            paddata = 0xffff ^ buttons.btns;

            new_pad = paddata & ~old_pad;
            old_pad = paddata;

            return new_pad;
        }
        else return 0;
}

static inline int GetISO9660DiscImageFormatType(const char *path){
	int fd, result;
	static const unsigned char syncbytes[]={0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00};
	unsigned char buffer[16];

	fd=fioOpen(path, O_RDONLY);
	fioRead(fd, buffer, 16);
	fioClose(fd);

	if(memcmp(syncbytes, buffer, 12)==0){
		result=buffer[15]; /* Return the "mode" byte of the first sector of the disc image file. */
	}
	else result=0xFF;

	return(result);
}

int init_PS2ESDL_sys(const char *ELF_fname, const char *CDVDMAN_disc_img_path, unsigned char sys_type, unsigned char disc_type, u32 flags, unsigned int cache_sz, u8 *dnas_key){
	u32 pad1_status;

	int slicesLoaded;
        u32 MaxSectorsPerSlice;
	struct DeviceDriver_config DevGameConfig;

	DEBUG_PRINTF("ELF_fname: %s. Disc image path: %s.\n", ELF_fname, CDVDMAN_disc_img_path);

	/* Maximum number of sectors in each disc image slice */
	if((sys_type==SYSTEM_ISO9660_CD)||(sys_type==SYSTEM_ISO9660_DVD)) MaxSectorsPerSlice=UINT_MAX;	/* Set it to the maximum supported value, since there is no limitation for contiguous ISO9660 disc images. */
	else MaxSectorsPerSlice=(sys_type==SYSTEM_USBEXTREME)?LEGACY_MAX_SECTORS:MAX_SECTORS;

	if((slicesLoaded=initDevAndGameImgFiles(CDVDMAN_disc_img_path, &DevGameConfig, sys_type))<0){
		slicesLoaded=(-slicesLoaded); /* If any one of the game files were fragmented, the value returned would be the negative equivalent of the number of slices loaded. */

		DEBUG_PRINTF("Warning: Game is fragmented.\n");

		waitPad1Clear();
		gsKit_mode_switch(gsGlobal_settings, GS_ONESHOT);

		do{
			messageBox(&mbox_graphic, "Warning: Game is fragmented.\n\nSTART: Continue\nCROSS: Abort load.", "!!Warning!!");
			update_screen();

			pad1_status=read_pad1_status();
		}while((pad1_status!=PAD_START)&&(pad1_status!=PAD_CROSS));
		if(pad1_status==PAD_CROSS){
			waitPad1Clear();
			return(-1);
		}
	}

	if(slicesLoaded==0){	/* If no slices were loaded, that means that the game's files are missing or are somehow unavailable. */
		DEBUG_PRINTF("Error: Unable to access game files.\n");

		waitPad1Clear();
		gsKit_mode_switch(gsGlobal_settings, GS_ONESHOT);

		do{
			messageBox(&mbox_graphic, "Error: Unable to access game files.\n\nCROSS: Abort load.", "!!Error!!");
			update_screen();

			pad1_status=read_pad1_status();
		}while(pad1_status!=PAD_CROSS);

		waitPad1Clear();
		return(-2);
	}

	/* ISO9660 disc images only: Check to see if the correct format is used. */
	if((sys_type==SYSTEM_ISO9660_CD)||(sys_type==SYSTEM_ISO9660_DVD)){
		if(GetISO9660DiscImageFormatType(CDVDMAN_disc_img_path)!=0xFF){
			DEBUG_PRINTF("Error: Unsupported disc image type.\n");

			waitPad1Clear();
			gsKit_mode_switch(gsGlobal_settings, GS_ONESHOT);

			do{
				messageBox(&mbox_graphic, "Error: Unsupported disc image type.\n\nCROSS: Abort load.", "!!Error!!");
				update_screen();

				pad1_status=read_pad1_status();
			}while(pad1_status!=PAD_CROSS);

			waitPad1Clear();
			return(-3);
		}
	}


	/* *** !! NOTE !! The "flags" variable contains shared flags used by the ALL configurable parts of PS2ESDL!! *** */

	load_PS2ESDL_patch_list();

	/* Add code to configure additional modules here. */
	configure_SectorIO(Devices[DisplayData.SourceDevice].UDNL_module.Module, Devices[DisplayData.SourceDevice].UDNL_module.Size, MaxSectorsPerSlice, &DevGameConfig);
	configure_cdvdman(Devices[DisplayData.SourceDevice].UDNL_module.Module, Devices[DisplayData.SourceDevice].UDNL_module.Size, CDVDMAN_disc_img_path, sys_type, disc_type, flags, cache_sz, dnas_key, slicesLoaded, MaxSectorsPerSlice);

	/* Configure the EE core */
	configure_EE_core(ELF_fname, flags);

	return 1;
}

/* CDVDMAN emulation configuration */
static const struct cdvdman_config dummy_cdvdman_conf={
	SCECdPS2DVD,	/* Type of disc loaded */
	0,		/* CDVDMAN operation mode */

	/* Information on the disc/individual layers. */
	{	/* Layer 0 */
		{
			0x105,
			0x400,
			0x107,
			0x10000000,
		},

		/* Layer 1 */
		{
			0x105,
			0x400,
			0x107,
			0x10000000,
		}
	},

	0x12345678,		/* Layer break for DVD9 discs only. It's set to 0 when the disc image represents a DVD5 disc. */
	0x12345678,		/* Maximum number of sectors per disc image slice */

	32,			/* Size of the read buffer (In 2048-byte sectors). */

	{0x11, 0x22, 0x33, 0x44, 0x55},	/* DNAS key */
};

static inline void configure_cdvdman(unsigned char* CDVDMAN_module, unsigned int size_CDVDMAN, const unsigned char *disc_img_fname, unsigned char sys_type, unsigned char disc_type, u32 flags, unsigned int cache_sz, u8 *dnas_key, u32 nSlicesLoaded, u32 MaxSectorsPerSlice){
	unsigned char *databuf;   /* Data buffer for disc information extraction */
	int discimgfd;
	u32 i;
	struct cdvdman_config *new_cdvdman_conf=NULL;
	unsigned int layerBreakSlice;
	unsigned char *slice_fname;
	fio_stat_t FileStat;

        for(i=0;i<size_CDVDMAN;i++){ /* Find the configuration structure in CDVDMAN. */

		new_cdvdman_conf=(void *)&CDVDMAN_module[i];
		if(!memcmp(new_cdvdman_conf, &dummy_cdvdman_conf, sizeof(struct cdvdman_config))) break;
        }


	/* Open disc image file (Slice 0) */
	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Reading disc image data...\n");
	discimgfd=fioOpen(disc_img_fname, O_RDONLY);

	/* Take the number of slices loaded. */
	new_cdvdman_conf->layerBreak=nSlicesLoaded;

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Configuring CDVDMAN...\n");

	databuf=malloc(2048);

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Now buffering disc information...\n");

	fioLseek(discimgfd, 2048*16, SEEK_SET);
	fioRead(discimgfd, databuf, 2048);

	memcpy((unsigned char*)&new_cdvdman_conf->discInfo[0].sectorcount, &databuf[80], 4); /* Read number of sectors in disc image */

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Total number of sectors on disc (or just layer 0): %u\n", new_cdvdman_conf->discInfo[0].sectorcount);

	memcpy((unsigned char*)&new_cdvdman_conf->discInfo[0].ptable_sz, &databuf[132], 4); /* Read the size of the type-L path table. */
	memcpy((unsigned char*)&new_cdvdman_conf->discInfo[0].ptableLSN, &databuf[140], 4); /* Read the LSN of the type-L path table. */

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: LSN of type-L path table: 0x%x\n", new_cdvdman_conf->discInfo[0].ptableLSN);

	/* Read the LSN of the root directory record. */
	memcpy((unsigned char*)&new_cdvdman_conf->discInfo[0].rdirdsc, &databuf[158], 4);

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: LSN of root directory record: 0x%x\n", new_cdvdman_conf->discInfo[0].rdirdsc);

	if((sys_type==SYSTEM_ISO9660_CD)||(sys_type==SYSTEM_ISO9660_DVD)){
		new_cdvdman_conf->sectorsPerSlice=new_cdvdman_conf->discInfo[0].sectorcount;
	}
	else{
		new_cdvdman_conf->sectorsPerSlice=(sys_type==SYSTEM_PS2ESDL)?MAX_SECTORS:LEGACY_MAX_SECTORS;
	}
	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Maximum number of sectors in each disc image slice: %u\n", new_cdvdman_conf->sectorsPerSlice);

	fioClose(discimgfd);

	/* Determine whether the disc image represents a DVD9 disc. */
	layerBreakSlice=(new_cdvdman_conf->discInfo[0].sectorcount)/new_cdvdman_conf->sectorsPerSlice;
	if(new_cdvdman_conf->discInfo[0].sectorcount%new_cdvdman_conf->sectorsPerSlice) layerBreakSlice++;

	layerBreakSlice--; /* Slice numbers start counting from 0. */

	slice_fname=malloc(strlen(disc_img_fname)+1);
	strcpy(slice_fname, disc_img_fname);
	if(sys_type==SYSTEM_PS2ESDL) slice_fname[strlen(slice_fname)-5]=(u8)(layerBreakSlice+'0');		/* PS2ESDL mode. */
	else if(sys_type==SYSTEM_USBEXTREME) slice_fname[strlen(slice_fname)-1]=(u8)(layerBreakSlice+'0');	/* USBExtreme mode. */

	discimgfd=fioOpen(slice_fname, O_RDONLY);
	fioGetstat(slice_fname, &FileStat);
	free(slice_fname);

	/* new_cdvdman_conf->layerBreak contains the number of disc image slices that were sucessfully loaded. */
	if((((u64)FileStat.hisize<<32)|FileStat.size)>((new_cdvdman_conf->discInfo[0].sectorcount%MaxSectorsPerSlice)*2048)){
		/* It's a DVD9 image if the disc image is larger than the stated number of sectors in layer 0. */

		/* Configure DVD9 layer 1 related information. */
		/* NOTE!! The layer 1 ISO9660 filesystem is "jammed" into the end of layer 0. It starts from the last 16 sectors of the layer 0 ISO9660 filesystem. */
		i=new_cdvdman_conf->discInfo[0].sectorcount%MaxSectorsPerSlice; /* Find out which sector is sector 16 of layer 1. It's the sector after the end of the layer 0 ISO9660 filesystem. */

		/* Read sector 16 of layer 1 (The ISO9660 filesystem exists uniquely for each layer, like 2 seperate DVD5 discs. But the 1st 16 sectors of the 2nd layer are "jammed" into the end of the ISO9660 filesystem of layer 0). */
		fioLseek(discimgfd, i*2048, SEEK_SET);
		fioRead(discimgfd, databuf, 2048);

		/* Some images are larger than they really are, but are CD or DVD5 disc images. :( */
		if((sys_type==SYSTEM_ISO9660_CD||sys_type==SYSTEM_ISO9660_DVD)||(databuf[0]!=0x01)||memcmp(&databuf[1], "CD001", 5)){
			/* The disc may have "hidden" data outside the ISO9660 filesystem. */
			new_cdvdman_conf->discInfo[0].sectorcount=(new_cdvdman_conf->layerBreak-1)*new_cdvdman_conf->sectorsPerSlice+(((u64)FileStat.hisize<<32)|FileStat.size)/2048; /* Recalculate the actual number of sectors in the disc image. */

			DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: New maximum number of sectors in the disc: %u\n", new_cdvdman_conf->discInfo[0].sectorcount);

			goto no_dvd9_emu; /* Check if there really is a 2nd layer (Check for the "signature" of the ISO9660 filesystem of layer 1). */
		}

		DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: DVD9 format disc detected.\n");

		memcpy((unsigned char*)&new_cdvdman_conf->discInfo[1].sectorcount, &databuf[80], 4); /* Read number of sectors in disc image */

		DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Total number of sectors on layer 1: %u\n", new_cdvdman_conf->discInfo[1].sectorcount);

		memcpy((unsigned char*)&new_cdvdman_conf->discInfo[1].ptable_sz, &databuf[132], 4); /* Read the size of the type-L path table. */
		memcpy((unsigned char*)&new_cdvdman_conf->discInfo[1].ptableLSN, &databuf[140], 4); /* Read the LSN of the type-L path table. */

		DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: LSN of type-L path table on layer 1: 0x%x\n", new_cdvdman_conf->discInfo[1].ptableLSN);

		/* Read the LSN of the root directory record. */
		memcpy((unsigned char*)&new_cdvdman_conf->discInfo[1].rdirdsc, &databuf[158], 4);
		DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: LSN of root directory record on layer 1: 0x%x\n", new_cdvdman_conf->discInfo[1].rdirdsc);

		/* Convert all of the LSNs gathered into LSNs that can be used for "Flat addressing". */
		new_cdvdman_conf->layerBreak=new_cdvdman_conf->discInfo[0].sectorcount-16; /* This is where the real layer break is at. */
		new_cdvdman_conf->discInfo[1].ptableLSN+=new_cdvdman_conf->layerBreak;
		new_cdvdman_conf->discInfo[1].rdirdsc+=new_cdvdman_conf->layerBreak;
	}
	else{ /* DVD5 disc. */
no_dvd9_emu:
		new_cdvdman_conf->layerBreak=0;
		memset(&new_cdvdman_conf->discInfo[1], 0, sizeof(struct discInfo));

		DEBUG_PRINTF("S2ESDL_EE_LOADER Init: DVD5 disc detected.\n");
	}

	fioClose(discimgfd);
	free(databuf);

	new_cdvdman_conf->cdvdman_mmode=disc_type;     /* Disc type to emulate */

	if(dnas_key!=NULL) memcpy(new_cdvdman_conf->dnas_key, dnas_key, 5);   /* Copy the DNAS key. */

	new_cdvdman_conf->operation_mode=sys_type;     /* Determines which disc image standard to follow: PS2ESDL, the legacy USBExtreme format, or a ISO9660 disc image. */
	new_cdvdman_conf->operation_mode|=flags;

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Read buffer/DMA cache size (In 2048-byte sectors): %d\n", cache_sz);

	new_cdvdman_conf->read_buffer_sz=cache_sz;      /* Read buffer size in 2048-byte sectors */

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Completed configuring CDVDMAN.\n");
}

static const struct ext_dev_conf dummy_devConfig={
	/* part_LBA[9] */
	{
		0xAAAAAAAA,
		0xBBBBBBBB,
		0xCCCCCCCC,
		0xDDDDDDDD,
		0xEEEEEEEE,
		0xFFFFFFFF,
		0x00000000,
		0x11111111,
		0x22222222,
	},

	512,		/* sector_sz */
	4096,		/* xfer_blk_sz */

	MAX_SECTORS,	/* sectorsPerSlice */

	NULL,		/* dev_driver_cfg */
};

/* Builds the configuration structure containing the size of the sectors of the media and the LBAs of each of the game's disc image slices. Returns the number of slices loaded. */
static inline int initDevAndGameImgFiles(const char *disc_img_fname, struct DeviceDriver_config *DevGameConfig, unsigned char sys_type){
	unsigned char *slice0_fname;
	int isFragmented, slicesLoaded, discimgfd, i, MaxSlicesToCheck;

	slice0_fname=malloc(strlen(disc_img_fname)+1);
	strcpy(slice0_fname, disc_img_fname);
	memset(DevGameConfig, 0, sizeof(struct DeviceDriver_config));

	/* NOTE: The size of the sectors of the disk will be retrieved by SectorIO (Part of CDVDMAN). */

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Initilizing the LBAs of all slices...\n");

	slicesLoaded=0;
	isFragmented=0;
	MaxSlicesToCheck=((sys_type==SYSTEM_ISO9660_CD)||(sys_type==SYSTEM_ISO9660_DVD))?1:9;
	for(i=0;i<MaxSlicesToCheck;i++){ /* Initilize the LBAs of all slices */
			if(sys_type==SYSTEM_PS2ESDL) slice0_fname[strlen(slice0_fname)-5]=(u8)(i+'0');		/* PS2ESDL game. */
			else if(sys_type==SYSTEM_USBEXTREME) slice0_fname[strlen(slice0_fname)-1]=(u8)(i+'0');	/* USBExtreme game. */

			DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Opening disc image file slice %d: %s\n",i,slice0_fname);

			/* Open disc image file */
			if((discimgfd=fioOpen(slice0_fname, O_RDONLY))<0){ /* Note!! DVD9 detection is BLIND!! Just try load the LBAs of 9 slices that might or might not that exist. */
				continue;
			}

			DevGameConfig->sliceLBAs[i]=fioIoctl(discimgfd, IOCTL_GETLBA, &DevGameConfig->sliceLBAs[i]);

			if(!(GlobalConfiguration.Services&DISABLE_FRAG_CHECK)){
				if(fioIoctl(discimgfd, IOCTL_FRAGCHECK, &DevGameConfig->sliceLBAs[i])!=0){ /* The &DevGameConfig->sliceLBAs[i] argument isn't used, but is passed because Sony's IOCTL function requires a value there. */
					isFragmented=1;
				}
			}

			fioClose(discimgfd);
			slicesLoaded++;
	}

	free(slice0_fname);

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: LBA initilization complete.\n");
	return(isFragmented?(-slicesLoaded):slicesLoaded);
}

/* Initialize SectorIO. */
static inline void configure_SectorIO(unsigned char* CDVDMAN_module, unsigned int size_CDVDMAN, u32 MaxSectorsPerSlice, struct DeviceDriver_config *DevGameConfig){
	u32 i;
	struct ext_dev_conf *new_devConfig=NULL;

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Configuring SECTORIO. Max sectors per slice: %u.\n", MaxSectorsPerSlice);

	for(i=0;i<size_CDVDMAN;i++){ /* Find the SECTORIO configuration structure in CDVDMAN. */
		new_devConfig=(void *)&CDVDMAN_module[i];
		if(!memcmp(new_devConfig, &dummy_devConfig, sizeof(struct ext_dev_conf))){
			DEBUG_PRINTF("Found the device configuration structure.\n");
			break;
		}
	}

	/* Retrieve the size of the sectors on the disk. */
	/* new_devConfig->sector_sz=DevGameConfig->sectorSize; *//* Not in use. */

	new_devConfig->xfer_blk_sz=4096;
	new_devConfig->sectorsPerSlice=MaxSectorsPerSlice;
	for(i=0; i<9; i++) new_devConfig->part_LBA[i]=DevGameConfig->sliceLBAs[i];
}

static const struct PS2ESDL_EE_conf static_EE_core_conf={
	0xDEADBEEF,

	/* Patches that need to be applied to the game's ELF once it's loaded into memory. */
	{
		0, /* mode */

		{	/* src */
			0x11111111,
			0x22222222,
			0x33333333,
			0x44444444,
		},

		{	/* mask */
			0x55555555,
			0x66666666,
			0x77777777,
			0x88888888,
		},

		{	/* patchData */
			0x99999999,
			0xAAAAAAAA,
			0xBBBBBBBB,
			0xCCCCCCCC,
		},
	}
};

static inline void configure_EE_core(const char *ELF_fname, u32 flags){
	unsigned int i;
	struct PS2ESDL_EE_conf *EE_core_conf=NULL;

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Configuring EE core...\n");

	for(i=0;i<size_EE_core_elf;i++){ /* Find the EE core configuration structure */
		EE_core_conf=(void *)&EE_core_elf[i];
		if(!memcmp(EE_core_conf, &static_EE_core_conf, sizeof(struct PS2ESDL_EE_conf))){
			DEBUG_PRINTF("Found the EE core configuration structure.\n");
			break;
		}
	}

	/* Initilze the patch section of the EE core. */
	memset(EE_core_conf, 0, sizeof(struct PS2ESDL_EE_conf));

	for(i=0; i<PS2ESDL_gamesToPatch; i++){
		DEBUG_PRINTF("Scanning: %s\n", PS2ESDL_gamesToPatch_list[i].disc_ID);
		if(strstr(ELF_fname, PS2ESDL_gamesToPatch_list[i].disc_ID)!=NULL){
			DEBUG_PRINTF("Found %s. Loading patches...\n", PS2ESDL_gamesToPatch_list[i].disc_ID);
			EE_core_conf->patch.mode=PS2ESDL_gamesToPatch_list[i].mode;
			memcpy(EE_core_conf->patch.src, PS2ESDL_patch_list[PS2ESDL_gamesToPatch_list[i].patch_ID].src, sizeof(EE_core_conf->patch.src));
			memcpy(EE_core_conf->patch.mask, PS2ESDL_patch_list[PS2ESDL_gamesToPatch_list[i].patch_ID].mask, sizeof(EE_core_conf->patch.mask));
			memcpy(EE_core_conf->patch.patchData, PS2ESDL_patch_list[PS2ESDL_gamesToPatch_list[i].patch_ID].patchData, sizeof(EE_core_conf->patch.patchData));

			break;
		}
	}

	EE_core_conf->flags=flags;

	DEBUG_PRINTF("PS2ESDL_EE_LOADER Init: Completed configuring the EE core.\n");
}

/*	void patchLoadExecPS2(void *new_eeload);
 *
 *	Patches LoadExecPS2(), which resides in kernel memory, to use a specified ELF loading function instead of using rom0:EELOAD
 */
static inline void patchLoadExecPS2(void *new_eeload){
	/* The pattern of the code in LoadExecPS2() that prepares the kernel for copying EELOAD from rom0: */
	static const u32 initEELOADCopyPattern[] = {
		0x8FA30010,	/* lw		v1, 0x0010(sp) */
		0x0240302D,	/* daddu	a2, s2, zero */
		0x8FA50014,	/* lw		a1, 0x0014(sp) */
		0x8C67000C,	/* lw		a3, 0x000C(v1) */
		0x18E00009,	/* blez		a3, +9 <- The kernel will skip the EELOAD copying loop if the value in $a3 is less than, or equal to 0. Lets do that... */
	};

	u32 *p;

	DIntr();		/* Disable interrupts so that we won't be interrupted while patching the kernel. */
	ee_kmode_enter();	/* Enter kernel mode; We're modifying LoadExecPS2() that resides inside kernel memory. */

	/* Find the part of LoadExecPS2() that initilizes the EELOAD copying loop's variables */
	for(p=(u32 *)0x80000000; p<(u32 *)0x80030000; p++){	/* Search for the loop initilization code only at the start of kernel memory, since LoadExecPS2() usually resides in that area (The area after 0x80030000 is usually empty). */
		if(memcmp(p, &initEELOADCopyPattern, sizeof(initEELOADCopyPattern))==0){
			p[0] = 0x3C120000 | (u16)((u32)new_eeload >> 16);	/* lui s2, HI16(new_eeload) */
			p[1] = 0x36520000 | (u16)((u32)new_eeload & 0xFFFF);	/* ori s2, s2, LO16(new_eeload) */
			p[2] = 0x00000000;					/* nop */
			p[3] = 0x24070000;					/* li a3, 0 <- Disable the EELOAD copying loop */

			break;	/* All done. */
		}
	}

	ee_kmode_exit();	/* Exit from kernel mode */
	EIntr();		/* Re-enable interrupts */
}

/*
	static inline int HasValidPS2Logo(u32 StartLBA);

	Returns whether the specified disc image contains a valid Playstation 2 logo for use with the console.

	Parameters: const char *path	- The path to the disc image.
	Return values:
		<0	- Error.
		0	- Logo is not valid for use with the console.
		1	- Logo is valid for use with the console.
*/
static inline int HasValidPS2Logo(const char *path){
	int fd, result;
	char *LogoBuffer, RegionCode;
	unsigned short int crc16checksum;

	fd=fioOpen("rom0:ROMVER", O_RDONLY);
	fioLseek(fd, 4, SEEK_SET);
	fioRead(fd, &RegionCode, 1);
	fioClose(fd);

	InitCRC16Table();

	if((fd=fioOpen(path, O_RDONLY))>=0){
		LogoBuffer=memalign(16, 12*2048);

		if((result=fioRead(fd, LogoBuffer, 12*2048))==12*2048){
			crc16checksum=ReflectAndXORCRC16(CalculateCRC16(LogoBuffer, 2048*12, CRC16_INITIAL_CHECKSUM));

			switch(RegionCode){
				/* NTSC regions. */
				case 'J':
				case 'A':
				case 'C':
				case 'H':
				case 'M':
					result=(crc16checksum==0xD2BC)?1:0;
					break;
				/* PAL regions. */
				case 'E':
				case 'R':
				case 'O':
					result=(crc16checksum==0x4702)?1:0;
					break;
				default:
					result=0;
			}

			printf("Console region:\t%c\nLogo checksum:\t0x%04x\nResult:\t%d\n", RegionCode, crc16checksum, result);
		}
		else{
			printf("Error reading logo: %d\n", result);
		}

		free(LogoBuffer);
		fioClose(fd);
	}
	else{
		result=fd;
		printf("Error opening disc image file. Result: %d\n", result);
	}

	return result;
}

static inline void CopyIOPRPToRAM(struct DeviceDriver *Device){
	struct ModuleEntry *Module;
	void *udnl_module;
	int size_udnl;

	Module=(struct ModuleEntry *)0x00088000;

	size_udnl=LoadUDNL(Device, &udnl_module);
	Module[0].buffer=(void *)0x00088010;
	memcpy(Module[0].buffer, udnl_module, size_udnl);
	Module[0].size=size_udnl;
}

inline void LaunchGame(const unsigned char *disc_img_fname, const char *filename)
{
	elf_header_t *eh;
	elf_pheader_t *eph;
	unsigned int i;
	char *argv[2];
	int HasValidLogo;

	AddHistoryRecordUsingFullPath(filename);

	HasValidLogo=HasValidPS2Logo(disc_img_fname);

	fioExit();
	SifExitRpc();
	SifLoadFileExit();
	SifExitIopHeap();

	/* Free up allocated resources. */
	free(currentWorkingDirectory);

	for(i=0; i<SUPPORTED_GAME_FORMATS; i++){
		if(GameLists[i].nGames>0) free(GameLists[i].GameList);
	}

	if(DisplayData.ResourcesLoaded&UI_RESOURCE_SKIN) free(UI_Skin.Mem);
	if(DisplayData.ResourcesLoaded&UI_RESOURCE_BKGND) free(background.Mem);
	free(mbox_graphic.Mem);

	memset((void *)0x00082000, 0, 0x00100000-0x00082000); /* Wipe the area below the "real" user memory area. */

	CopyIOPRPToRAM(&Devices[DisplayData.SourceDevice]);
	FreeAllDrivers();

 	eh = (elf_header_t *)EE_core_elf;
	eph = (elf_pheader_t *)(EE_core_elf + eh->phoff);

	/* Scan through the ELF's headers and copy them into RAM, then
	zero out any non-loadable regions.  */
	for (i = 0; i < eh->phnum; i++)
	{
		if(eph[i].type!=ELF_PT_LOAD) continue;

		memcpy(eph[i].vaddr, (void *)(EE_core_elf + eph[i].offset), eph[i].filesz);

		if (eph[i].memsz > eph[i].filesz) memset(eph[i].vaddr + eph[i].filesz, 0, eph[i].memsz - eph[i].filesz);
        }

	sbv_patch_user_mem_clear((void*)0x000D0000);	/* Patch initilizeUserMemory() to wipe user memory starting from 0x000D0000 instead. */
	patchLoadExecPS2((void *)eh->entry);	/* Patch LoadExecPS2() to not load rom0:EELOAD, but use a specified ELF loader. */

	FlushCache(0);
	FlushCache(2);

	/* If the disc image has a valid Playstation 2 logo, execute the game via PS2LOGO. */
	if(GlobalConfiguration.Services&ENABLE_PS2LOGO && HasValidLogo==1){
		argv[0]=(char *)filename;
		argv[1]=NULL;
		LoadExecPS2("rom0:PS2LOGO", 1, argv);
	}
	else LoadExecPS2(filename, 0, NULL);		/* Otherwise, invoke LoadExecPS2() to launch the game directly. */
}

void delay(u32 ms){
	u64 i;

	for(i=0;i<ms*30000;i++){};
}

#ifdef DEBUG_TTY_FEEDBACK
void LoadDebugModules(void){
	SifExecModuleBuffer(POWEROFF_irx,size_POWEROFF_irx,0,NULL,NULL); /* Load POWEROFF */
	SifExecModuleBuffer(PS2DEV9_irx,size_PS2DEV9_irx,0,NULL,NULL); /* Load PS2DEV9 */
	SifExecModuleBuffer(UDPTTY_irx,size_UDPTTY_irx,0,NULL,NULL); /* Load UDPTTY */
}
#endif

enum CONSOLE_REGIONS{
	CONSOLE_REGION_INVALID	= -1,
	CONSOLE_REGION_JAPAN	= 0,
	CONSOLE_REGION_USA,	//USA and HK/SG.
	CONSOLE_REGION_EUROPE,
	CONSOLE_REGION_CHINA,

	CONSOLE_REGION_COUNT
};

static short int ConsoleRegion=CONSOLE_REGION_INVALID;
static char SystemDataFolderPath[]="BRDATA-SYSTEM";
static char SystemFolderLetter='R';

static void UpdateSystemPaths(void){
	char regions[CONSOLE_REGION_COUNT]={'I', 'A', 'E', 'C'};

	SystemFolderLetter=regions[ConsoleRegion];
	SystemDataFolderPath[1]=SystemFolderLetter;
}

int InitConsoleRegionData(void){
	int result;
	char romver[16];

	if((result=ConsoleRegion)<0){
		GetRomName(romver);

		switch(romver[4]){
			case 'C':
				ConsoleRegion=CONSOLE_REGION_CHINA;
				break;
			case 'E':
				ConsoleRegion=CONSOLE_REGION_EUROPE;
				break;
			case 'H':
			case 'A':
				ConsoleRegion=CONSOLE_REGION_USA;
				break;
			case 'J':
				ConsoleRegion=CONSOLE_REGION_JAPAN;
		}

		result=ConsoleRegion;

		UpdateSystemPaths();
	}

	return result;
}

const char *GetSystemDataPath(void){
	return SystemDataFolderPath;
}

char GetSystemFolderLetter(void){
	return SystemFolderLetter;
}
