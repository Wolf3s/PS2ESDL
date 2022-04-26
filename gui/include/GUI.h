//#define DEBUG_TTY_FEEDBACK /* Comment out to disable generation of debug TTY messages */

#ifdef DEBUG_TTY_FEEDBACK
 #define DEBUG_PRINTF(args...) printf(args)
#else
 #define DEBUG_PRINTF(args...)
#endif

#define PAD_SLOT 0
#define PAD_PORT 0

#define PS2ESDL_VERSION	"version 0.825"

#define UI_VISIBLE_SCREEN_LENGTH	640
#define UI_VISIBLE_SCREEN_HEIGHT	448

#define PAD_POLL_DELAY				4000000000	/* The amount of delay between polling pad states. */
#define PAD_POLL_IN_BETWEEN_KEY_PRESSES_DELAY	20000000	/* The amount of delay between polling pad states, when a D-pad button is held. */
#define PAD_POLL_DELAY_MS			175		/* The amount of delay between polling pad states, when a D-pad button is held. */

#define SUPPORTED_GAME_FORMATS 4

#define SYSTEM_PS2ESDL		0
#define SYSTEM_USBEXTREME	1
#define SYSTEM_ISO9660_CD	2
#define SYSTEM_ISO9660_DVD	3
#define SYSTEM_INVALID		0xFF

#define GAMES_LIST_MAX		17	/* maximun number to list on the screen at 1 time */
#define SCROLL_BAR_LENGTH	303	/* The maximum length of the scroll bar, in pixels. */
#define GAME_ENTRY_UNIT_LENGTH	10	/* The length of the scrollbar that the scrollbar should be shortened by to represent each game that isn't listed on the screen. */

/* Game list structures. */
struct GameList{
	struct PS2ESDL_config_entry *GameList;
	char	label[32];
	unsigned long int nGames;
	unsigned long int CurrentlySelectedIndexInList;
	unsigned long int CurrentlySelectedIndexOnScreen;
	unsigned long int old_list_bottom_ID;
	unsigned char GameListType;
};

/* Function prototypes */
/* graphics.c */
void messageBox(GSTEXTURE *mbox, const char *message, const char *title);
int UploadTexture(GSGLOBAL *gsGlobal, GSTEXTURE* Texture);
int load_embeddedGraphics(GSGLOBAL *gsGlobal, GSTEXTURE *Texture, void *buffer, unsigned int graphic_sz);
void redrawGameListScreenBackground(struct GameList *ActiveGameList, unsigned char current_sys_type, GSTEXTURE *background);
void update_screen(void);
void drawBackground(GSTEXTURE *background);

/* PS2ESDL_format.c */
int load_PS2ESDL_list(const char *path);

/* USBExtreme_format.c */
int load_USBExtreme_list(const char *path);

/* ISO9660.c */
int load_ISO9600_CD_disc_image_list(const char *path);
int load_ISO9600_DVD_disc_image_list(const char *path);
