
/******************************************************************************
 *  Gnuboy Gamecube port
 *  Original code by Softdev (@tehskeen.com)
 *  Adapted for Gnuboy Port by Eke-Eke (@tehskeen.com)
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
 *
 * Nintendo Gamecube Menus
 *
 ***************************************************************************/
#include "defs.h"
#include "font.h"
#include "dvd.h"
#include "mem.h"
#include "hw.h"
#include "rtc.h"
#define PSOSDLOADID 0x7c6000a6

extern void emu_reset();

/****************************************************************************
 * Generic Menu Routines
 ****************************************************************************/
int menu = 0;

void DrawMenu (char items[][20], int maxitems, int selected)
{
  int i;
  int ypos;
 
  ypos = (330 - (maxitems * fheight)) >> 1;
  ypos += 90;
  ClearScreen ();

  for (i = 0; i < maxitems; i++)
  {
      if (i == selected) WriteCentre_HL (i * fheight + ypos, (char *) items[i]);
      else WriteCentre (i * fheight + ypos, (char *) items[i]);
  }

  SetScreen ();
}

/****************************************************************************
 * domenu
 *
 * Returns index into menu array when A is pressed, -1 for B
 ****************************************************************************/
int DoMenu (char items[][20], int maxitems)
{
  int redraw = 1;
  int quit = 0;
  short p;
  int ret = 0;
  signed char a,b;

  while (quit == 0)
  {
    if (redraw)
	{
	  DrawMenu (&items[0], maxitems, menu);
	  redraw = 0;
	}

    p = PAD_ButtonsDown (0);
    a = PAD_StickY (0);
    b = PAD_StickX (0);

	/*** Look for up ***/
    if ((p & PAD_BUTTON_UP) || (a > 70))
	{
	  redraw = 1;
	  menu--;
	  if (menu < 0) menu = maxitems - 1;
	}

	/*** Look for down ***/
    if ((p & PAD_BUTTON_DOWN) || (a < -70))
	{
	  redraw = 1;
	  menu++;
	  if (menu == maxitems) menu = 0;
	}

    if ((p & PAD_BUTTON_A) || (b > 40) || (p & PAD_BUTTON_RIGHT))
	{
	  quit = 1;
	  ret = menu;
	}

	if ((b < -40) || (p & PAD_BUTTON_LEFT))
	{
	   quit = 1;
	   ret = 0 - 2 - menu;
	}

    if (p & PAD_BUTTON_B)
	{
	  quit = 1;
	  ret = -1;
	}
  }

  return ret;
}

/****************************************************************************
 * Options Menu
 *
 ****************************************************************************/
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

char paltxt[28][20] =
{
	{"   Default"},
	{"      Grey"},
	{"    Multi1"},
	{"    Multi2"},
	{"     Zelda"},
	{"   Metroid"},
	{"    AdvIsl"},
	{"   AdvIsl2"},
	{" BallonKid"},
	{"    Batman"},
	{"   BatmnRJ"},
	{"   BionCom"},
	{"    CV Adv"},
	{"  Dr.Mario"},
	{"     Kirby"},
	{"   DK Land"},
	{" WarioLand"},
	{"   GB-Left"},
	{" GB-Left+A"},
	{"     GB-Up"},
	{"   GB-Up+A"},
	{"   GB-Up+B"},
	{"  GB-Right"},
	{"GB-Right+A"},
	{"GB-Right+B"},
	{"   GB-Down"},
	{" GB-Down+A"},
	{" GB-Down+B"}
};

void OptionMenu ()
{
  int ret;
  int quit = 0;
  int filtering = 0;
  int optioncount = 10;
  char optionmenu[10][20];
  int prevmenu = menu;
  
  menu = 0;
  
  sprintf (optionmenu[0], "Scale X:         %02d", square[3]);
  sprintf (optionmenu[1], "Scale Y:         %02d", square[1]);
 
  filtering = usefilter + (filterdmg << 1);
  if (filtering == 0)      sprintf (optionmenu[2], "Filtering:      OFF");
  else if (filtering == 1) sprintf (optionmenu[2], "Filtering: GBC ONLY");
  else if (filtering == 3) sprintf (optionmenu[2], "Filtering:      ALL");
  
  sprintf (optionmenu[3], "Sprite Sorting:   %s", sprsort ? "Y" : "N");
  sprintf (optionmenu[4], "Force Mono:       %s",forcedmg ? "Y" : "N");
  sprintf (optionmenu[5], "GBA Features:     %s",gbamode ? "Y" : "N");
  sprintf (optionmenu[6], "Palette: %s", paltxt[paletteindex]);
  sprintf (optionmenu[7], "SRAM AutoLoad:    %s", autoload ? "Y" : "N");
  sprintf (optionmenu[8], "RTC Synchro       %s", syncrtc ? "Y" : "N");
  sprintf (optionmenu[9],"Return to previous");

  while (quit == 0)
  {
	  ret = DoMenu (&optionmenu[0], optioncount);

      switch (ret)
	  {
		  case 0: /* Scale X */
		  case -2:
			  if (ret<0) square[3] -= 2;
			  else square[3] += 2;
			  if (square[3] < 40) square[3] = 80;
  		      if (square[3] > 80) square[3] = 40;
			  square[6] = square[3];
			  square[0] = square[9] = -square[3];
			  sprintf (optionmenu[0], "Scale X:         %02d", square[3]);
              oldvwidth = -1;
			  break;

		  case 1: /* Scale Y */
		  case -3:
			  if (ret<0) square[1] -= 2;
			  else square[1] += 2;
			  if (square[1] < 30) square[1] = 60;
			  if (square[1] > 60) square[1] = 30;
			  square[4] = square[1];
			  square[7] = square[10] = -square[1];
			  sprintf (optionmenu[1], "Scale Y:         %02d", square[1]);
	          break;
		  
		  case 2: /* Graphics Filtering */
		      filtering += 1;
			  if (filtering == 2) filtering += 1; /* forbidden mode */
			  else if (filtering > 3) filtering = 0;
			  usefilter = filtering & 1;
			  filterdmg = (filtering >> 1) & 1;
			  if (filtering == 0)      sprintf (optionmenu[2], "Filtering:      OFF");
			  else if (filtering == 1) sprintf (optionmenu[2], "Filtering: GBC ONLY");
			  else if (filtering == 3) sprintf (optionmenu[2], "Filtering:      ALL");
			  break;

		  case 3:
			  sprsort ^= 1;
			  sprintf (optionmenu[3], "Sprite Sorting:   %s", sprsort ? "Y" : "N");
			  break;

		  case 4:
			  forcedmg ^= 1;
		      sprintf (optionmenu[4], "Force Mono:       %s",forcedmg ? "Y" : "N");
			  break;
		
		  case 5:
			  gbamode ^= 1;
		      sprintf (optionmenu[5], "GBA Features:     %s",gbamode ? "Y" : "N");
			  break;
		
		  case -8:
		  case 6:
              if (ret<0)
			  {
				if (paletteindex == 0) paletteindex = 27;
				else  paletteindex --;
		      }
			  else paletteindex++;
              if (paletteindex > 27) paletteindex = 0;
    		  sprintf (optionmenu[6], "Palette: %s", paltxt[paletteindex]);
		      break;

		  case 7:
			  autoload ^= 1;
			  sprintf (optionmenu[7], "SRAM AutoLoad:    %s", autoload ? "Y" : "N");
			  break;
			  
		  case 8:
			  syncrtc ^= 1;
			  sprintf (optionmenu[8], "RTC Synchro       %s", syncrtc ? "Y" : "N");
			  break;

		  case -1:
		  case 9:
			  quit = 1;
		      break;
	  }
  }

  menu = prevmenu;

}

/****************************************************************************
 * Rom Info Menu
 *
 ****************************************************************************/
void RomInfo ()
{
	char msg[128];
	int rlen, ypos;

	ClearScreen ();
  	ypos = 140;
	
    /* Title */
	WriteCentre (ypos , "Game Information");
	
	/* Rom name */
	ypos += fheight + fheight/2;
	sprintf(msg,"Internal Name - %s",rom.name);
	WriteCentre (ypos , msg);

	/* ROM Size */
	rlen = 16384 * mbc.romsize;
	rlen = rlen / 1024;
	if ((rlen/1024) == 0) sprintf(msg,"Rom Size - %d Ko",rlen);
	else sprintf(msg,"ROM Size - %d Mo",rlen/1024);
	ypos += fheight;
	WriteCentre (ypos , msg);

	/* RAM Size */
	sprintf(msg,"RAM Size - %d Ko",mbc.ramsize);
	ypos += fheight;
	WriteCentre (ypos , msg);

	/* Type */
	ypos += fheight;
	if (hw.cgb) WriteCentre (ypos , "Type - GameBoy COLOR");
	else WriteCentre (ypos , "Type - GameBoy MONO");

	/* Memory Controller Type */
	switch (mbc.type)
	{
		case MBC_NONE:
   	        sprintf(msg,"MBC Type - NONE");
		    break;
        case MBC_MBC1:
   	        sprintf(msg,"MBC Type - MBC1");
		    break;
        case MBC_MBC2:
   	        sprintf(msg,"MBC Type - MBC2");
		    break;
        case MBC_MBC3:
   	        sprintf(msg,"MBC Type - MBC3");
		    break;
        case MBC_MBC5:
   	        sprintf(msg,"MBC Type - MBC5");
		    break;
        case MBC_RUMBLE:
   	        sprintf(msg,"MBC Type - RUMBLE");
		    break;
        case MBC_HUC1:
   	        sprintf(msg,"MBC Type - HUC1");
		    break;
        case MBC_HUC3:
   	        sprintf(msg,"MBC Type - HUC3");
		    break;
    }
	ypos += fheight;
	WriteCentre ( ypos , msg);

	/* Internal battery */
	ypos += fheight;
	if (mbc.batt) WriteCentre (ypos , "Internal SRAM Batt. - YES");
	else WriteCentre (ypos , "Internal SRAM Batt. - NO");

	/* Internal RTC */
	ypos += fheight;
	if (rtc.batt) WriteCentre (ypos , "Internal RTC - YES");
	else WriteCentre (ypos , "Internal RTC - NO");

	ypos += 2*fheight;
	WriteCentre (ypos , "Press A to exit");
	
	SetScreen ();
    WaitButtonA ();
}


/****************************************************************************
 * Load game Menu
 *
 ****************************************************************************/
extern void OpenDVD ();
extern int OpenSD ();
extern u8 UseSDCARD;

void GameMenu ()
{
  int quit = 0;
  int ret;
  int prevmenu = menu;
  int loadcount = 3;
  char loadmenu[3][20] = {
    {"Load from DVD"},
    {"Load from SDCARD"},
    {"Return to previous"}};

  menu = UseSDCARD ? 1 : 0;

  while (quit == 0)
  {
    ret = DoMenu(&loadmenu[0], loadcount);

    switch (ret)
	{
      case -1:
      case  2:
	    quit = 1;
	    menu = prevmenu;
	    break;
	  case 0:
	    OpenDVD ();
		quit = 1;
		break;
	  case 1:
	    quit = OpenSD ();
		break;
	 }
  }
}

/****************************************************************************
 * Load/Save Menu
 *
 ****************************************************************************/
int CARDSLOT = CARD_SLOTB;
int use_SDCARD = 0;

extern int ManageState (int direction);
extern int ManageSRAM (int direction);
extern int LoadConfigFile();
extern int SaveConfigFile();

int loadsavemenu (int which)
{
  int prevmenu = menu;
  int quit = 0;
  int ret;
  int count = 5;
  char items[5][20];
  
  if (use_SDCARD) sprintf(items[0], "Device: SDCARD");
  else sprintf(items[0], "Device:  MCARD");

  if (CARDSLOT == CARD_SLOTA) sprintf(items[1], "Use: SLOT A");
  else sprintf(items[1], "Use: SLOT B");

  if (which  == 1)
  {
      sprintf(items[2], "Save State");
      sprintf(items[3], "Load State");
  }
  else if (which == 0)
  {
      sprintf(items[2], "Save SRAM");
      sprintf(items[3], "Load SRAM");
  }
  else if (which == 2)
  {
      sprintf(items[0], "Device: SDCARD");
	  sprintf(items[1], "Use: SLOT A");
	  sprintf(items[2], "Save Config File");
      sprintf(items[3], "Load Config File");
  }
  sprintf(items[4], "Return to previous");

  menu = 2;

  while (quit == 0)
  {
    ret = DoMenu(&items[0], count);

    switch (ret)
	{
      case -1:
      case  4:
	    quit = 1;
		break;
	  case 0:
		if (which == 2) break;
		use_SDCARD ^= 1;
		if (use_SDCARD) sprintf(items[0], "Device: SDCARD");
		else sprintf(items[0], "Device:  MCARD");
		break;
	  case 1:
	    if (which == 2) break;
		
		CARDSLOT ^= 1;
		if (CARDSLOT == CARD_SLOTA) sprintf(items[1], "Use: SLOT A");
		else sprintf(items[1], "Use: SLOT B");
	    break;
	  case 2:
        if (which == 1) quit = ManageState (0);
	    else if (which == 0) quit = ManageSRAM (0);
		else SaveConfigFile();
	    if (quit) return 1;
	    break;
	  case 3:
        if (which == 1) quit = ManageState (1);
	    else if (which == 0)quit = ManageSRAM (1);
		else quit = LoadConfigFile();
	    if (quit) return 1;
	    break;
	}
  }

  menu = prevmenu;
  return 0;
}

/****************************************************************************
 * File Manager Menu
 *
 ****************************************************************************/
int FileMenu ()
{
  int quit = 0;
  int ret;
  int prevmenu = menu;
  int mcardcount = 4;
  char mcardmenu[4][20] = {
    {"SRAM Manager"},
    {"STATE Manager"},
    {"Config File Manager"},
    {"Return to previous"}};

  menu = 0;

  while (quit == 0)
  {
    ret = DoMenu(&mcardmenu[0], mcardcount);

    switch (ret)
	{
      case -1:
      case  3:
	    quit = 1;
	    menu = prevmenu;
	    break;
	  case 0:
	  case 1:
	  case 2:
	    if (loadsavemenu(ret)) return 1;
	    break;
	}
  }

  return 0;
}

/****************************************************************************
 * Main Menu
 *
 ****************************************************************************/
void MainMenu ()
{
  int ret;
  int quit = 0;
  int *psoid = (int *) 0x80001800;
  void (*PSOReload) () = (void (*)()) 0x80001800;
  int maincount = 8;
  char mainmenu[8][20] = {
	{"Play Game"},
	{"Game Infos"},
	{"Reset Game"},
	{"Load New Game"},
	{"File Management"},
	{"Emulator Options"},
	{"Stop DVD Motor"},
	{"System Reboot"}};

  menu = 0;
  while (quit == 0)
  {
    ret = DoMenu (&mainmenu[0], maincount);

    switch (ret)
	{
	  case -1:  /*** Button B ***/
      case 0:	/*** Play Game ***/
	    quit = 1;
	    break;
	  case 1:
		RomInfo();
		break;
	  case 2:
		emu_reset();
		quit = 1;
		break;
	  case 3:
 		GameMenu ();
		menu = 0;
		break;
	  case 4:
		quit = FileMenu ();
		break;
      case 5:
		OptionMenu();
		break;
	  case 6:
  		ShowAction("Stopping DVD Motor ...");
        dvd_motor_off();
        break;
	  case 7:
		if (psoid[0] == PSOSDLOADID) PSOReload ();
		else SYS_ResetSystem(SYS_HOTRESET,0,FALSE);
	    break;
  	}
  }

  /*** Remove any still held buttons ***/
  while(PAD_ButtonsHeld(0)) VIDEO_WaitVSync();

  /*** Stop the DVD from causing clicks while playing ***/
  uselessinquiry ();
}
