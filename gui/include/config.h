#define PS2ESDL_CONFIG_VERSION 0x102

struct PS2ESDLConfiguration{
	unsigned char signature[4]; /* PCF +  1 padding byte */
	unsigned short int version;

	/* Screen positioning correction. */
	unsigned short int X_offset;
	unsigned short int Y_offset;

	unsigned short int displayMode;

	/* Other options. */
	/* DisableFragCheck		-> Contains whether the game disc image file fragmentation check is run whenever a game is launched.
	 * DisableVideoModeOverride	-> Whether the video mode override function should be disabled.
	 * EnableTitleSorting		-> Whether to sort the game titles alphabetically or not.
	 * EnablePS2LOGO		-> Whether to running rom0:PS2LOGO or not.
	*/
	unsigned long int Services;

	unsigned short int selectButton;	/* The button used to select options on the configuration/options screen. */
	unsigned short int cancelButton;	/* The button used to abort/cancel/go back at the configuration/options screen. */
};

/* Services */
#define DISABLE_FRAG_CHECK		0x00000001
#define DISABLE_VIDEO_MODE_OVERRIDE	0x00000002
#define ENABLE_TITLE_SORTING		0x00000004
#define ENABLE_PS2LOGO			0x00000008

#define MENU_TYPE_SUBMENU	0x00
#define MENU_TYPE_SELECTOR	0x01
#define MENU_TYPE_EXIT	0xFF

struct MenuOption{
	char title[32];
	unsigned char menuType;
	void *optionData;
};

#define FONT_CHAR_WIDTH 12
#define FONT_SCALE	0.4f

/* Function prototypes */
int configurePS2ESDL(GSTEXTURE *background);
void setScreenRes(GSGLOBAL *gsGlobal);
void setScreenPos(GSGLOBAL *gsGlobal, int X_Offset, int Y_Offset);
void ApplyVideoOptions(void);
int LoadConfiguration(void);
inline int SaveConfiguration(void);
