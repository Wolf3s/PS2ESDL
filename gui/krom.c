#include <kernel.h>
#include <stdio.h>

#include <dmaKit.h>
#include <gsKit.h>
#include <gsInline.h>

#include "include/krom.h"

static FONTX krom_ascii;
static FONTX krom_kanji;

static unsigned char *get_fontx_char(FONTX* fontx, unsigned short character);
static void draw_fontx_char(GSGLOBAL *gsGlobal, unsigned short character, float X, float Y, int Z, float scale, u64 colour, FONTX *fontx_data);
static inline void draw_fontx_row(GSGLOBAL *gsGlobal, unsigned char bitmap, int x, int y, int z, u64 colour, int bold);

/* For loading a FONTX2 file. */
static int load_krom_single(FONTX *fontx);
static int load_krom_double(FONTX *fontx);
static int load_fontx(const char *path, FONTX* fontx, int type, int wmargin, int hmargin, int bold);

int gsKit_init_krom(GSGLOBAL *gsGlobal){
	if(load_fontx("rom0:KROM", &krom_ascii, SINGLE_BYTE, 2, 1, 1)<0) goto krom_load_failure;
	if(load_fontx("rom0:KROM", &krom_kanji, DOUBLE_BYTE, 2, 1, 1)<0) goto krom_load_failure;

	return 0;

krom_load_failure:
	return -1;
}

void gsKit_unload_krom(void){

}

void gsKit_krom_print_scaled(GSGLOBAL *gsGlobal, float X, float Y, int Z, float scale, u64 colour, const unsigned char *str){
	int i,j;
	int alignment=LEFT_ALIGN;

	fontx_hdr *ascii_header = (fontx_hdr *)(krom_ascii.font);
	fontx_hdr *kanji_header = (fontx_hdr *)(krom_kanji.font);

	int length = strlen(str);

	int line = 0;
	short halfwidth[100];
	short fullwidth[100];
	float x_orig[100];

	unsigned short wide;

	float hw = ascii_header->width;

	float fw = kanji_header->width;
	float h = kanji_header->height;

	float wm = krom_kanji.w_margin;
	float hm = krom_kanji.h_margin;

	// line_num is used to keep track of number of characters per line
	halfwidth[0] = 0;
	fullwidth[0] = 0;

	switch (alignment)
	{
		default:
		case LEFT_ALIGN:
		{
			for (i = 0; i < length; i++)
			{

				while (str[i] == '\t'|| str[i] == '\n')
				{
					if (str[i] == '\t')
					{
						halfwidth[line] += 4;
					}
					if (str[i] == '\n')
					{
						x_orig[line] = X;
						line++;
						halfwidth[line] = 0;
						fullwidth[line] = 0;
					}
					i++;
				}


				if (i == length-1)
				{
					x_orig[line] = X;
					line++;
				}

			}
			break;

		}
		case RIGHT_ALIGN:
		{
			for (i = 0; i < length; i++)
			{

				while (str[i] == '\t'|| str[i] == '\n')
				{
					if (str[i] == '\t')
					{
						halfwidth[line] += 4;
					}
					if (str[i] == '\n')
					{
						x_orig[line] = X - ((halfwidth[line] * (hw+wm)) + (fullwidth[line] * (fw + wm)));;
						line++;
						halfwidth[line] = 0;
						fullwidth[line] = 0;
					}
					i++;
				}

				if (str[i] < 0x80)
				{
					halfwidth[line]++;
				}
				else if (str[i] >= 0xA1 && str[i] <= 0xDF)
				{
					halfwidth[line]++;
				}
				else if (str[i] >= 0x81 && str[i] <= 0x9F)
				{
					fullwidth[line]++;
				}
				else if (str[i] >= 0xE0 && str[i] <= 0xEF)
				{
					fullwidth[line]++;
				}

				if (i == length-1)
				{
					x_orig[line] = X - ((halfwidth[line] * (hw+wm)) + (fullwidth[line] * (fw + wm)));
					line++;
				}

			}
			break;

		}
		case CENTER_ALIGN:
		{
			for (i = 0; i < length; i++)
			{

				while (str[i] == '\t'|| str[i] == '\n')
				{
					if (str[i] == '\t')
					{
						halfwidth[line] += 4;
					}
					if (str[i] == '\n')
					{
						x_orig[line] = X - ((halfwidth[line] * (hw+wm)) + (fullwidth[line] * (fw + wm)))/2.0f;
						line++;
						halfwidth[line] = 0;
						fullwidth[line] = 0;
					}
					i++;
				}

				if (str[i] < 0x80)
				{
					halfwidth[line]++;
				}
				else if (str[i] >= 0xA1 && str[i] <= 0xDF)
				{
					halfwidth[line]++;
				}
				else if (str[i] >= 0x81 && str[i] <= 0x9F)
				{
					fullwidth[line]++;
				}
				else if (str[i] >= 0xE0 && str[i] <= 0xEF)
				{
					fullwidth[line]++;
				}

				if (i == length-1)
				{
					x_orig[line] = X - ((halfwidth[line] * (hw+wm)) + (fullwidth[line] * (fw + wm)))/2.0f;
					line++;
				}

			}
			break;

		}

	};

	line = 0;
	X = x_orig[0];

	for (j = 0; j < length; j++)
	{

		wide = 0;

		while(str[j] == '\n' || str[j] == '\t')
		{
			if (str[j] == '\n')
			{
				line++;
				Y += h + hm;
				X = x_orig[line];
			}
			if (str[j] == '\t')
			{
				X += hw * 5.0f;
			}
			j++;
		}

		if (str[j] < 0x80)
		{
			draw_fontx_char(gsGlobal, str[j], X, Y, Z, scale, colour, &krom_ascii);
			X += hw + wm;
		}
		else if (str[j] >= 0xA1 && str[j] <= 0xDF)
		{
			draw_fontx_char(gsGlobal, str[j], X, Y, Z, scale, colour, &krom_ascii);
			X += hw + wm;
		}
		else if (str[j] >= 0x81 && str[j] <= 0x9F)
		{
			wide = str[j++]<<8;
			wide += str[j];
			draw_fontx_char(gsGlobal, wide, X, Y, Z, scale, colour, &krom_kanji);
			X += fw + wm;
		}
		else if (str[j] >= 0xE0 && str[j] <= 0xEF)
		{
			wide = str[j++]<<8;
			wide += str[j];
			draw_fontx_char(gsGlobal, wide, X, Y, Z, scale, colour, &krom_kanji);
			X += fw + wm;
		}
		else
		{
			X += fw + wm;
		}

	}
}

static unsigned char *get_fontx_char(FONTX* fontx, unsigned short character)
{
	unsigned int i;
	unsigned char *offset;

	int table = -1;
	int table_offset = 0;

	fontx_hdr *fontx_header=(fontx_hdr*)fontx->font;

	if ((fontx_header->type == SINGLE_BYTE) && (character <= 0xFF))
	{
		offset=fontx->font + (fontx->offset + character * fontx->charsize);
		goto get_fontx_char_end;
	}

	for (i = 0; i < fontx_header->table_num; i++)
	{
		if ((fontx_header->block[i].start <= character) && (fontx_header->block[i].end >= character))
		{
				table = i;
				break;
		}
	}

	// If table not found
	if (table < 0) return NULL;

	for (i = 0; i < table; i++)
	{

		table_offset += fontx_header->block[i].end - fontx_header->block[i].start;

	}

	table_offset = table_offset + table + (character - fontx_header->block[table].start);
	offset=fontx->font + (fontx->offset + table_offset*fontx->charsize);

get_fontx_char_end:
	return(offset);
}

/* Draws a single character. */
static void draw_fontx_char(GSGLOBAL *gsGlobal, unsigned short character, float X, float Y, int Z, float scale, u64 colour, FONTX *fontx_data)
{
	int i, j;
	unsigned char *char_offset;

	fontx_hdr* fontx_header = (fontx_hdr*)fontx_data->font;

	char_offset=get_fontx_char(fontx_data, character);

	if(!char_offset) return;

	for (i=0;i<fontx_header->height;i++)
	{
		for (j=0;j < fontx_data->rowsize;j++)
		{
			/* Draw each row of 8px (The current font character might not be 8px long.... but multiples of it). */
			draw_fontx_row(gsGlobal, char_offset[(i*fontx_data->rowsize) + j], X + j*8, Y+i, Z, colour, fontx_data->bold);
		}
	}
}

static inline void draw_fontx_row(GSGLOBAL *gsGlobal, unsigned char bitmap, int x, int y, int z, u64 colour, int bold)
{
	unsigned char mask = 0x80;

	int i;
	int px = 0;

	/* For each bit in a row. */
	for (i=0;i<8;i++)
	{
		/* If the current bit is set. */
		if (bitmap & mask)
		{
			gsKit_prim_point(gsGlobal, x+i, y, z, colour);
			px = 1;
		}
		else
		{
			if (bold && px) gsKit_prim_point(gsGlobal, x+i, y, z, GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00));
			px = 0;
		}

		mask >>= 1;
	}

	/* Add some boldness to the last pixel in a row. */
	if (bold && px)
	{
		gsKit_prim_point(gsGlobal, x+i, y, z, GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00));
	}
}

/* For loading a FONTX2 font. */
/* These are the SJIS table ranges for character lookup */
static unsigned short sjis_table[] = {
0x8140,0x817e,
0x8180,0x81ac,
0x81b8,0x81bf,
0x81c8,0x81ce,
0x81da,0x81e8,
0x81f0,0x81f7,
0x81fc,0x81fc,
0x824f,0x8258,
0x8260,0x8279,
0x8281,0x829a,
0x829f,0x82f1,
0x8340,0x837e,
0x8380,0x8396,
0x839f,0x83b6,
0x83bf,0x83d6,
0x8440,0x8460,
0x8470,0x847e,
0x8480,0x8491,
0x849f,0x84be,
0x889f,0x88fc,
0x8940,0x897e,
0x8980,0x89fc,
0x8a40,0x8a7e,
0x8a80,0x8afc,
0x8b40,0x8b7e,
0x8b80,0x8bfc,
0x8c40,0x8c7e,
0x8c80,0x8cfc,
0x8d40,0x8d7e,
0x8d80,0x8dfc,
0x8e40,0x8e7e,
0x8e80,0x8efc,
0x8f40,0x8f7e,
0x8f80,0x8ffc,
0x9040,0x907e,
0x9080,0x90fc,
0x9140,0x917e,
0x9180,0x91fc,
0x9240,0x927e,
0x9280,0x92fc,
0x9340,0x937e,
0x9380,0x93fc,
0x9440,0x947e,
0x9480,0x94fc,
0x9540,0x957e,
0x9580,0x95fc,
0x9640,0x967e,
0x9680,0x96fc,
0x9740,0x977e,
0x9780,0x97fc,
0x9840,0x9872
};

static int load_krom_single(FONTX *fontx)
{
	fontx_hdr *fontx_header;

	int header_size = 17;
	int char_size = 15;

	int fd = 0;
	int size;

	if((fd = fioOpen("rom0:KROM", O_RDONLY))<0)
	{
		printf("Error opening KROM font.\n");
		return -1;
	}

	// header without table pointer + size of a character * 256 characters
	size = header_size + char_size * 256;

	if((fontx->font = malloc(size))==NULL)
	{

		printf("Error allocating %d bytes of memory.\n", size);
		fioClose(fd);

		return -1;
	}

	// Clear the memory
	memset(fontx->font,0,size);

	// The offset for the ASCII characters
	fioLseek(fd, 0x198DE, SEEK_SET);

	// 17 bytes of header and 15 bytes per 33 characters
	// Read in 95 characters
	if(fioRead(fd,fontx->font + header_size+char_size*33, char_size*95) < 0)
	{

		printf("Error reading rom0:KROM.\n");

		free(fontx->font);
		fioClose(fd);

		return -1;
	}

	fioClose(fd);

	fontx_header = (fontx_hdr*)fontx->font;

	// define header as single-byte font
	strncpy(fontx_header->id, "FONTX2", 6);
	fontx_header->id[6] = '\0';
	strncpy(fontx_header->name, "KROM", 8);
	fontx_header->name[8] = '\0';

	fontx_header->width = 8;
	fontx_header->height = 15;
	fontx_header->type = SINGLE_BYTE;

	/* Save it as a FONTX2 file. */
	/* fd=fioOpen("host:KROM_ascii.fnt", O_WRONLY|O_TRUNC|O_CREAT);
	fioWrite(fd, fontx->font, size);
	fioClose(fd); */

	return 0;

}

static int load_krom_double(FONTX *fontx)
{

	fontx_hdr *fontx_header;

	int size;
	int fd = 0;

	// Font characteristics for double-byte font
	int header_size = 18;
	int table_num = 51;
	int table_size = 4;
	int char_size = 30;
	int char_num = 3489;

	if((fd = fioOpen("rom0:KROM", O_RDONLY))<0)
	{

		printf("Error opening KROM font.\n");

	}

	size = header_size + table_num*table_size +  char_num*char_size;

	if((fontx->font = (char*)malloc(size))==NULL)
	{
		printf("Error allocating memory.\n");
		fioClose(fd);

		return -1;
	}

	// Clear memory
	memset(fontx->font,0,size);

	// Make sure we're at the beginning
	fioLseek(fd, 0, SEEK_SET);

	// Read in 95 characters
	if(fioRead(fd, fontx->font+header_size+table_num*table_size, char_size*char_num)<0)
	{

		printf("Error reading font.\n");
		free(fontx->font);
		fioClose(fd);

		return -1;
	}

	fioClose(fd);

	fontx_header = (fontx_hdr*)fontx->font;

	// define the header as double-byte font
	strncpy(fontx_header->id, "FONTX2", 6);
	fontx_header->id[6] = '\0';
	strncpy(fontx_header->name, "KROM", 8);
	fontx_header->name[8] = '\0';

	fontx_header->width = 16;
	fontx_header->height = 15;
	fontx_header->type = DOUBLE_BYTE;
	fontx_header->table_num = table_num;

	// Add the SJIS tables to the font
	memcpy(fontx->font+header_size, sjis_table, table_num*table_size);

	// Save it as a font
	//fd=fioOpen("host:KROM_kanji.fnt",O_WRONLY | O_TRUNC | O_CREAT);
	//fioWrite(fd, fontx->font, size);
	//fioClose(fd);

	return 0;

}

static int load_fontx(const char *path, FONTX* fontx, int type, int wmargin, int hmargin, int bold)
{

	int fd;

	int ret = -1;
	long size = 0;

	fontx_hdr *fontx_header = NULL;

	if (!strcmp("rom0:KROM",path) || !strcmp("rom0:/KROM",path))
	{

		ret=(type==SINGLE_BYTE)?load_krom_single(fontx):load_krom_double(fontx);

		if (ret < 0)
		{

			printf("Error opening %s\n", path);
			return -1;

		}

	}

	else
	{

		if((fd = fioOpen(path, O_RDONLY))<0)
		{
			printf("Error opening %s\n", path);
			return -1;

		}

		// get size of file
		size=fioLseek(fd, 0, SEEK_END);
		fioLseek(fd, 0, SEEK_SET);

		if((fontx->font = (char *)malloc(size))==NULL)
		{
			printf("Error allocating %ld bytes of memory.\n", size);
			fioClose(fd);
			return -1;
		}

		fioRead(fd, fontx->font, size);

		fioClose(fd);

	}

	fontx_header = (fontx_hdr*)fontx->font;

	if (strncmp(fontx_header->id, "FONTX2", 6) != 0)
	{

		printf("Not FONTX2 type font!\n");
		free(fontx->font);

		return -1;

	}

	if (fontx_header->type != type)
	{

		printf("Type mismatch\n");
		free(fontx->font);

		return -1;

	}

	// Fill in some information about the font
	strcpy(fontx->name,fontx_header->name);

	fontx->rowsize = ((fontx_header->width+7)>>3);
	fontx->charsize = fontx->rowsize * fontx_header->height;
	fontx->w_margin = wmargin;
	fontx->h_margin = hmargin;
	fontx->bold = bold;

	// This is the offset that character data starts
	if (fontx_header->type == SINGLE_BYTE)
	{
		fontx->offset = 17;
	}
	else
	{
		// 17 + 1 + (number of tables * 4) bytes
		fontx->offset = 18 + (fontx_header->table_num * 4);
	}

	return 0;
}
