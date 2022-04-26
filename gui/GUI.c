#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <libcdvd.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>
#include <timer.h>
#include <malloc.h>

#include <dmaKit.h>
#include <gsKit.h>
#include <gsToolkit.h>
#include <libmc.h>
#include <libpad.h>

#include "include/PS2ESDL_format.h"

#include "include/GUI.h"
#include "include/system.h"
#include "include/skin.h"
#include "include/config.h"
#include "include/plugin.h"

unsigned char padBuf[256] __attribute__((aligned(64)));

struct GameList GameLists[SUPPORTED_GAME_FORMATS]={
	{
	    	NULL,				/* GameList; */
		"PS2ESDL format",		/* label */
		0,				/* nGames */
		0,				/* CurrentlySelectedIndexInList */
		0,				/* CurrentlySelectedIndexOnScreen */
		0,				/* old_list_bottom_ID */
		SYSTEM_PS2ESDL,			/* GameListType */
	},

	{
	    	NULL,				/* GameList; */
		"USBExtreme format",		/* label */
		0,				/* nGames */
		0,				/* CurrentlySelectedIndexInList */
		0,				/* CurrentlySelectedIndexOnScreen */
		0,				/* old_list_bottom_ID */
		SYSTEM_USBEXTREME,		/* GameListType */
	},

	{
	    	NULL,				/* GameList; */
		"ISO9660 disc image (CD)",	/* label */
		0,				/* nGames */
		0,				/* CurrentlySelectedIndexInList */
		0,				/* CurrentlySelectedIndexOnScreen */
		0,				/* old_list_bottom_ID */
		SYSTEM_ISO9660_CD,		/* GameListType */
	},

	{
	    	NULL,				/* GameList; */
		"ISO9660 disc image (DVD)",	/* label */
		0,				/* nGames */
		0,				/* CurrentlySelectedIndexInList */
		0,				/* CurrentlySelectedIndexOnScreen */
		0,				/* old_list_bottom_ID */
		SYSTEM_ISO9660_DVD,		/* GameListType */
	}
};

unsigned char PS2ESDL_cache_sz=2;
u32 PS2ESDL_op_flags=0;

extern GSGLOBAL *gsGlobal_settings;
extern u64 gsBlack,gsWhiteF,gsBlue,gsWhite,gsGray,gsDBlue,gsBlueTrans;

/* Graphics */
extern GSTEXTURE mbox_graphic;
GSTEXTURE background;

/* The Current Working Directory */
char *currentWorkingDirectory;

/* The path to the game lists. */
char PathToGameList[8];

volatile int PS2driveTableUpdated=0;

extern unsigned int nDeviceDrivers;
extern struct DeviceDriver Devices[MAX_DEVICES];
struct ScreenDisplayData DisplayData; 

static int nDevices=0; /* The number of devices that are detected and usable. */

static void driveIntrConnectHandler(void *deviceType, void *driveTable){
	nDevices++;

	PS2driveTableUpdated=1;
}

static void driveIntrDisconnectHandler(void *deviceType, void *driveTable){
	if(nDevices>0) nDevices--;

	PS2driveTableUpdated=1;
}

static unsigned char SwapDisplayedGameList(unsigned long int *max_displayed_games, unsigned char CurrentListIndexNumber, char Direction){
	unsigned char i;

	if(CurrentListIndexNumber==SYSTEM_INVALID) CurrentListIndexNumber=0;

	for(i=0; i<SUPPORTED_GAME_FORMATS; i++){
		if(GameLists[CurrentListIndexNumber].nGames>0){
			*max_displayed_games=(GameLists[CurrentListIndexNumber].nGames>DisplayData.SkinParameters.nGamesToList)?DisplayData.SkinParameters.nGamesToList:GameLists[CurrentListIndexNumber].nGames;

			return(CurrentListIndexNumber);
		}

		if(Direction){
			CurrentListIndexNumber++;
			if(CurrentListIndexNumber>=SUPPORTED_GAME_FORMATS) CurrentListIndexNumber=0;
		}
		else{
			if(CurrentListIndexNumber==0) CurrentListIndexNumber=SUPPORTED_GAME_FORMATS-1;
			else CurrentListIndexNumber--;
		}
	}

	*max_displayed_games=0;
	return SYSTEM_INVALID;
}

static int loadBackgroundFromPath(char *path, GSTEXTURE* background){
	char* fname;
	int result;

	result=0;
	if((fname=malloc(strlen(path)+15))!=NULL){	/* Length of path + length of "background.png" + 1 NULL terminator. */
		sprintf(fname, "%sbackground.png", path);

		/* Try to load the background from the path. */
		background->Delayed=GS_SETTING_ON;
		if((result=gsKit_texture_png(gsGlobal_settings, background, fname))==0){
			UploadTexture(gsGlobal_settings, background);
		}

		free(fname);
	}

	return result;
}

int main(int argc, char** argv)
{
	u32 pad1_status, OldCpuTicks, CpuTicks, PadPollDelay;
	unsigned char i, DirectionalKeyPressed;
	struct GameList *ActiveGameList;
	char* path;
	int result;

	/* For the initialization of PS2ESDL's drivers later. */
	unsigned char ELF_fname[27], *disc_img_pth;

	/* For reading the status/details of the inserted Memory Card(s). */
	int SpaceFree, format, type;

	SifInitRpc(0);
	fioInit();

	if((argc>0)&&(argv!=NULL)) getCWD(argv, &currentWorkingDirectory); /* Get the path to the current working directory (CWD). */
	else{
		currentWorkingDirectory=malloc(7);
		strcpy(currentWorkingDirectory, "mass:/");	/* Assume that PS2ESDL was launched from mass: */
	}

	DisplayData.ResourcesLoaded=0;
	DisplayData.displayLength=UI_VISIBLE_SCREEN_LENGTH;
	DisplayData.displayHeight=UI_VISIBLE_SCREEN_HEIGHT;

	LoadPlugins(currentWorkingDirectory);
	LoadConfiguration();
	init_gs();

	/* Try to load a background from the CWD. */
	if(loadBackgroundFromPath(currentWorkingDirectory, &background)>=0){
		DisplayData.ResourcesLoaded|=UI_RESOURCE_BKGND;
	}

	/* Try to load a skin from the CWD. */
	if(LoadSkin(currentWorkingDirectory, &DisplayData)!=0) DisplayData.ResourcesLoaded|=UI_RESOURCE_SKIN;

	fioExit();
	SifExitRpc();

	/* Startup initialization */
	SifIopReset(NULL, 0);

	/* Do some things while waiting for the IOP to be reset. */
	InitBuiltInDrivers();

	while(!SifIopSync()){};

	SifInitRpc(0);
	SifInitIopHeap();
	SifLoadFileInit();
	fioInit();

	cdInit(CDVD_INIT_NOCHECK);

	sbv_patch_enable_lmb();

	SifAddCmdHandler(12, &driveIntrConnectHandler, NULL);
	SifAddCmdHandler(13, &driveIntrDisconnectHandler, NULL);

	SifLoadModule("rom0:SIO2MAN", 0, NULL);
	SifLoadModule("rom0:MCMAN", 0, NULL);
	SifLoadModule("rom0:MCSERV", 0, NULL);
	SifLoadModule("rom0:PADMAN", 0, NULL);

	InitConsoleRegionData();

	padInit(0);
	padPortOpen(PAD_PORT,PAD_SLOT,padBuf);

	/* NOTE: Doing these actions below isn't necessary for accessing the Memory Cards...
	but Sony's MCMAN modules don't reset the SIO2 interface before use (Resulting in some kind of authentication failure in some games).

	This offers a workaround by forcing the interface to be reset (mcGetInfo() seems to reset the SIO2 interface). */

	mcInit(MC_TYPE_MC);
	/* Reset both MC slots by reading their status. */
	for(i=0; i<2; i++){
		mcGetInfo(0, i, &type, &SpaceFree, &format);
		mcSync(0, NULL, &result);
	}

#ifdef DEBUG_TTY_FEEDBACK
	LoadDebugModules();
#endif

	init_sys();

	if((argc<=0)||(argv==NULL)){
		DEBUG_PRINTF("Warning: Invalid arguments passed.\nPlease upgrade your ELF launcher.\n");
		gsKit_mode_switch(gsGlobal_settings, GS_ONESHOT);

		do{
			messageBox(&mbox_graphic, "Warning: Invalid arguments passed.\nPlease upgrade your ELF launcher.\n\nSTART: Continue\nCROSS: Abort load.", "!!Warning!!");
			update_screen();

			pad1_status=read_pad1_status();
		}while((pad1_status!=PAD_START)&&(pad1_status!=PAD_CROSS));
		if(pad1_status==PAD_CROSS) ExecOSD(0, NULL);
	}

	messageBox(&mbox_graphic, "Loading drivers...", "Please wait...");
	gsKit_mode_switch(gsGlobal_settings, GS_PERSISTENT);
	update_screen();

	/* Load all device drivers. */
	for(i=0; i<nDeviceDrivers; i++) ExecFreeDrivers(&Devices[i]);

	gsKit_mode_switch(gsGlobal_settings, GS_ONESHOT);
	update_screen();

	/* Try to load the background from "mc0:PS2ESDL/background.png" */
	if(!DisplayData.ResourcesLoaded&UI_RESOURCE_BKGND) if(loadBackgroundFromPath("mc0:PS2ESDL/", &background)>=0) DisplayData.ResourcesLoaded|=UI_RESOURCE_BKGND;

	/* Try to load the background from "mc1:PS2ESDL/background.png", if one has not been loaded. */
	if(!DisplayData.ResourcesLoaded&UI_RESOURCE_BKGND) if(loadBackgroundFromPath("mc1:PS2ESDL/", &background)>=0) DisplayData.ResourcesLoaded|=UI_RESOURCE_BKGND;

	DisplayData.CurrentlySelectedGameListType=SwapDisplayedGameList(&DisplayData.max_displayed_games, 0, 1);
 	ActiveGameList=(DisplayData.CurrentlySelectedGameListType!=SYSTEM_INVALID)?&GameLists[DisplayData.CurrentlySelectedGameListType]:NULL;
	DirectionalKeyPressed=0;
	OldCpuTicks=0;
	PadPollDelay=PAD_POLL_DELAY;

mainGameSelectionLoop:
	gsKit_mode_switch(gsGlobal_settings, GS_ONESHOT);

	while(1){
		if(PS2driveTableUpdated){ /* Check if a device has been added or removed. */
			PS2driveTableUpdated=0;
			LoadGameListsFromDevices(); /* (Re-)load game lists from all devices. */

			DisplayData.CurrentlySelectedGameListType=SwapDisplayedGameList(&DisplayData.max_displayed_games, DisplayData.CurrentlySelectedGameListType, 1);
			ActiveGameList=(DisplayData.CurrentlySelectedGameListType!=SYSTEM_INVALID)?&GameLists[DisplayData.CurrentlySelectedGameListType]:NULL;

			/* Load the background from <device>PS2ESDL/, if one has not been loaded */
			if(!(DisplayData.ResourcesLoaded&UI_RESOURCE_BKGND)){
				path=malloc(strlen(PathToGameList)+9);
				sprintf(path, "%sPS2ESDL/", PathToGameList);

				if(loadBackgroundFromPath(path, &background)>=0) DisplayData.ResourcesLoaded|=UI_RESOURCE_BKGND;
				free(path);
			}
		}

		pad1_status=read_pad1_status();

		CpuTicks=cpu_ticks();
		if(CpuTicks<OldCpuTicks) OldCpuTicks=CpuTicks;

		/* Determine whether either the UP or DOWN directional keys were held. */
		DirectionalKeyPressed=((pad1_status&PAD_UP)||(pad1_status&PAD_DOWN))?1:0;

		/* Add a small delay between pad reads. */
		PadPollDelay=DirectionalKeyPressed?PAD_POLL_IN_BETWEEN_KEY_PRESSES_DELAY:PAD_POLL_DELAY;

		if(DisplayData.CurrentlySelectedGameListType!=SYSTEM_INVALID){
			if(CpuTicks>(OldCpuTicks+PadPollDelay)){
				OldCpuTicks=CpuTicks;

				if(pad1_status&PAD_UP){
					if(ActiveGameList->CurrentlySelectedIndexInList>0){
						ActiveGameList->CurrentlySelectedIndexInList--;
						if(ActiveGameList->CurrentlySelectedIndexOnScreen>0) ActiveGameList->CurrentlySelectedIndexOnScreen--;
					}
					else{	/* If the first game entry was highlighted and the user pressed UP, warp around to the last entry. */
						ActiveGameList->CurrentlySelectedIndexInList=ActiveGameList->nGames-1;
						ActiveGameList->CurrentlySelectedIndexOnScreen=((ActiveGameList->nGames>DisplayData.SkinParameters.nGamesToList)?DisplayData.SkinParameters.nGamesToList:ActiveGameList->nGames)-1;
					}
				}
				else if(pad1_status&PAD_DOWN){
					if(ActiveGameList->CurrentlySelectedIndexInList<(ActiveGameList->nGames-1)){
						ActiveGameList->CurrentlySelectedIndexInList++;
						if(ActiveGameList->CurrentlySelectedIndexOnScreen<(DisplayData.SkinParameters.nGamesToList-1)) ActiveGameList->CurrentlySelectedIndexOnScreen++;
					}
					else{	/* If the last game entry was highlighted and the user pressed DOWN, warp around to the first entry. */
						ActiveGameList->CurrentlySelectedIndexInList=0;
						ActiveGameList->CurrentlySelectedIndexOnScreen=0;
					}
				}
			}

			if(pad1_status&PAD_LEFT){
				waitPad1Clear();
				if(DisplayData.CurrentlySelectedGameListType>0) DisplayData.CurrentlySelectedGameListType--;

				DisplayData.CurrentlySelectedGameListType=SwapDisplayedGameList(&DisplayData.max_displayed_games, DisplayData.CurrentlySelectedGameListType, 0);
 				ActiveGameList=(DisplayData.CurrentlySelectedGameListType!=SYSTEM_INVALID)?&GameLists[DisplayData.CurrentlySelectedGameListType]:NULL;
			}
			else if(pad1_status&PAD_RIGHT){
				waitPad1Clear();
				if(DisplayData.CurrentlySelectedGameListType<(SUPPORTED_GAME_FORMATS-1)) DisplayData.CurrentlySelectedGameListType++;

				DisplayData.CurrentlySelectedGameListType=SwapDisplayedGameList(&DisplayData.max_displayed_games, DisplayData.CurrentlySelectedGameListType, 1);
 				ActiveGameList=(DisplayData.CurrentlySelectedGameListType!=SYSTEM_INVALID)?&GameLists[DisplayData.CurrentlySelectedGameListType]:NULL;
			}
			else if((pad1_status&PAD_CIRCLE)||(pad1_status&PAD_CROSS)||(pad1_status&PAD_START)){
				if(DisplayData.CurrentlySelectedGameListType==SYSTEM_PS2ESDL){
					PS2ESDL_op_flags=((struct PS2ESDL_config_entry *)(ActiveGameList->GameList))[ActiveGameList->CurrentlySelectedIndexInList].flags;	/* Set whatever compatibility modes that were set by the game installer. */
					PS2ESDL_cache_sz=((struct PS2ESDL_config_entry *)(ActiveGameList->GameList))[ActiveGameList->CurrentlySelectedIndexInList].cache_sz;	/* Take the cache size setting set by the game installer. */
				}

				if(pad1_status&PAD_TRIANGLE) PS2ESDL_op_flags|=HDL_MODE_3;
				if(pad1_status&PAD_SQUARE) PS2ESDL_op_flags|=SYS_FORCESYN;

				/* Check if the user had specified a non-default cache size. Default = 2. */
				if(pad1_status&PAD_L1) PS2ESDL_cache_sz=8;
				else if(pad1_status&PAD_L2) PS2ESDL_cache_sz=16;
				else if(pad1_status&PAD_R1) PS2ESDL_cache_sz=24;
				else if(pad1_status&PAD_R2) PS2ESDL_cache_sz=32;

				goto game_select_complete;
			}

			/* Recalculate some of the values required for displaying the game list. */
			if(ActiveGameList->old_list_bottom_ID>(ActiveGameList->CurrentlySelectedIndexInList+DisplayData.SkinParameters.nGamesToList-1)){
				ActiveGameList->old_list_bottom_ID=ActiveGameList->CurrentlySelectedIndexInList+DisplayData.SkinParameters.nGamesToList-1;	/* If the current selection is below of the old bottom game entry;
																					This only occurs for game lists with more game entries than what can be listed on the screen at 1 time */
			} /* If the current selection is greater than the old bottom game entry */
			else if(ActiveGameList->old_list_bottom_ID<ActiveGameList->CurrentlySelectedIndexInList){
				ActiveGameList->old_list_bottom_ID=ActiveGameList->CurrentlySelectedIndexInList;
			}
		}

		if(pad1_status&PAD_SELECT){
			if(pad1_status&PAD_R1) ExecOSD(0, NULL);	/* Secret exit combo code: SELECT + R1 */
			else configurePS2ESDL(&background);		/* Otherwise, the user wants to enter the Options screen. */
		}

		redrawGameListScreenBackground(ActiveGameList, DisplayData.CurrentlySelectedGameListType, &background);
		if(DisplayData.CurrentlySelectedGameListType==SYSTEM_INVALID){
			messageBox(&mbox_graphic, "== No game list loaded ==", "No game list loaded");
		}
		update_screen();
	}	/* End of main "while" loop */

game_select_complete:
	messageBox(&mbox_graphic, "Loading...", "Please wait...");

	gsKit_mode_switch(gsGlobal_settings, GS_PERSISTENT);
	update_screen();

	DEBUG_PRINTF("Installing EE core into EE memory and launching game...\n");

	memset(ELF_fname, 0, sizeof(ELF_fname));
	strcpy(ELF_fname, "cdrom0:\\");

	if(DisplayData.CurrentlySelectedGameListType==SYSTEM_PS2ESDL){
		unsigned char PDI_fname[17];

		memcpy(PDI_fname, ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].ELF_fname, 11);
		PDI_fname[8]=PDI_fname[9];
		PDI_fname[9]=PDI_fname[10];
		strcpy(&PDI_fname[10], "_00.pdi");

		disc_img_pth=malloc(strlen(PathToGameList)+26);	/* length of <path>/PS2ESDL/SLXX_12399_00.pdi + 1 NULL terminator. */
		sprintf(disc_img_pth,"%sPS2ESDL/%s", PathToGameList, PDI_fname);

		strncat(ELF_fname, ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].ELF_fname, 11);
		strcat(ELF_fname, ";1");
	}
	else if(DisplayData.CurrentlySelectedGameListType==SYSTEM_USBEXTREME){ /* DisplayData.CurrentlySelectedGameListType==SYSTEM_USBEXTREME */
		strncat(ELF_fname, ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].ELF_fname, 11);
		strcat(ELF_fname, ";1");

		disc_img_pth=malloc(strlen(PathToGameList)+27);	/* length of <path>/ul.XXXXXXXX.SLXX_123.99.XX + 1 NULL terminator. */
		sprintf(disc_img_pth, "%sul.%08X.%s%s", PathToGameList, calculate_str_crc32(ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].title, 32), ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].ELF_fname, ".00");
	}
	else{ /* DisplayData.CurrentlySelectedGameListType == SYSTEM_ISO9660_CD || DisplayData.CurrentlySelectedGameListType == SYSTEM_ISO9660_DVD */
		strncat(ELF_fname, ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].ELF_fname, 11);
		strcat(ELF_fname, ";1");

		/* length of <path>/<"CD" or "DVD">/SLXX_123.99.<title>.iso + 1 NULL terminator. */
		disc_img_pth=malloc(strlen(PathToGameList)+23+strlen(ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].title));
		strcpy(disc_img_pth, PathToGameList);

		if(DisplayData.CurrentlySelectedGameListType==SYSTEM_ISO9660_CD) strcat(disc_img_pth, "/CD/");
		else strcat(disc_img_pth, "/DVD/");

		disc_img_pth[strlen(disc_img_pth)+11]='\0';	/* NULL terminate the string first (Early). */
		memcpy(&disc_img_pth[strlen(disc_img_pth)], ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].ELF_fname, 11);
		sprintf(&disc_img_pth[strlen(disc_img_pth)], ".%s.iso", ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].title);
	}

	result=init_PS2ESDL_sys(ELF_fname, disc_img_pth, DisplayData.CurrentlySelectedGameListType, ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].disctype, PS2ESDL_op_flags, PS2ESDL_cache_sz, ActiveGameList->GameList[ActiveGameList->CurrentlySelectedIndexInList].cdvd_dnas_key);

	free(disc_img_pth);
	if(result<0) goto mainGameSelectionLoop;

	padPortClose(PAD_PORT, PAD_SLOT);

	/* Attempt to launch the game. */
	LaunchGame(disc_img_pth, ELF_fname);

	return 0; /* Should not reach here */
}
