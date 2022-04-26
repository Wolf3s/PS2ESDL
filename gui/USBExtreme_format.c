#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <fileio.h>

#include <dmaKit.h>
#include <gsKit.h>
#include <gsToolkit.h>

#include "include/PS2ESDL_format.h"
#include "include/GUI.h"

extern struct GameList GameLists[SUPPORTED_GAME_FORMATS];

struct USBExtreme_cfg_entry{
	unsigned char title[32];
	unsigned char ELF_fname[14];
	unsigned short int unknown;
	unsigned char disc_type;
	unsigned char unknown2[15];     
};

int load_USBExtreme_list(const char *path){
 unsigned long int i, list_size;
 int game_list_fd;
 struct USBExtreme_cfg_entry *GameEntries;
 char CfgPath[64];

 sprintf(CfgPath, "%sUL.cfg", path);

 if((game_list_fd=fioOpen(CfgPath, O_RDONLY))<0){
	GameLists[SYSTEM_USBEXTREME].nGames=0;
	return 0; /* Failure loading UL.cfg */
 }

 if(GameLists[SYSTEM_USBEXTREME].nGames>0){
	fioClose(game_list_fd);
	return 1; /* If a USBExtreme game list has been loaded before */
 }

 list_size=fioLseek(game_list_fd,0,SEEK_END);

 GameLists[SYSTEM_USBEXTREME].nGames=list_size/sizeof(struct USBExtreme_cfg_entry);
 GameLists[SYSTEM_USBEXTREME].old_list_bottom_ID=GameLists[SYSTEM_USBEXTREME].nGames-1;	/* Initialize the ID of the last entry visible entry of the list on the screen. */
 GameEntries=malloc(list_size);

 if(list_size>0){
   GameLists[SYSTEM_USBEXTREME].GameList=malloc(GameLists[SYSTEM_USBEXTREME].nGames*sizeof(struct PS2ESDL_config_entry));
   fioLseek(game_list_fd,0,SEEK_SET);
   fioRead(game_list_fd, GameEntries, list_size);
 }
 fioClose(game_list_fd);

 /* Convert the USBExtreme game list into the general format. */
 for(i=0; i<GameLists[SYSTEM_USBEXTREME].nGames; i++){
     	memset(&GameLists[SYSTEM_USBEXTREME].GameList[i], 0, sizeof(GameLists[SYSTEM_USBEXTREME].GameList[i]));
	strncpy(GameLists[SYSTEM_USBEXTREME].GameList[i].title, GameEntries[i].title, sizeof(GameEntries[i].title));
	GameLists[SYSTEM_USBEXTREME].GameList[i].disctype=GameEntries[i].disc_type;
	GameLists[SYSTEM_USBEXTREME].GameList[i].flags=0;
	GameLists[SYSTEM_USBEXTREME].GameList[i].cache_sz=2;
	memcpy(GameLists[SYSTEM_USBEXTREME].GameList[i].ELF_fname, &GameEntries[i].ELF_fname[3], 11);
	memset(GameLists[SYSTEM_USBEXTREME].GameList[i].cdvd_dnas_key, 0, 5);
 }

 free(GameEntries);

 DEBUG_PRINTF("Loaded USBExtreme games. Games loaded: %lu.\n", GameLists[SYSTEM_USBEXTREME].nGames);

 return 1;
}
