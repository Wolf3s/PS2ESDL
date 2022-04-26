#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <libcdvd.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <smod.h>
#include <stdio.h>
#include <string.h>

#include <gsKit.h>
#include "include/skin.h"
#include "include/GUI.h"
#include "include/system.h"
#include "include/plugin.h"
#include "include/config.h"

/* This is declared just to keep the compiler happy (To have an error-free environment...). */
struct GameList GameLists[SUPPORTED_GAME_FORMATS];

struct ScreenDisplayData DisplayData;

//unsigned char padBuf[256] __attribute__((aligned(64)));

void *USBAdvance_game_list=NULL;
unsigned int USBAdvance_games=0;
void *PS2ESDL_game_list=NULL;
unsigned int PS2ESDL_games=0;

/* The Current Working Directory */
char *currentWorkingDirectory;

/* The path to the game lists. */
char PathToGameList[8];

/* Global configuration. */
extern struct PS2ESDLConfiguration GlobalConfiguration;

extern unsigned int nDeviceDrivers;
extern struct DeviceDriver Devices[MAX_DEVICES];

/* Stuff that are not used, but must be declared to keep the compiler happy. */
GSTEXTURE background;

static int threadID;
static void driveIntrConnectHandler(void *deviceType, void *driveTable){
	iWakeupThread(threadID);
}

int main(int argc,char *argv[])
{
	unsigned int i;

	SifInitRpc(0);
	fioInit();

	/* Do some other things while the IOP is being reset. */
	getCWD(argv, &currentWorkingDirectory);
	LoadPlugins(currentWorkingDirectory);

	/* Startup initilization */
	while(!SifIopReset(NULL, 0)){};

	strcpy(PathToGameList, "mass0:");
//	strcpy(PathToGameList, "sd0:");
	InitBuiltInDrivers();

	DisplayData.SourceDevice=0;	/* "mass:" is always device 0. */
//	DisplayData.SourceDevice=1;

	while(!SifIopSync()){};

	SifInitRpc(0);
	SifInitIopHeap();
	SifLoadFileInit();
	fioInit();

	cdInit(CDVD_INIT_NOCHECK);

	sbv_patch_enable_lmb();

#ifdef DEBUG_TTY_FEEDBACK
	LoadDebugModules();
#endif

	for(i=0; i<nDeviceDrivers; i++) ExecFreeDrivers(&Devices[i]);

	SifLoadFileExit();

	SifAddCmdHandler(12, &driveIntrConnectHandler, NULL);

	threadID=GetThreadId();

	DEBUG_PRINTF("Waiting for the USB device...\n");

	SleepThread();		/* Wait for the USB device to be detected. */

	DEBUG_PRINTF("Installing EE core into EE memory and launching game...\n");

	/* "Path to the game's main executable" "Path to the 1st disc image slice" "System type" "disc type" "Options" "cache size" "DNAS ID */
//	init_PS2ESDL_sys("cdrom0:\\SLPM_550.52;1", "mass0:PS2ESDL/SLPM_55052_00.pdi", SYSTEM_PS2ESDL, SCECdPS2DVD, 0, 2, NULL);
//	LaunchGame("mass0:PS2ESDL/SLPM_55052_00.pdi", "cdrom0:\\SLPM_550.52;1");

//	init_PS2ESDL_sys("cdrom0:\\SLUS_215.50;1", "mass0:PS2ESDL/SLUS_21550_00.pdi", SYSTEM_PS2ESDL, SCECdPS2DVD, 0, 2, NULL);
//	LaunchGame("mass0:PS2ESDL/SLUS_21550_00.pdi", "cdrom0:\\SLUS_215.50;1");

	init_PS2ESDL_sys("cdrom0:\\SLPM_622.27;1", "mass0:PS2ESDL/SLPM_62227_00.pdi", SYSTEM_PS2ESDL, SCECdPS2CD, 0, 2, NULL);
	LaunchGame("mass0:PS2ESDL/SLPM_62227_00.pdi", "cdrom0:\\SLPM_622.27;1");

//	init_PS2ESDL_sys("cdrom0:\\SLUS_210.65;1", "mass0:PS2ESDL/SLUS_21065_00.pdi", SYSTEM_PS2ESDL, SCECdPS2DVD, 0, 2, NULL);
//	LaunchGame("mass0:PS2ESDL/SLUS_21065_00.pdi", "cdrom0:\\SLUS_210.65;1");
//	init_PS2ESDL_sys("cdrom0:\\SLUS_210.65;1", "sd0:PS2ESDL/SLUS_21065_00.pdi", SYSTEM_PS2ESDL, SCECdPS2DVD, 0, 2, NULL);
//	LaunchGame("sd0:PS2ESDL/SLUS_21065_00.pdi", "cdrom0:\\SLUS_210.65;1");

//	LaunchGame("cdrom0:", "cdrom0:\\SLPM_652.70;1");
//	LaunchGame("cdrom0:", "cdrom0:\\SLPM_622.27;1");

	return 0; /* Should not reach here */
}
