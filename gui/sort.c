#include <string.h>
#include <stdlib.h>

#include "include/PS2ESDL_format.h"
#include "include/sort.h"

static int GameEntryComparator(const void *GameEntry1, const void *GameEntry2){
	return strncmp(((struct PS2ESDL_config_entry *)GameEntry1)->title, ((struct PS2ESDL_config_entry *)GameEntry2)->title, MAX_TITLE_LENGTH);
}

void SortGameList(struct PS2ESDL_config_entry* GameList, unsigned int nGames){
	qsort(GameList, nGames, sizeof(struct PS2ESDL_config_entry), &GameEntryComparator);
}
