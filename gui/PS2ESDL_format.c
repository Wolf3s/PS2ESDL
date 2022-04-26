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

static struct PS2ESDL_conf_hdr dummy_PS2ESDL_conf_hdr={
        "PDI",
        PS2ESDL_FMT_VERSION,
};

int load_PS2ESDL_list(const char *path){
 unsigned long int list_size;
 static struct PS2ESDL_conf_hdr sample_PS2ESDL_conf_hdr;
 int game_list_fd;
 char CfgPath[64];

 sprintf(CfgPath, "%sPS2ESDL.cfg", path);

 if((game_list_fd=fioOpen(CfgPath, O_RDONLY))<0){
        GameLists[SYSTEM_PS2ESDL].nGames=0;
        return 0; /* Failure loading PS2ESDL.cfg */
 }

 if(GameLists[SYSTEM_PS2ESDL].nGames>0){
        fioClose(game_list_fd);
        return 1; /* If a valid PS2ESDL game list has been loaded before */
 }

 fioRead(game_list_fd, &sample_PS2ESDL_conf_hdr, sizeof(struct PS2ESDL_conf_hdr));

 /* Verify the PS2ESDL configuration file's version. */
 if(memcmp(&dummy_PS2ESDL_conf_hdr, &sample_PS2ESDL_conf_hdr, sizeof(struct PS2ESDL_conf_hdr))!=0){
	DEBUG_PRINTF("Invalid PS2ESDL configuration file\n");

	GameLists[SYSTEM_PS2ESDL].nGames=0;
	fioClose(game_list_fd);
	return 0; /* If the PS2ESDL configuration file is invalid/unsupported. */
 }

 list_size=fioLseek(game_list_fd,0,SEEK_END)-sizeof(struct PS2ESDL_conf_hdr);

 GameLists[SYSTEM_PS2ESDL].nGames=list_size/sizeof(struct PS2ESDL_config_entry);
 GameLists[SYSTEM_PS2ESDL].old_list_bottom_ID=GameLists[SYSTEM_PS2ESDL].nGames-1;	/* Initialize the ID of the last entry visible entry of the list on the screen. */

 if(list_size>0){
   GameLists[SYSTEM_PS2ESDL].GameList=malloc(list_size);
   fioLseek(game_list_fd, sizeof(struct PS2ESDL_conf_hdr), SEEK_SET);
   fioRead(game_list_fd, GameLists[SYSTEM_PS2ESDL].GameList, list_size);
   fioClose(game_list_fd);
 }

 DEBUG_PRINTF("Loaded PS2ESDL games. Games loaded: %lu.\n", GameLists[SYSTEM_PS2ESDL].nGames);

 return 1;
}
