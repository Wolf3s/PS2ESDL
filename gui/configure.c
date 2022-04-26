#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <stdio.h>
#include <string.h>

#include <dmaKit.h>
#include <gsKit.h>
#include <gsToolkit.h>
#include <libmc.h>
#include <libpad.h>

#include "include/GUI.h"
#include "include/system.h"
#include "include/config.h"
#include "include/skin.h"

/* Data arrays/structures. */
GSGLOBAL *gsGlobal_settings;
GSFONTM *gsFontM;

/* Global configuration. */
struct PS2ESDLConfiguration GlobalConfiguration;
extern struct ScreenDisplayData DisplayData;

/* Graphics */
extern GSTEXTURE mbox_graphic;
extern GSTEXTURE background;
extern GSTEXTURE UI_Skin;

/* Colors */
extern u64 gsBlack,gsWhiteF,gsWhite,gsGray,gsDBlue,gsBlueTransSel;

/* The Current Working Directory */
extern char *currentWorkingDirectory;

/* Function prototypes of local functions. */
static inline void redrawConfigScreen(int currentSelection, struct MenuOption *menuOptions, int nMenuOptions);

/* This is present in gsInit.c of gsKit, but there's no function prototype of it in any of the gsKit header files. :( */
void gsKit_set_buffer_attributes(GSGLOBAL *gsGlobal);

static int performScreenAdjustment(GSGLOBAL *gsGlobal, GSTEXTURE *background){
	u32 pad1_status;
	int XPosition_Offset, YPosition_Offset;
	unsigned char instructions[256], selectButton, cancelButton;

	XPosition_Offset=GlobalConfiguration.X_offset;
	YPosition_Offset=GlobalConfiguration.Y_offset;

	selectButton=(GlobalConfiguration.selectButton==PAD_CIRCLE)?'O':'X';
	cancelButton=(GlobalConfiguration.cancelButton==PAD_CIRCLE)?'O':'X';

	do{
		setScreenPos(gsGlobal_settings, XPosition_Offset, YPosition_Offset);

		pad1_status=read_pad1_status();

		/* Add a small delay between pad reads. */
		delay(PAD_POLL_DELAY_MS/2);

		redrawGameListScreenBackground(NULL, SYSTEM_INVALID, background);
		sprintf(instructions, "D-PAD: Position of the screen.\n\nPress %c to save and quit.\nPress %c to quit without saving.\n", selectButton, cancelButton);
		messageBox(&mbox_graphic, instructions, "Screen Positioning correction");

		/* Top */
		gsKit_prim_quad(gsGlobal_settings, 0, 0, 0, 2, DisplayData.displayLength, 0, DisplayData.displayLength, 2, 5, GS_SETREG_RGBAQ(0xFF,0x00,0x00,0x00,0x00));
		/* Left */
		gsKit_prim_quad(gsGlobal_settings, 0, 0, 2, 0, 0, DisplayData.displayHeight, 2, DisplayData.displayHeight, 5, GS_SETREG_RGBAQ(0xFF,0x00,0x00,0x00,0x00));
		/* Bottom */
		gsKit_prim_quad(gsGlobal_settings, 0, DisplayData.displayHeight-2, 0, DisplayData.displayHeight, DisplayData.displayLength, DisplayData.displayHeight-2, DisplayData.displayLength, DisplayData.displayHeight, 5, GS_SETREG_RGBAQ(0xFF,0x00,0x00,0x00,0x00));
		/* Right */
		gsKit_prim_quad(gsGlobal_settings, DisplayData.displayLength-2, 0, DisplayData.displayLength, 0, DisplayData.displayLength-2, DisplayData.displayHeight, DisplayData.displayLength, DisplayData.displayHeight, 5, GS_SETREG_RGBAQ(0xFF,0x00,0x00,0x00,0x00));

		update_screen();

		if(pad1_status&PAD_UP) YPosition_Offset--;
		if(pad1_status&PAD_DOWN) YPosition_Offset++;
		if(pad1_status&PAD_LEFT) XPosition_Offset--;
		if(pad1_status&PAD_RIGHT) XPosition_Offset++;
	}while(!(pad1_status&GlobalConfiguration.selectButton)&&!(pad1_status&GlobalConfiguration.cancelButton));

	if(pad1_status&GlobalConfiguration.selectButton){
		GlobalConfiguration.X_offset=XPosition_Offset;
		GlobalConfiguration.Y_offset=YPosition_Offset;

		waitPad1Clear();

		return 1;
	}
	else{
		setScreenPos(gsGlobal_settings, GlobalConfiguration.X_offset, GlobalConfiguration.Y_offset);

		waitPad1Clear();
		return 0;
	}
}

struct selectorMenuTemplate{
	char options[4][16];
	unsigned char selectedOption;
	unsigned char nOptions;
};

static struct selectorMenuTemplate signalList={
	{
		"NTSC",
		"PAL",
		"720x480P",
		"\0",
	},

	0,	/* 0=GS_MODE_NTSC; 1=GS_MODE_PAL; 2=GS_MODE_DTV_480P; */
	3
};

static struct selectorMenuTemplate DisableFragCheck={
	{
		"Enabled",
		"Disabled"
		"\0",
		"\0",
	},

	0,	/* 0=Check enabled; 1=Check disabled. */
	2
};

static struct selectorMenuTemplate selectButtonOption={
	{
		"CIRCLE",
		"CROSS"
		"\0",
		"\0",
	},

	0,	/* 0=Circle; 1=Cross. */
	2
};

static struct selectorMenuTemplate DisableDisplayModeOverrideOption={
	{
		"Disabled",
		"Enabled"
		"\0",
		"\0",
	},

	0,	/* 0=Disabled; 1=Enabled. */
	2
};

static struct selectorMenuTemplate EnableTitleSortingOption={
	{
		"Disabled",
		"Enabled"
		"\0",
		"\0",
	},

	0,	/* 0=Disabled; 1=Enabled. */
	2
};

static struct selectorMenuTemplate EnablePS2LOGO={
	{
		"Disabled",
		"Enabled"
		"\0",
		"\0",
	},

	0,	/* 0=Disabled; 1=Enabled. */
	2
};

static struct MenuOption menuOptions[]={
	{"Screen positioning correction", MENU_TYPE_SUBMENU, &performScreenAdjustment},
	{"Display mode: ", MENU_TYPE_SELECTOR, &signalList},
	{"Game fragmentation check: ", MENU_TYPE_SELECTOR, &DisableFragCheck},
	{"Select button: ", MENU_TYPE_SELECTOR, &selectButtonOption},
	{"Disable video mode override: ", MENU_TYPE_SELECTOR, &DisableDisplayModeOverrideOption},
	{"Enable game title sorting: ", MENU_TYPE_SELECTOR, &EnableTitleSortingOption},
	{"Enable PS2 logo: ", MENU_TYPE_SELECTOR, &EnablePS2LOGO},
	{"Exit", MENU_TYPE_EXIT, NULL},
};

int configurePS2ESDL(GSTEXTURE *background){
	u32 pad1_status;
	int currentSelection=0, nMenuOptions, i;
	int (*configFunction)(GSGLOBAL *gsGlobal, GSTEXTURE *background);
	unsigned char OriginalVideoMode, WasTitleSortingEnabled;

	nMenuOptions=0;
	for(i=0; (menuOptions[i].menuType!=MENU_TYPE_EXIT); i++) nMenuOptions++;
	nMenuOptions++;

	/* Load the user's preferences. */
	OriginalVideoMode=GlobalConfiguration.displayMode; /* Record the currently used video mode. */
	if(GlobalConfiguration.displayMode==GS_MODE_NTSC) signalList.selectedOption=0;
	else if(GlobalConfiguration.displayMode==GS_MODE_PAL) signalList.selectedOption=1;
	else signalList.selectedOption=2;

	DisableFragCheck.selectedOption=(GlobalConfiguration.Services&DISABLE_FRAG_CHECK)?1:0;
	DisableDisplayModeOverrideOption.selectedOption=(GlobalConfiguration.Services&DISABLE_VIDEO_MODE_OVERRIDE)?1:0;
	selectButtonOption.selectedOption=(GlobalConfiguration.selectButton==PAD_CIRCLE)?0:1;
	WasTitleSortingEnabled=EnableTitleSortingOption.selectedOption=(GlobalConfiguration.Services&ENABLE_TITLE_SORTING)?1:0;
	EnablePS2LOGO.selectedOption=(GlobalConfiguration.Services&ENABLE_PS2LOGO)?1:0;

	do{
		drawBackground(background);
		redrawConfigScreen(currentSelection, menuOptions, nMenuOptions);
		update_screen();

		/* Add a small delay between pad reads. */
		delay(PAD_POLL_DELAY_MS);
		pad1_status=read_pad1_status();

		if((pad1_status&PAD_UP)&&(currentSelection>0)) currentSelection--;
		else if((pad1_status&PAD_DOWN)&&((currentSelection+1)<nMenuOptions)) currentSelection++;
		else if(pad1_status&PAD_LEFT){
			waitPad1Clear();

			if(menuOptions[currentSelection].menuType==MENU_TYPE_SELECTOR){
				if(((struct selectorMenuTemplate *)menuOptions[currentSelection].optionData)->selectedOption>0) ((struct selectorMenuTemplate *)menuOptions[currentSelection].optionData)->selectedOption--;
			}
		}
		else if(pad1_status&PAD_RIGHT){
			waitPad1Clear();

			if(menuOptions[currentSelection].menuType==MENU_TYPE_SELECTOR){
				if(((struct selectorMenuTemplate *)menuOptions[currentSelection].optionData)->selectedOption<(((struct selectorMenuTemplate *)menuOptions[currentSelection].optionData)->nOptions-1)) ((struct selectorMenuTemplate *)menuOptions[currentSelection].optionData)->selectedOption++;
			}
		}
		else if(pad1_status&GlobalConfiguration.selectButton){
			waitPad1Clear();

			if((menuOptions[currentSelection].menuType==MENU_TYPE_SUBMENU)&&(menuOptions[currentSelection].optionData!=NULL)){
			    configFunction=menuOptions[currentSelection].optionData;
			    configFunction(gsGlobal_settings, background);
			}
			else if(menuOptions[currentSelection].menuType==MENU_TYPE_EXIT) break;
		}
		else if(pad1_status&GlobalConfiguration.cancelButton){
			waitPad1Clear();
			return 0;
		}
	}while(1);

	/* Process the user's choices.... */
	if(signalList.selectedOption==0) GlobalConfiguration.displayMode=GS_MODE_NTSC;
	else if(signalList.selectedOption==1) GlobalConfiguration.displayMode=GS_MODE_PAL;
	else GlobalConfiguration.displayMode=GS_MODE_DTV_480P;

	if(selectButtonOption.selectedOption==0){
		GlobalConfiguration.selectButton=PAD_CIRCLE;
		GlobalConfiguration.cancelButton=PAD_CROSS;
	}
	else{
		GlobalConfiguration.selectButton=PAD_CROSS;
		GlobalConfiguration.cancelButton=PAD_CIRCLE;
	}

	GlobalConfiguration.Services=0;
	if(DisableFragCheck.selectedOption) GlobalConfiguration.Services|=DISABLE_FRAG_CHECK;
	if(DisableDisplayModeOverrideOption.selectedOption) GlobalConfiguration.Services|=DISABLE_VIDEO_MODE_OVERRIDE;
	if(EnableTitleSortingOption.selectedOption) GlobalConfiguration.Services|=ENABLE_TITLE_SORTING;

	if(WasTitleSortingEnabled!=EnableTitleSortingOption.selectedOption) LoadGameListsFromDevices();	/* Reload the game lists to display the game titles with or without sorting enabled, only if this option was adjusted. */

	if(GlobalConfiguration.displayMode!=OriginalVideoMode){
		GlobalConfiguration.X_offset=GlobalConfiguration.Y_offset=0; /* Erase all screen positioning correction settings. */
		ApplyVideoOptions(); /* Change the screen display mode only if the recorded mode is different from the one currently used. */
	}

	if(EnablePS2LOGO.selectedOption) GlobalConfiguration.Services|=ENABLE_PS2LOGO;

	messageBox(&mbox_graphic, "Saving configuration...\n\n\nPlease do not unplug your device\nor switch off the console.", "Saving...");
	update_screen();

	return(SaveConfiguration()?1:-1);
}

static inline void redrawConfigScreen(int currentSelection, struct MenuOption *menuOptions, int nMenuOptions){
	int i, textLength;
	u64 colour;
	unsigned char selectButton, cancelButton, message[128];

	selectButton=(GlobalConfiguration.selectButton==PAD_CIRCLE)?'O':'X';
	cancelButton=(GlobalConfiguration.cancelButton==PAD_CIRCLE)?'O':'X';

	/* Draw that help text that's at the bottom of the screen. */
	sprintf(message, "%c \t\t\t- Select\t\t\t\t\t%c \t- Cancel", selectButton, cancelButton);
	gsKit_fontm_print_scaled(gsGlobal_settings, gsFontM, 10, 400, 3, 0.3f, gsWhiteF, message);

	for(i=0; i<nMenuOptions; i++){
		if(menuOptions[i].menuType!=MENU_TYPE_SELECTOR){
			colour=(i==currentSelection)?GS_SETREG_RGBAQ(0xA0,0xA0,0x00,0xFF,0x00):gsWhiteF;
			gsKit_fontm_print_scaled(gsGlobal_settings, gsFontM, 14, 79+(i*FONT_CHAR_WIDTH), 3, FONT_SCALE, colour, menuOptions[i].title);
		}
		else{
			gsKit_fontm_print_scaled(gsGlobal_settings, gsFontM, 14, 79+(i*FONT_CHAR_WIDTH), 3, FONT_SCALE, gsWhiteF, menuOptions[i].title);

			/* !!NOTE!!
				For the calculation of the coordinates of where the text for the next few gsKit_fontm_print_scaled() calls:
					The size of 1 character that's drawn by gsKit_fontm_print_scaled() is about 12px, so you can guess why there's a need to multiply some of the values by 18. ;)
					>>>Also, add a spacing of about 24px to in-between 2 options that are listed on the same line.
		    	*/
			colour=(i==currentSelection)?GS_SETREG_RGBAQ(0xA0,0xA0,0x00,0xFF,0x00):GS_SETREG_RGBAQ(0x30,0x30,0xA0,0xFF,0x00);

			textLength=strlen(((struct selectorMenuTemplate *)menuOptions[i].optionData)->options[((struct selectorMenuTemplate *)menuOptions[i].optionData)->selectedOption]);
			gsKit_fontm_print_scaled(gsGlobal_settings, gsFontM, 400, 79+(i*FONT_CHAR_WIDTH), 3, FONT_SCALE, colour, ((struct selectorMenuTemplate *)menuOptions[i].optionData)->options[((struct selectorMenuTemplate *)menuOptions[i].optionData)->selectedOption]);
		}
	}

	gsKit_fontm_print_scaled(gsGlobal_settings, gsFontM, 490, 420, 3, 0.4f, gsWhiteF, PS2ESDL_VERSION);
}

void setScreenRes(GSGLOBAL *gsGlobal){
	/* These are the parameters used by most supported video modes. */
	gsGlobal->Width=640;
	gsGlobal->Interlace=GS_INTERLACED;
	gsGlobal->Field=GS_FIELD;
	gsGlobal->PSM=GS_PSM_CT32;

	switch(gsGlobal->Mode){
		case GS_MODE_PAL:
			gsGlobal->Height=512;
			break;
		case GS_MODE_DTV_480P:
			gsGlobal->Width=720;
			gsGlobal->Height=480;
			gsGlobal->Interlace=GS_NONINTERLACED;
			gsGlobal->Field=GS_FRAME;
			gsGlobal->PSM=GS_PSM_CT16;
			break;
		default:	/* NTSC and other modes. */
			gsGlobal->Height=448;
	}
}

void setScreenPos(GSGLOBAL *gsGlobal, int X_Offset, int Y_Offset){
	/* Automatically attempt to re-centre the screen when a larger than normal resolution is used.
		Note: DW and DH contain the "true" resolution of the currently used video mode. At low resolutions (E.g. not 1080I or VGA 1028x1024), DW is doubled.
			Since none of those high resolution video modes are currently supported, we don't have any of such exceptions. :) */
	X_Offset+=((gsGlobal->Width-UI_VISIBLE_SCREEN_LENGTH)/2);
	Y_Offset+=((gsGlobal->Height-UI_VISIBLE_SCREEN_HEIGHT)/2);

	GS_SET_DISPLAY1(gsGlobal->StartX+X_Offset,	// X position in the display area (in VCK unit
		gsGlobal->StartY+Y_Offset,		// Y position in the display area (in Raster u
		gsGlobal->MagH,			// Horizontal Magnification
		gsGlobal->MagV,			// Vertical Magnification
		gsGlobal->DW - 1,		// Display area width
		gsGlobal->DH - 1);		// Display area height

	GS_SET_DISPLAY2(gsGlobal->StartX+X_Offset,	// X position in the display area (in VCK units)
		gsGlobal->StartY+Y_Offset,		// Y position in the display area (in Raster units)
		gsGlobal->MagH,			// Horizontal Magnification
		gsGlobal->MagV,			// Vertical Magnification
		gsGlobal->DW - 1,		// Display area width
		gsGlobal->DH - 1);		// Display area height
}

void ApplyVideoOptions(void){
	/* Apply options from the settings recorded in the loaded configuration file. */
	gsGlobal_settings->Mode = GlobalConfiguration.displayMode;

	gsKit_vram_clear(gsGlobal_settings);
	setScreenRes(gsGlobal_settings);
	gsKit_init_screen(gsGlobal_settings);	/* Apply settings. */
	setScreenPos(gsGlobal_settings, GlobalConfiguration.X_offset, GlobalConfiguration.Y_offset);

	gsKit_fontm_upload(gsGlobal_settings, gsFontM);
	UploadTexture(gsGlobal_settings, &mbox_graphic);
	if(DisplayData.ResourcesLoaded&UI_RESOURCE_BKGND) UploadTexture(gsGlobal_settings, &background);
	if(DisplayData.ResourcesLoaded&UI_RESOURCE_SKIN) UploadTexture(gsGlobal_settings, &UI_Skin);

	gsKit_mode_switch(gsGlobal_settings, GS_ONESHOT);
}

static void loadDefaultConfiguration(void){
	/* Clear the global configuration data structure. */
	memset(&GlobalConfiguration, 0, sizeof(struct PS2ESDLConfiguration));

	/* GlobalConfiguration.X_offset and GlobalConfiguration.Y_offset will be set to 0 because of the line above. */
	GlobalConfiguration.displayMode=0;		/* "Auto" */

	GlobalConfiguration.Services=0;			/* Do not enable any services. */
	GlobalConfiguration.selectButton=PAD_CIRCLE;
	GlobalConfiguration.cancelButton=PAD_CROSS;
}

static int configurationLoaded=0;

int LoadConfiguration(void){
	unsigned char *cfgPath;
	int fd;

	if(configurationLoaded) return 1; /* Don't attempt to load a configuration file if one has been loaded before. */

	loadDefaultConfiguration();

	cfgPath=malloc(strlen(currentWorkingDirectory)+19);
	sprintf(cfgPath, "%sPS2ESDL_config.dat", currentWorkingDirectory);

	fd=fioOpen(cfgPath, O_RDONLY);
	free(cfgPath);

	if(fd>=0){
		if(fioRead(fd, &GlobalConfiguration, sizeof(struct PS2ESDLConfiguration))!=sizeof(struct PS2ESDLConfiguration)) goto configClose;
		if(memcmp(GlobalConfiguration.signature, "PCF", 3)||(GlobalConfiguration.version!=PS2ESDL_CONFIG_VERSION)) goto configClose; /* Error: Not a supported configuration file. */

		fioClose(fd);

		configurationLoaded=1;
		return 1;
	}
	else{ /* Error loading the configuration file. Load Defaults. */
		return 0;
	}

configClose:
	fioClose(fd);
	loadDefaultConfiguration();

	return 0;
}

inline int SaveConfiguration(void){
	unsigned char *cfgPath;
	int fd;

	cfgPath=malloc(strlen(currentWorkingDirectory)+19);
	sprintf(cfgPath, "%sPS2ESDL_config.dat", currentWorkingDirectory);

	/* Generate a new configuration file */
	memcpy(GlobalConfiguration.signature, "PCF", 3);
	GlobalConfiguration.version=PS2ESDL_CONFIG_VERSION;
	GlobalConfiguration.displayMode=gsGlobal_settings->Mode;

	/* NOTE!! GlobalConfiguration.X_offset, GlobalConfiguration.Y_offset and GlobalConfiguration.DisableFragCheck are already recorded. */

	fd=fioOpen(cfgPath, O_WRONLY|O_TRUNC|O_CREAT);
	free(cfgPath);

	if(fd>=0){
		/* Recorded the currently used options into the configuration file. */
		if(fioWrite(fd, &GlobalConfiguration, sizeof(struct PS2ESDLConfiguration))!=sizeof(struct PS2ESDLConfiguration)) goto configClose;
		fioClose(fd);

		return 1;
	}

	/* Error saving the configuration file. */
configClose:
	fioClose(fd);
	return 0;
}
