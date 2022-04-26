#include <gsKit.h>
#include <gsToolkit.h>
#include <png.h>

#include "include/PS2ESDL_format.h"
#include "include/skin.h"
#include "include/GUI.h"
#include "include/krom.h"

/* Data arrays/structures. */
GSGLOBAL *gsGlobal_settings;
GSFONTM *gsFontM;

extern struct ScreenDisplayData DisplayData;

/* Graphics */
extern GSTEXTURE mbox_graphic;
GSTEXTURE UI_Skin;

/* Colors */
extern u64 gsBlack,gsWhiteF,gsWhite,gsGray,gsDBlue,gsBlueTransSel;

void messageBox(GSTEXTURE *mbox, const char *message, const char *title){
	gsKit_prim_sprite_texture(gsGlobal_settings, mbox,
							150,			/* X1 */
							100,			/* Y1 */
							0,			/* U1 */
							0,			/* V1 */
							150+mbox->Width,	/* X2 */
							100+mbox->Height,	/* Y2 */
							mbox->Width,		/* U2 */
							mbox->Height,		/* V2 */
							3,			/* Z */
							GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00)); /* RGBAQ */

	gsKit_fontm_print_scaled(gsGlobal_settings, gsFontM, 160, 106, 3, 0.4f, gsWhiteF, title);
	gsKit_fontm_print_scaled(gsGlobal_settings, gsFontM, 160, 150, 3, 0.4f, gsWhiteF, message);
}

int UploadTexture(GSGLOBAL *gsGlobal, GSTEXTURE* Texture){
	u32 VramTextureSize = gsKit_texture_size(Texture->Width, Texture->Height, Texture->PSM);
	Texture->Vram = gsKit_vram_alloc(gsGlobal, VramTextureSize, GSKIT_ALLOC_USERBUFFER);
	if(Texture->Vram == GSKIT_ALLOC_ERROR)
	{
		printf("VRAM Allocation Failed. Will not upload texture.\n");
		return -255;
	}

	gsKit_texture_send_inline(gsGlobal, Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_NONE);
	return 0;
}

int load_embeddedGraphics(GSGLOBAL *gsGlobal, GSTEXTURE *Texture, void *buffer, unsigned int graphic_sz)
{
	GSBITMAP Bitmap;
	int x, y;
	int cy;
	u32 FTexSize;
	u8  *image;
	u8  *p;

	memcpy(&Bitmap.FileHeader, buffer, sizeof(Bitmap.FileHeader));
	memcpy(&Bitmap.InfoHeader, (void *)(buffer+sizeof(Bitmap.FileHeader)), sizeof(Bitmap.InfoHeader));

	Texture->Width = Bitmap.InfoHeader.Width;
	Texture->Height = Bitmap.InfoHeader.Height;
	Texture->Filter = GS_FILTER_LINEAR;

	if(Bitmap.InfoHeader.BitCount == 4)	/* Broken somewhere in this function (Causes corruption of on-screen graphics). :( */
	{
		Texture->PSM = GS_PSM_T4;
		Texture->Clut = memalign(128, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));
		Texture->ClutPSM = GS_PSM_CT32;

		memset(Texture->Clut, 0, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));
		Texture->VramClut = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(8, 2, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);

		memcpy(Texture->Clut, (void *)(buffer+54), Bitmap.InfoHeader.ColorUsed*sizeof(u32));

		GSBMCLUT *clut = (GSBMCLUT *)Texture->Clut;
		int i;
		for (i = Bitmap.InfoHeader.ColorUsed; i < 16; i++)
		{
			memset(&clut[i], 0, sizeof(clut[i]));
		}

		for (i = 0; i < 16; i++)
		{
			u8 tmp = clut[i].Blue;
			clut[i].Blue = clut[i].Red;
			clut[i].Red = tmp;
			clut[i].Alpha = 0;
		}
	}
	else if(Bitmap.InfoHeader.BitCount == 8)
	{
		Texture->PSM = GS_PSM_T8;
		Texture->Clut = memalign(128, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));
		Texture->ClutPSM = GS_PSM_CT32;

		memset(Texture->Clut, 0, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));
		Texture->VramClut = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(16, 16, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);

		memcpy(Texture->Clut, (void *)(buffer+54), Bitmap.InfoHeader.ColorUsed*sizeof(u32));

		GSBMCLUT *clut = (GSBMCLUT *)Texture->Clut;
		int i;
		for (i = Bitmap.InfoHeader.ColorUsed; i < 256; i++)
		{
			memset(&clut[i], 0, sizeof(clut[i]));
                }

		for (i = 0; i < 256; i++)
		{
			u8 tmp = clut[i].Blue;
			clut[i].Blue = clut[i].Red;
			clut[i].Red = tmp;
			clut[i].Alpha = 0;
		}

		// rotate clut
		for (i = 0; i < 256; i++)
		{
			if ((i&0x18) == 8)
			{
				GSBMCLUT tmp = clut[i];
				clut[i] = clut[i+8];
				clut[i+8] = tmp;
			}
		}
	}
	else if(Bitmap.InfoHeader.BitCount == 16)
	{
		Texture->PSM = GS_PSM_CT16;
		Texture->VramClut = 0;
		Texture->Clut = NULL;
	}
	else if(Bitmap.InfoHeader.BitCount == 24)
	{
		Texture->PSM = GS_PSM_CT24;
		Texture->VramClut = 0;
		Texture->Clut = NULL;
	}
	else if(Bitmap.InfoHeader.BitCount == 32)
	{
		Texture->PSM = GS_PSM_CT32;
		Texture->VramClut = 0;
		Texture->Clut = NULL;
	}
	else return -1;

	FTexSize = graphic_sz - Bitmap.FileHeader.Offset;

	u32 TextureSize = gsKit_texture_size_ee(Texture->Width, Texture->Height, Texture->PSM);

	Texture->Mem = memalign(128, TextureSize);

	if(Bitmap.InfoHeader.BitCount == 24)
	{
		image = memalign(128, FTexSize);
		if (image == NULL) return -2;

		memcpy(image, (void *)(buffer+Bitmap.FileHeader.Offset), FTexSize);

		p = (void *)((u32)Texture->Mem | 0x30000000);
		for (y=Texture->Height-1,cy=0; y>=0; y--,cy++) {
			for (x=0; x<Texture->Width; x++) {
				p[(y*Texture->Width+x)*3+2] = image[(cy*Texture->Width+x)*3+0];
				p[(y*Texture->Width+x)*3+1] = image[(cy*Texture->Width+x)*3+1];
				p[(y*Texture->Width+x)*3+0] = image[(cy*Texture->Width+x)*3+2];
			}
		}
		free(image);
	}
	else if(Bitmap.InfoHeader.BitCount == 16)
	{
		printf("16-bit BMP not supported yet.\n");
	}
	else if(Bitmap.InfoHeader.BitCount == 8 || Bitmap.InfoHeader.BitCount == 4 )
	{
		char *tex = (char *)((u32)Texture->Mem | 0x30000000);
		image = memalign(128, FTexSize);
		if(image == NULL) return -2;

		memcpy(image, (void *)(buffer+Bitmap.FileHeader.Offset), FTexSize);

		for (y=Texture->Height-1; y>=0; y--)
		{
			if(Bitmap.InfoHeader.BitCount == 8) memcpy(&tex[y*Texture->Width], &image[(Texture->Height-y-1)*Texture->Width], Texture->Width);
			else memcpy(&tex[y*(Texture->Width/2)], &image[(Texture->Height-y-1)*(Texture->Width/2)], Texture->Width / 2);
		}
		free(image);

		if(Bitmap.InfoHeader.BitCount == 4)
		{
			int byte;
			u8 *tmpdst = (u8 *)((u32)Texture->Mem | 0x30000000);
			u8 *tmpsrc = (u8 *)tex;

			for(byte = 0; byte < FTexSize; byte++)
			{
				tmpdst[byte] = (tmpsrc[byte] << 4) | (tmpsrc[byte] >> 4);
			}
		}
	}
	else if(Bitmap.InfoHeader.BitCount == 32)
	{
		image = memalign(128, FTexSize);
                if(image == NULL) return -2;

		memcpy(image, (void *)(buffer+Bitmap.FileHeader.Offset), FTexSize);

		p = (void *)((u32)Texture->Mem | 0x30000000);
		for (y=Texture->Height-1,cy=0; y>=0; y--,cy++) {
			for (x=0; x<Texture->Width; x++) {
				p[(y*Texture->Width+x)*4+0] = image[(cy*Texture->Width+x)*4+2];
				p[(y*Texture->Width+x)*4+1] = image[(cy*Texture->Width+x)*4+1];
				p[(y*Texture->Width+x)*4+2] = image[(cy*Texture->Width+x)*4+3];
				p[(y*Texture->Width+x)*4+3] = image[(cy*Texture->Width+x)*4+0];
			}
		}
		free(image);
	}

	gsKit_setup_tbw(Texture);
	return 0;
}

void redrawGameListScreenBackground(struct GameList *ActiveGameList, unsigned char current_sys_type, GSTEXTURE *background){
	unsigned int i, game_number;
	unsigned int list_base, max_displayed_titles, UI_OffsetX, UI_OffsetY;
	int scrollbar_length;
	static unsigned char game_title[MAX_TITLE_LENGTH+1];
	float scaleRatio;

  /* In summary: The screen is redrawn in this order: */
  /*	1. Clear the screen and draw the background(If one was loaded).
	2. Draw the UI.
	5. Highlight the currently selected game entry.
	6. Draw the game titles in the list.
	7. Draw the text at the bottom.
	8. Synchronise the EE with the GS.
	9. Start drawing the screen. */

	/* !!NOTE!! If current_sys_type==SYSTEM_INVALID, ActiveGameList might be NULL. */

	drawBackground(background);

	if(DisplayData.ResourcesLoaded&UI_RESOURCE_SKIN){
		//gsKit_set_test(gsGlobal_settings, GS_ATEST_ON); // Enable AT for this primitive

		/* Calculate the scale ratios for the skin, on the current display mode. */
		gsKit_prim_sprite_texture(gsGlobal_settings, &UI_Skin,
							DisplayData.SkinParameters.UIDispX,			/* X1 */
							DisplayData.SkinParameters.UIDispY,			/* Y1 */
							0,							/* U1 */
							0,							/* V1 */
							DisplayData.SkinParameters.UIDispX+UI_Skin.Width,	/* X2 */
							DisplayData.SkinParameters.UIDispY+UI_Skin.Height,	/* Y2 */
							UI_Skin.Width,		/* U2 */
							UI_Skin.Height,		/* V2 */
							1,			/* Z */
							GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00)); /* RGBAQ */

		//gsKit_set_test(gsGlobal_settings, GS_ATEST_OFF); // Disable AT for other primitives
	}

	UI_OffsetX=DisplayData.SkinParameters.UIDispX;
	UI_OffsetY=DisplayData.SkinParameters.UIDispY;

	/* Draw the scroll bar. */
	if((current_sys_type!=SYSTEM_INVALID)&&(ActiveGameList->nGames>DisplayData.SkinParameters.nGamesToList)){	/* Allow the scroll bar to scroll, only if there are more games to be listed than what can be shown at 1 time. */
		scaleRatio=(float)ActiveGameList->CurrentlySelectedIndexInList/(float)ActiveGameList->nGames;	/* Calculate how far down the games list the user is now at (In percentage). */

		scrollbar_length=DisplayData.SkinParameters.ScrollBarLen-(ActiveGameList->nGames-DisplayData.SkinParameters.nGamesToList)*DisplayData.SkinParameters.GameEntryLength; /* Calculate the length of the scroll bar (This is a point-based system where the length of the bar will be shortened by 5 pixels for every game that's not listed). */
		if(scrollbar_length<5) scrollbar_length=DisplayData.SkinParameters.GameEntryLength;				/* Make sure that the scroll bar has a minimum length. */

		i=(DisplayData.SkinParameters.ScrollBarLen-scrollbar_length)*scaleRatio; /* Calculate the new position of the scroll bar (Offset). */
	}
	else{ /* If there are fewer games than what can be listed on the screen at any one time. */
		i=0;
		scrollbar_length=DisplayData.SkinParameters.ScrollBarLen; /* Maximum length. */
	}

	gsKit_prim_quad(gsGlobal_settings, DisplayData.SkinParameters.ScrollBarX, DisplayData.SkinParameters.ScrollBarY + i, DisplayData.SkinParameters.ScrollBarX+9, DisplayData.SkinParameters.ScrollBarY + i, DisplayData.SkinParameters.ScrollBarX, DisplayData.SkinParameters.ScrollBarY + scrollbar_length + i, DisplayData.SkinParameters.ScrollBarX+9, DisplayData.SkinParameters.ScrollBarY + scrollbar_length + i, 1, gsGray);

	/**********************************************************************************/
	if(current_sys_type!=SYSTEM_INVALID){
		/* Display the label of the currently displayed format type */
		gsKit_fontm_print_scaled(gsGlobal_settings, gsFontM, DisplayData.SkinParameters.GameListTypeDispX, DisplayData.SkinParameters.GameListTypeDispY, 3, DisplayData.SkinParameters.GameListTypeDispMag, gsWhiteF, ActiveGameList->label);

		/* Highlight the currently selected entry */
		gsKit_prim_quad(gsGlobal_settings, 12+UI_OffsetX, 78+UI_OffsetY+(ActiveGameList->CurrentlySelectedIndexOnScreen*18), 12.0f+UI_OffsetX, 97+UI_OffsetY+(ActiveGameList->CurrentlySelectedIndexOnScreen*18), 12+UI_OffsetX+DisplayData.SkinParameters.GameListHighlightLen, 78+UI_OffsetY+(ActiveGameList->CurrentlySelectedIndexOnScreen*18), 12+UI_OffsetX+DisplayData.SkinParameters.GameListHighlightLen, 97+UI_OffsetY+(ActiveGameList->CurrentlySelectedIndexOnScreen*18), 1, gsBlueTransSel);

		/* Note that ANY calculations done here are related to calculating OFFSETs!! */
		max_displayed_titles=(ActiveGameList->nGames>DisplayData.SkinParameters.nGamesToList)?DisplayData.SkinParameters.nGamesToList:ActiveGameList->nGames;

		if(ActiveGameList->CurrentlySelectedIndexOnScreen==0) list_base=ActiveGameList->CurrentlySelectedIndexInList;	/* For when the selection is the entry at the top of the visible list. */
		else if(ActiveGameList->CurrentlySelectedIndexOnScreen==(DisplayData.SkinParameters.nGamesToList-1)) list_base=ActiveGameList->CurrentlySelectedIndexInList-(DisplayData.SkinParameters.nGamesToList-1);	/* Bottom. */
		else list_base=ActiveGameList->old_list_bottom_ID-(max_displayed_titles-1);				/* For when the selection is the one of the entries in the middle of the visible list. */

		game_number=0;
		for(i=list_base;i<(list_base+max_displayed_titles);i++){
			memset(game_title, 0, sizeof(game_title));
			memcpy(game_title, ActiveGameList->GameList[i].title, MAX_TITLE_LENGTH);

			gsKit_krom_print_scaled(gsGlobal_settings, 16+UI_OffsetX, 77+UI_OffsetY+(game_number*18), 3, 0.6f, gsWhite, game_title);
			game_number++;
		}
	}

	if(!DisplayData.SkinParameters.HideVerNumInGameList) gsKit_fontm_print_scaled(gsGlobal_settings, gsFontM, DisplayData.SkinParameters.VersionDispX, DisplayData.SkinParameters.VersionDispY, 3, DisplayData.SkinParameters.VersionDispMag, gsWhiteF, PS2ESDL_VERSION);
}

void update_screen(void){
	gsKit_sync_flip(gsGlobal_settings);
	gsKit_queue_exec(gsGlobal_settings);
}

void drawBackground(GSTEXTURE *background){
	gsKit_clear(gsGlobal_settings, gsBlack);	/* Clear screen */

	if(DisplayData.ResourcesLoaded&UI_RESOURCE_BKGND){
					gsKit_prim_sprite_texture(gsGlobal_settings, background,	/* Redraw the background, if one was loaded */
									0.0f,  /* X1 */
									0.0f,  /* Y1 */
									0.0f,  /* U1 */
									0.0f,  /* V1 */
									background->Width,	/* X2 */
									background->Height,	/* Y2 */
									background->Width,	/* U2 */
									background->Height,	/* V2 */
									0,			/* Z */
									GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00)); /* RGBAQ */
	}
}
