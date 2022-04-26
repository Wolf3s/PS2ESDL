#define PS2ESDL_SKIN_VER	0x100

struct SkinParaHeader{
	unsigned char signature[4];	/* "PLSF" */
	unsigned short version;
};

struct SkinParameters{
	unsigned int UIDispX, UIDispY;
	unsigned int ScrollBarX, ScrollBarY;

	unsigned int GameListTypeDispX, GameListTypeDispY;
	float GameListTypeDispMag;
	unsigned int GameListHighlightLen;

	unsigned int VersionDispX, VersionDispY;
	float VersionDispMag;

	unsigned int ScrollBarLen;
	unsigned char GameEntryLength;
	unsigned char HideVerNumInGameList;
	unsigned char nGamesToList;
};

/* Display data. */
#define UI_RESOURCE_SKIN	0x01
#define UI_RESOURCE_BKGND	0x02

struct ScreenDisplayData{
	unsigned char CurrentlySelectedGameListType;
	unsigned char ResourcesLoaded;
	unsigned char SourceDevice;

	unsigned short int displayLength;
	unsigned short int displayHeight;

	unsigned long int max_displayed_games;

	struct SkinParameters SkinParameters;
};

/* Function prototypes */
int LoadSkin(char* path, struct ScreenDisplayData* DisplayData);


