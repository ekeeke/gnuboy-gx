	/******************************************************************************
 *  Gnuboy Gamecube port
 *  Original code by Softdev (@tehskeen.com)
 *  Modifications & SDCARD subdirectory support by Eke-Eke (@tehskeen.com)
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
#include <sdcard.h>
#include "font.h"


struct ConfigOptions
{
	u8 UseFilter;	/* Apply Display Filter */
	u8 FilterDMG;	/* Apply Filter also for GameBoy MONO */
	u8 SpriteSort;  /* Sprite Sorting */
	u8 SyncRTC;		/* Synchronize in-game RTC on SRAM load with your own Console RTC  */
	u8 AutoLoad;	/* SRAM Auto-load feature */
	u8 ForceDMG;	/* Force Mono Gameboy  */
	u8 GBAMode;		/* enables GBA-only features present in some GBC games */
	u8 PaletteIndex; /*Index of selected palette color*/
	int ScaleX;
	int ScaleY;
} Config;

char *configFile = "dev0:\\gnuboy\\gnuboyGX.cfg";  

extern u8 usefilter; /* Apply Display Filter */
extern u8 filterdmg; /* Apply Filter also for GameBoy MONO */
extern u8 sprsort;   /* Sprite Sorting */
extern u8 syncrtc;   /* Synchronize in-game RTC on SRAM load with your own Console RTC  */
extern u8 autoload;  /* SRAM Auto-load feature */
extern u8 forcedmg;  /* Force Mono Gameboy  */
extern u8 gbamode;  /* enables GBA-only features present in some GBC games */
extern u8 paletteindex; /*Index of selected palette color*/
extern s16 square[];
extern int oldvwidth;

int LoadConfigFile()
{
	sd_file *handle = SDCARD_OpenFile (configFile, "rb");     
    if (handle > 0)
	{
		ShowAction("Loading file ...");
		SDCARD_ReadFile (handle, &Config, sizeof(struct ConfigOptions));

		square[3] = Config.ScaleX; /* Scale X */
		if (square[3] > 80) square[3] = 40;
		square[6] = square[3];
		square[0] = square[9] = -square[3];
		oldvwidth = -1;		         

		square[1] = Config.ScaleY; /* Scale Y */
		if (square[1] > 60) square[1] = 30;
		square[4] = square[1];
		square[7] = square[10] = -square[1];

		usefilter = Config.UseFilter;
		filterdmg = Config.FilterDMG;
		sprsort	= Config.SpriteSort;
		syncrtc = Config.SyncRTC;
		autoload = Config.AutoLoad; 
		forcedmg = Config.ForceDMG;
		gbamode = Config.GBAMode;
		paletteindex = Config.PaletteIndex;

		SDCARD_CloseFile (handle);
		WaitPrompt("File loaded");
		return 1;    
	}

	WaitPrompt("Error opening dev0:\\gnuboy\\gnuboyGX.cfg");
	return 0;    
}

int SaveConfigFile()
{
	sd_file *handle = SDCARD_OpenFile (configFile, "wb");
	
	if (handle > 0)
	{
		Config.ScaleX = square[3];
		Config.ScaleY = square[1];
		Config.UseFilter = usefilter;      
		Config.FilterDMG = filterdmg;      
		Config.SpriteSort = sprsort;
		Config.SyncRTC = syncrtc;
		Config.AutoLoad = autoload;
		Config.ForceDMG = forcedmg;       
		Config.GBAMode = gbamode;        
		Config.PaletteIndex = paletteindex;   

		ShowAction("Saving file...");
        SDCARD_WriteFile (handle, &Config, sizeof(struct ConfigOptions));
        SDCARD_CloseFile (handle);
		WaitPrompt("File saved");
		return 1;
    }

	WaitPrompt("Error opening dev0:\\gnuboy\\gnuboyGX.cfg");
	return 0;    
}
