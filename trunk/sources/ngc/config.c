/******************************************************************************
 *  Gnuboy Gamecube port
 *  Eke-Eke (@tehskeen.com)
 *  Additional code & support by Askot
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ***************************************************************************/
#include "defs.h"
#include "config.h"
#include "font.h"

extern u8 usefilter;    /* Apply Display Filter */
extern u8 filterdmg;    /* Apply Filter also for GameBoy MONO */
extern u8 syncrtc;      /* Synchronize in-game RTC on SRAM load with your own Console RTC  */
extern u8 forcedmg;     /* Force Mono Gameboy  */
extern u8 gbamode;      /* enables GBA-only features present in some GBC games */
extern u8 paletteindex; /* Index of selected palette color*/
extern s16 square[];
extern void draw_init();
extern int use_FAT;

ConfigOptions config;

void config_save()
{
  if (!fat_enabled) return;
  
  /* first check if directory exist */
  DIR* dir = opendir("/gnuboy");
  if (dir == NULL) mkdir("/gnuboy",S_IRWXU);
  else closedir(dir);

  /* open config file */
  FILE *fp = fopen("/gnuboy/gnuboy.ini", "wb");
  if (fp == NULL) return;

  /* write file */
  fwrite(&config, sizeof(config), 1, fp);
  fclose(fp);
}

void config_load()
{
  /* open config file */
  FILE *fp = fopen("/gnuboy/gnuboy.ini", "rb");
  if (fp == NULL) return;

  /* read file */
  fread(&config, sizeof(config), 1, fp);
  fclose(fp);

  /* update gnuboy defaults */
  usefilter     = config.usefilter;
  filterdmg     = config.filterdmg;
  syncrtc       = config.syncrtc;
  forcedmg      = config.forcedmg;
  gbamode       = config.gbamode;
  paletteindex  = config.paletteindex;

  /* restore display */
  u16 xscale, yscale;
  if (config.aspect == 0)
  {
    xscale = 320;
    yscale = 224;
  }
  else if (config.aspect == 1)
  {
    xscale = 160;
    yscale = 144;
  }
  else
  {
    xscale = 250;
    yscale = 224;
  }
        
  square[6] = square[3]  =  xscale;
  square[0] = square[9]  = -xscale;
  square[4] = square[1]  =  yscale;
  square[7] = square[10] = -yscale;
  draw_init();
}
