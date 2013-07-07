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
#ifndef __CONFIG_H__
#define __CONFIG_H__

typedef struct 
{
  u8 aspect;       /* Aspect Ratio */
  u8 usefilter;    /* Apply Display Filter */
  u8 filterdmg;    /* Apply Filter also for GameBoy MONO */
  u8 syncrtc;      /* Synchronize in-game RTC on SRAM load with your own Console RTC  */
  u8 forcedmg;     /* Force Mono Gameboy  */
  u8 gbamode;      /* enables GBA-only features present in some GBC games */
  u8 paletteindex; /* Index of selected palette color */
  s8 sram_auto;
  s8 freeze_auto;
} ConfigOptions;

extern ConfigOptions config;

extern void config_load();
extern void config_save();

#endif
