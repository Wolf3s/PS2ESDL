// Single byte fonts have only a single table whose offset starts after type.
typedef struct {
	char id[6];						// "FONTX2" id Identifier
	char name[8];					// Name of the font
	unsigned char width;			// Font Width XSize
	unsigned char height;			// Font Height YSize
	unsigned char type;				// Type of Font
									// Single-byte font headers end here
	unsigned char table_num;		// Number of tables
	struct {
			unsigned short start;	// Table[n] starting position
			unsigned short end;		// Table[n] ending position
	} block[];
} fontx_hdr;

typedef struct {
	char name[9];	// Name of font
	char bold;		// Enable/Disable bold effect
	char w_margin;	// Margin width between characters
	char h_margin;	// Margin height between lines
	int rowsize;	// Size in byte of each row
	int charsize;	// Size in bytes of each character
	int offset;		// Offset from beginning of font to character
	char *font;		// The font data
} FONTX;

#define SINGLE_BYTE 0
#define DOUBLE_BYTE 1

#ifndef LEFT_ALIGN
#define LEFT_ALIGN 0
#endif

#ifndef CENTER_ALIGN
#define CENTER_ALIGN 1
#endif

#ifndef RIGHT_ALIGN
#define RIGHT_ALIGN 2
#endif

int gsKit_init_krom(GSGLOBAL *gsGlobal);
void gsKit_unload_krom(void);
void gsKit_krom_print_scaled(GSGLOBAL *gsGlobal, float X, float Y, int Z, float scale, u64 colour, const unsigned char *str);
