/* PS2ESDL Disc Image format version 1.22 */
#define MAX_TITLE_LENGTH	80		/* 80-bytes maximum (Sufficient for 40 souble-byte characters). */

struct PS2ESDL_config_entry{
 unsigned char title[80];		/* Title of the game that will be displayed. */
 unsigned char disctype;		/* Currently whether it's a PS2DVD or PS2CD */
 unsigned char flags;			/* Optional flags for PS2ESDL to use. */
 unsigned char cache_sz;		/* Read buffer size in 2048-byte sectors */
 unsigned char ELF_fname[11];		/* Name of main ELF on the game's disc. e.g.: SLXX_999.99 */

// unsigned long int crc32;		/* Checksum of the entire game. */
 unsigned char crc32[4];		/* !!Bug!! With alignment, the entire game list isn't being parsed properly on the PS2. :(
						Anyway, the checksum isn't important for PS2ESDL on the PS2. XD */
 unsigned char cdvd_dnas_key[5];	/* DNAS disc key */
};

#define PS2ESDL_FMT_VERSION 0x122

struct PS2ESDL_conf_hdr{
	unsigned char signature[4];	/* "PDI" + a padding field (A 0x00 byte). */
	unsigned short int version;
};
