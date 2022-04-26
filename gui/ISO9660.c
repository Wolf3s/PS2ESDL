#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <fileio.h>

#include <dmaKit.h>
#include <gsKit.h>
#include <gsToolkit.h>

#include "include/PS2ESDL_format.h"
#include "include/GUI.h"
#include "include/system.h"

extern struct GameList GameLists[SUPPORTED_GAME_FORMATS];

struct iso9660_GameListConstructionEntry{
	struct iso9660_GameListConstructionEntry *next;
	char title[MAX_TITLE_LENGTH];	/* !!!Note!! The maximum possible length of a title would be 255 characters MAX - 11 (E.g. SLXX-99999) + 1 NULL terminator. */
					/* However, limit it to 40 double-bytes for all formats. */
	char ELF_fname[11];		/* Name of main ELF on the game's disc. e.g.: SLXX_999.99 */
};

static int load_ISO9600_list(const char *path, struct GameList *GameList, unsigned char DiscType);

int load_ISO9600_DVD_disc_image_list(const char *path){
	char ISOPath[128];

	sprintf(ISOPath, "%sDVD/", path);
	return(load_ISO9600_list(ISOPath, &GameLists[SYSTEM_ISO9660_DVD], SCECdPS2DVD));
}

int load_ISO9600_CD_disc_image_list(const char *path){
	char ISOPath[128];

	sprintf(ISOPath, "%sCD/", path);
	return(load_ISO9600_list(ISOPath, &GameLists[SYSTEM_ISO9660_CD], SCECdPS2CD));
}

static int load_ISO9600_list(const char *path, struct GameList *GameList, unsigned char DiscType){
	fio_dirent_t FileInfo;
	unsigned int nGames, i;
	int fd;
	struct iso9660_GameListConstructionEntry *FirstEntry;
	struct iso9660_GameListConstructionEntry *CurrentEntry;
	void *temp;

	if((fd=fioDopen(path))<0) return 0;

	FirstEntry=CurrentEntry=malloc(sizeof(struct iso9660_GameListConstructionEntry));
	nGames=0;

	while(fioDread(fd, &FileInfo)>0){
		/* Ignore the current directory entry, the parent directory entry, and only add ISO9660 disc images to the list. */
		if((strcmp(FileInfo.name, ".")==0)||(strcmp(FileInfo.name, "..")==0)||(strcmp(&FileInfo.name[strlen(FileInfo.name)-4], ".iso")!=0)) continue;

		memset(CurrentEntry, 0, sizeof(struct iso9660_GameListConstructionEntry));
		strncpy(CurrentEntry->title, &FileInfo.name[12], strlen(FileInfo.name)-16); /* strlen(FileInfo.name) - length of the name of the game's main executable - the length of the file's extension (Inclusive of the '.' seperator) */
		memcpy(CurrentEntry->ELF_fname, FileInfo.name, 11);

		nGames++;

		CurrentEntry->next=malloc(sizeof(struct iso9660_GameListConstructionEntry));
		CurrentEntry=CurrentEntry->next;
	}
	fioDclose(fd);

	if(nGames>0){
		CurrentEntry->next=NULL;

		GameList->GameList=malloc(nGames*sizeof(struct PS2ESDL_config_entry));

		CurrentEntry=FirstEntry;

		for(i=0; i<nGames; i++){
			memset(&GameList->GameList[i], 0, sizeof(struct PS2ESDL_config_entry));
			strncpy(GameList->GameList[i].title, CurrentEntry->title, MAX_TITLE_LENGTH);
			memcpy(GameList->GameList[i].ELF_fname, CurrentEntry->ELF_fname, 11);
			GameList->GameList[i].disctype=DiscType;

			temp=CurrentEntry->next;
			free(CurrentEntry);
			CurrentEntry=temp;
		}

		free(CurrentEntry);
	}

	GameList->nGames=nGames;
	GameList->old_list_bottom_ID=nGames-1;	/* Initialize the ID of the last entry visible entry of the list on the screen. */

	DEBUG_PRINTF("Loaded ISO9660 games. Games loaded: %lu.\n", GameList->nGames);

	return 1;
}
