#include <gsKit.h>
#include <gsToolkit.h>
#include <kernel.h>
#include <png.h>

#include "include/PS2ESDL_format.h"
#include "include/GUI.h"
#include "include/krom.h"
#include "include/skin.h"

/* Data arrays/structures. */
GSGLOBAL *gsGlobal_settings;
GSFONTM *gsFontM;

/* Graphics */
extern GSTEXTURE mbox_graphic, UI_Skin;

int LoadSkin(char* path, struct ScreenDisplayData* DisplayData){
	char* fname;
	int result, fd;
	static struct SkinParaHeader SkinParaHeader ALIGNED(64);
	static const struct SkinParaHeader SampleSkinParaHeader={
		"PLSF",
		PS2ESDL_SKIN_VER
	};

	fname=malloc(strlen(path)+7);	/* Length of the path + length of "UI.png" + 1 NULL terminator. */
	sprintf(fname, "%sUI.png", path);
	UI_Skin.Delayed=GS_SETTING_ON;
	if(gsKit_texture_png(gsGlobal_settings, &UI_Skin, fname)==0){
		UploadTexture(gsGlobal_settings, &UI_Skin);
		result=1;
	}else result=0;

	sprintf(fname, "%sUI.dat", path);
	if((fd=fioOpen(fname, O_RDONLY))>=0){
		fioRead(fd, &SkinParaHeader, sizeof(struct SkinParaHeader));
		if(memcmp(&SkinParaHeader, &SampleSkinParaHeader, sizeof(struct SkinParaHeader))==0){
			fioRead(fd, &DisplayData->SkinParameters, sizeof(struct SkinParameters));
			fioClose(fd);
		}
		else{
			fioClose(fd);
			goto LoadDefaultOptions;
		}
	}
	else{	/* Load default options. */
LoadDefaultOptions:
		DisplayData->SkinParameters.UIDispX=0;
		DisplayData->SkinParameters.UIDispY=0;

		DisplayData->SkinParameters.ScrollBarX=619;
		DisplayData->SkinParameters.ScrollBarY=77;

		DisplayData->SkinParameters.GameListTypeDispX=12;
		DisplayData->SkinParameters.GameListTypeDispY=34;
		DisplayData->SkinParameters.GameListTypeDispMag=0.4f;
		DisplayData->SkinParameters.GameListHighlightLen=598;

		DisplayData->SkinParameters.VersionDispX=490;
		DisplayData->SkinParameters.VersionDispY=420;
		DisplayData->SkinParameters.VersionDispMag=0.4f;

		DisplayData->SkinParameters.ScrollBarLen=SCROLL_BAR_LENGTH;
		DisplayData->SkinParameters.GameEntryLength=GAME_ENTRY_UNIT_LENGTH;
		DisplayData->SkinParameters.HideVerNumInGameList=1;
		DisplayData->SkinParameters.nGamesToList=GAMES_LIST_MAX;
	}

	free(fname);
	return result;
}
