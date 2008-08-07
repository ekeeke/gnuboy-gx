/*
 * Gnuboy ROM Loader Support for the Nintendo Gamecube
 * Original Code by Laguna & Gilgamesh - Modified by Eke-Eke 2007
 * This file may be distributed under the terms of the GNU GPL.
 */

#include "defs.h"
#include "regs.h"
#include "mem.h"
#include "hw.h"
#include "rtc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *gbrom;

static int mbc_table[256] =
{
	0, 1, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 3,
	3, 3, 3, 3, 0, 0, 0, 0, 0, 5, 5, 5, MBC_RUMBLE, MBC_RUMBLE, MBC_RUMBLE, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MBC_HUC3, MBC_HUC1
};

static int rtc_table[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static int batt_table[256] =
{
	0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0,
	1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
	0
};

static int romsize_table[256] =
{
	2, 4, 8, 16, 32, 64, 128, 256, 512,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 128, 128, 128
	/* 0, 0, 72, 80, 96  -- actual values but bad to use these! */
};

static int ramsize_table[256] =
{
	1, 1, 1, 4, 16,
	4 /* FIXME - what value should this be?! */
};

s8 autoload = -1;  /* SRAM Auto-load feature */
u8 forcedmg = 0;  /* Force Mono Gameboy  */
u8 gbamode  = 1;  /* enables GBA-only features present in some GBC games */
int memfill = 0;
int memrand = -1;

static void initmem(void *mem, int size)
{
	char *p = mem;
	if (memrand >= 0)
	{
		srand(memrand ? memrand : time(0));
		while(size--) *(p++) = rand();
	}
	else if (memfill >= 0)
		memset(p, memfill, size);
}

int rom_load()
{
	byte c, *data, *header;
	
	data = header = (byte *)gbrom;
	
	memcpy(rom.name, header+0x0134, 16);
	if (rom.name[14] & 0x80) rom.name[14] = 0;
	if (rom.name[15] & 0x80) rom.name[15] = 0;
	rom.name[16] = 0;

	c = header[0x0147];
	mbc.type = mbc_table[c];
	mbc.batt = batt_table[c];
	rtc.batt = rtc_table[c];
	mbc.romsize = romsize_table[header[0x0148]];
	mbc.ramsize = ramsize_table[header[0x0149]];

	if (!mbc.romsize) return 0;
	if (!mbc.ramsize) return 0;

	rom.bank  = (void *)data;
	ram.sbank = malloc(8192 * mbc.ramsize);

	initmem(ram.sbank, 8192 * mbc.ramsize);
	initmem(ram.ibank, 4096 * 8);

	mbc.rombank = 1;
	mbc.rambank = 0;

	c = header[0x0143];
	hw.cgb = ((c == 0x80) || (c == 0xc0)) && !forcedmg;
	hw.gba = (hw.cgb && gbamode);

	return 1;
}

void loader_unload()
{
  if (ram.sbank) free(ram.sbank);
	rom.bank = NULL;
	ram.sbank = NULL;
	mbc.type = mbc.romsize = mbc.ramsize = mbc.batt = 0;
}

void sram_load(u8 *srambuf)
{
	int pos = 0;
	unsigned long inbytes, outbytes;
	unsigned char *sram;
	
	/* get compressed state size */
	memcpy(&inbytes,&srambuf[0],sizeof(inbytes));

	/* uncompress state file */
	outbytes = 0;
	if (mbc.batt) outbytes += 8192*mbc.ramsize;
	if (rtc.batt) outbytes += 32;
	sram = (unsigned char *)malloc(outbytes);
	uncompress ((Bytef *) &sram[0], &outbytes, (Bytef *) &srambuf[sizeof(inbytes)], inbytes);

	if (mbc.batt)
	{
		memcpy(ram.sbank, sram, 8192*mbc.ramsize);
		pos += (8192*mbc.ramsize);
	}

	/* get RTC if it exists */
	if (rtc.batt) rtc_load_internal(&sram[pos]);

	free(sram);
	emu_reset();
}

int sram_save(u8 *srambuf)
{
	int pos = 0;
	unsigned long inbytes, outbytes;
	unsigned char *sram;

	inbytes = 0;
	if (mbc.batt) inbytes += 8192*mbc.ramsize;
	if (rtc.batt) inbytes += 32;
	sram = (unsigned char *)malloc(inbytes);
	
	if (mbc.batt)
	{
		memcpy(sram, ram.sbank, 8192*mbc.ramsize);
		pos += (8192*mbc.ramsize);
	}

	/* include RTC if it exists */
	if (rtc.batt)
	{
		rtc_save_internal(&sram[pos]);
		pos += 32;
	}

	/* compress state file */
	outbytes = 0x30000;
	compress2 ((Bytef *) &srambuf[sizeof(outbytes)], &outbytes, (Bytef *) &sram[0], inbytes, 9);
	free(sram);

	/* write compressed size in the first 32 bits for decompression */
	memcpy(&srambuf[0], &outbytes, sizeof(outbytes));

	/* return total size */
	return (sizeof(outbytes) + outbytes);
}
