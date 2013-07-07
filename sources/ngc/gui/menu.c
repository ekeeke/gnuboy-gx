/****************************************************************************
 *  menu.c
 *
 *  Gnuboy Plus GX menu
 *
 *  code by Softdev (March 2006), Eke-Eke (2007,2008)
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
 ***************************************************************************/

#include "defs.h"
#include "font.h"
#include "dvd.h"
#include "mem.h"
#include "hw.h"
#include "rtc.h"
#include "config.h"
#include "file_fat.h"
#include "file_dvd.h"

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <di/di.h>
#endif

extern u8 *gbrom;
extern u8 legal();
/***************************************************************************
 * drawmenu
 *
 * As it says, simply draws the menu with a highlight on the currently
 * selected item :)
 ***************************************************************************/
char menutitle[60] = { "" };
int menu = 0;

void DrawMenu (char items[][20], int maxitems, int selected)
{
  int i;
  int ypos;
 
  ypos = (330 - (maxitems * fheight)) >> 1;
  ypos += 90;
  ClearScreen ();
  WriteCentre (134, menutitle);

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

  while (quit == 0)
  {
    if (redraw)
    {
      DrawMenu (&items[0], maxitems, menu);
      redraw = 0;
    }

    p = getMenuButtons ();

    if (p & PAD_BUTTON_UP)
    {
      redraw = 1;
      menu--;
      if (menu < 0) menu = maxitems - 1;
    }

    if (p & PAD_BUTTON_DOWN)
    {
      redraw = 1;
      menu++;
      if (menu == maxitems) menu = 0;
    }

    if ((p & PAD_BUTTON_A) || (p & PAD_BUTTON_RIGHT))
    {
      quit = 1;
      ret = menu;
    }

    if (p & PAD_BUTTON_LEFT)
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
extern void config_save();
extern u8 usefilter;    /* Apply Display Filter */
extern u8 filterdmg;    /* Apply Filter also for GameBoy MONO */
extern u8 syncrtc;      /* Synchronize in-game RTC on SRAM load with your own Console RTC  */
extern s8 autoload;     /* SRAM Auto-load feature */
extern u8 forcedmg;     /* Force Mono Gameboy  */
extern u8 gbamode;      /* enables GBA-only features present in some GBC games */
extern u8 paletteindex; /* Index of selected palette color*/
extern s16 square[];
extern void draw_init();

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
  int filtering = config.usefilter + (config.filterdmg << 1);
  char optionmenu[7][20];
  int prevmenu = menu;
  u16 xscale, yscale;
  
  menu = 0;


  while (quit == 0)
  {
    strcpy (menutitle, "");

    if (config.aspect == 0)      sprintf (optionmenu[0], "Aspect: STRETCH");
    else if (config.aspect == 1) sprintf (optionmenu[0], "Aspect: ORIGINAL");
    else if (config.aspect == 2) sprintf (optionmenu[0], "Aspect: SCALED");
    sprintf (optionmenu[1], "Force Mono: %s",config.forcedmg ? "YES" : "NO");
    if (filtering == 1) sprintf (optionmenu[2], "Filtering: GBC");
    else if (filtering == 3) sprintf (optionmenu[2], "Filtering: ALL");
    else sprintf (optionmenu[2], "Filtering: OFF");
    sprintf (optionmenu[3], "GBA Features: %s",config.gbamode ? "YES" : "NO");
    sprintf (optionmenu[4], "Palette: %s", paltxt[config.paletteindex]);
    sprintf (optionmenu[5], "RTC Synchro: %s", config.syncrtc ? "YES" : "NO");
    sprintf(optionmenu[6], "Return to previous");

    ret = DoMenu (&optionmenu[0], 7);

    switch (ret)
    {
      case 0: /* Aspect ratio */
        config.aspect = (config.aspect + 1) % 3;
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
        break;

      case 1: /* Force Monochrome Display */
        config.forcedmg ^= 1;
        break;
      
      case 2: /* Graphics Filtering */
        filtering ++;
        if (filtering == 2) filtering = 3;
        if (filtering > 3) filtering = 0;
        config.usefilter = filtering & 1;
        config.filterdmg = (filtering >> 1) & 1;
        break;

      case 3: /* GBA-only features (used in some GB/GBC games) */
        config.gbamode ^= 1;
        break;

      case 4: /* Color Palettes */
      case -6:
        if (ret<0)
        {
          if (config.paletteindex == 0) config.paletteindex = 27;
          else  config.paletteindex --;
        }
        else
        {
          config.paletteindex++;
          if (config.paletteindex > 27) config.paletteindex = 0;
        }
        break;

      case 5: /* RTC synchro */
        config.syncrtc ^= 1;
        break;

      case -1:
      case 6:
        quit = 1;
        break;
     }
  }

  /* save Config File */
  config_save();

  /* update gnuboy defaults */
  usefilter     = config.usefilter;
  filterdmg     = config.filterdmg;
  syncrtc       = config.syncrtc;
  paletteindex  = config.paletteindex;
  forcedmg      = config.forcedmg;
  gbamode       = config.gbamode;
  u8 c = gbrom[0x0147];
  hw.cgb = ((c == 0x80) || (c == 0xc0)) && !forcedmg;
  hw.gba = (hw.cgb && gbamode);

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
    strcpy (menutitle, "");
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
 * Load/Save Menu
 *
 ****************************************************************************/
static u8 device = 0;

extern int ManageSRAM (u8 direction, u8 device);
extern int ManageState (u8 direction, u8 device);

int loadsavemenu (int which)
{
  int prevmenu = menu;
  int quit = 0;
  int ret;
  int count = 5;
  char items[5][20];
  
  if (which  == 1)
  {
    strcpy (menutitle, "STATE Manager");
    sprintf(items[1], "Save State");
    sprintf(items[2], "Load State");
  }
  else
  {
    strcpy (menutitle, "SRAM Manager");
    sprintf(items[1], "Save SRAM");
    sprintf(items[2], "Load SRAM");
  }
  sprintf(items[4], "Return to previous");

  menu = 3;

  while (quit == 0)
  {
     if (device == 0) sprintf(items[0], "Device: FAT");
     else if (device == 1) sprintf(items[0], "Device: MCARD A");
     else if (device == 2) sprintf(items[0], "Device: MCARD B");

     if (which  == 1)
     {
        if (config.freeze_auto == 0) sprintf (items[3], "Auto FREEZE: FAT");
        else if (config.freeze_auto == 1) sprintf (items[3], "Auto FREEZE: MCARD A");
        else if (config.freeze_auto == 2) sprintf (items[3], "Auto FREEZE: MCARD B");
        else sprintf (items[3], "Auto FREEZE: OFF");
     }
     else
     {
        if (config.sram_auto == 0) sprintf (items[3], "Auto SRAM: FAT");
        else if (config.sram_auto == 1) sprintf (items[3], "Auto SRAM: MCARD A");
        else if (config.sram_auto == 2) sprintf (items[3], "Auto SRAM: MCARD B");
        else sprintf (items[3], "Auto SRAM: OFF");
     }

    ret = DoMenu(&items[0], count);

    switch (ret)
    {
      case -1:
      case 4:
         quit = 1;
         break;

      case 0:  // Device FAT / MEMORY CARD
         device = (device + 1)%3;
         break;
      case 1:  // ManageState or ManageSRAM
      case 2:
        if (which == 1) quit = ManageState (ret-1,device);
        else quit = ManageSRAM (ret-1,device);
        if (quit) return 1;
        break;
      case 3:  
         if (which  == 1)  // FreezeState autoload & autosave
         {
            config.freeze_auto ++;
            if (config.freeze_auto > 2) config.freeze_auto = -1;
            config_save();
            break;
         }
         else  // SRAM autoload & autosave
         {
            config.sram_auto ++;
            if (config.sram_auto > 2) config.sram_auto = -1;
            config_save();
            break;
         }
    }
  }

  menu = prevmenu;
  return 0;
}

/****************************************************************************
 * Emulator Options Menu
 *
 ****************************************************************************/
int Emu_options ()
{
  int quit = 0;
  int ret;
  int prevmenu = menu;
  int mcardcount = 6;
  char mcardmenu[6][20] =
  {
    {"Video Options"},
    {"SRAM Manager"},
    {"STATE Manager"},
    {"Game Infos"},    
    {"View Credits"},
    {"Return to previous"}
  };

  menu = 0;

  while (quit == 0)
  {
    strcpy (menutitle, "Emulator Options");
    ret = DoMenu(&mcardmenu[0], mcardcount);

    switch (ret)
     {
        case -1:
        case  5:
           quit = 1;
           break;
        case 0:   // Video Options
           OptionMenu();
           break;
        case 1:   // File Manager
        case 2:
           if (loadsavemenu(ret-1)) return 1;
           break;
        case 3:   // ROM Information
           RomInfo();
           break;
        case 4:   // view credits
           legal();
           break;
    }
  }

  menu = prevmenu;
  return 0;
}

/****************************************************************************
 * Load Rom menu
 *
 ****************************************************************************/
extern int gbromsize;
extern int reload_rom ();
extern void memfile_autosave();
extern void memfile_autoload();

static u8 load_menu = 0;
static u8 dvd_on = 0;


int loadmenu ()
{
  int prevmenu = menu;
  int ret,count,size;
  int quit = 0;
#ifdef HW_RVL
  count = 7;
  char item[7][20] = {
    {"Load Recent"},
    {"Load from SD"},
    {"Load from USB"},
    {"Load from IDE-EXI"},
    {"Load from DVD"},
    {"Stop DVD Motor"},
    {"Return to previous"}
  };
#else
  count = 7;
  char item[7][20] = {
    {"Load Recent"},
    {"Load from SD"},
    {"Load from IDE-EXI"},
    {"Load from WKF"},
    {"Load from DVD"},
    {"Stop DVD Motor"},
    {"Return to previous"}
  };
#endif

  menu = load_menu;
  
  while (quit == 0)
  {
    strcpy (menutitle, "");
    ret = DoMenu (&item[0], count);
    switch (ret)
    {
      // Load from DVD

      case 4:
        load_menu = menu;
        size = DVD_Open(gbrom);
        if (size)
        {
          dvd_on = 1;
          gbromsize = size;
          memfile_autosave();
          reload_rom();
          memfile_autoload();
          return 1;
        }
        break;

      // Stop DVD Disc
      case 5:
        dvd_motor_off();
        dvd_on = 0;
        menu = load_menu;
        break;


      // Button B - Return to Previous
      case -1:
      case 6:
        quit = 1;
        break;

      // Load from SD, USB, IDE-EXI, or WKF device
      default:
        load_menu = menu;
        size = FAT_Open(ret,gbrom);
        if (size)
        {
          gbromsize = size;
          memfile_autosave();
          reload_rom();
          memfile_autoload(); 
          return 1;
        }
        break;
    }
  }

  menu = prevmenu;
  return 0;
}

/****************************************************************************
 * Main Menu
 *
 ****************************************************************************/
extern void emu_reset();
extern u8 gc_pal;
extern unsigned int *xfb[2];
extern int whichfb;
extern GXRModeObj *vmode;


void MainMenu ()
{
  s8 ret;
  u8 quit = 0;
  menu = 0;
#ifdef HW_RVL
  u8 count = 6;
  char items[6][20] =
#else
  u8 count = 5;
  char items[5][20] =
#endif
  {
    {"Play Game"},
    {"Hard Reset"},
    {"Load New Game"},
    {"Emulator Options"},
#ifdef HW_RVL
    {"Return to Loader"},
#endif
    {"System Reboot"}
  };

  /* 50 hz TV mode */
  if (gc_pal)
  {
    VIDEO_Configure (vmode);
    VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_BLACK);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    VIDEO_WaitVSync();
  }

  /* autosave (SRAM only) */
  int temp = config.freeze_auto;
  config.freeze_auto = -1;
  memfile_autosave();
  config.freeze_auto = temp;

  
  while (quit == 0)
  {
    strcpy (menutitle, "Version 1.04.2");  
    ret = DoMenu (&items[0], count);

    switch (ret)
    {
      case -1: /*** Button B ***/
      case 0:  /*** Play Game ***/
        quit = 1;
        break;

      case 1:
        emu_reset();
        quit = 1;
        break;
 
      case 2:  /*** Load ROM Menu ***/
        quit = loadmenu();
        break;

      case 3:  /*** Emulator Options */
        Emu_options();
        break;

      case 4:  /*** SD/PSO/TP Reload ***/
        memfile_autosave();
        VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_BLACK);
        VIDEO_Flush();
        VIDEO_WaitVSync();
#ifdef HW_RVL
        DI_Close();
        exit(0);
        break;

      case 5:  /*** Return to Wii System Menu ***/
        memfile_autosave();
        VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_BLACK);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        DI_Close();
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
#else
        SYS_ResetSystem(SYS_HOTRESET,0,0);
#endif
        break;
    }
  }
  
  /*** Remove any still held buttons ***/
  while (PAD_ButtonsHeld(0)) PAD_ScanPads();
#ifdef HW_RVL
  while (WPAD_ButtonsHeld(0)) WPAD_ScanPads();
#endif

  /*** Restore fullscreen 50hz ***/
  if (gc_pal)
  {
    extern GXRModeObj TV50hz_576i;
    GXRModeObj *rmode = &TV50hz_576i;
    Mtx p;

    rmode->xfbHeight = 574;
    rmode->viYOrigin = 0;
    rmode->viHeight = 574;
    VIDEO_Configure (rmode);
    VIDEO_ClearFrameBuffer(rmode, xfb[whichfb], COLOR_BLACK);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    VIDEO_WaitVSync();

    /* reset rendering mode */
    GX_SetViewport (0.0F, 0.0F, rmode->fbWidth, rmode->efbHeight, 0.0F, 1.0F);
    GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);
    f32 yScale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u16 xfbHeight = GX_SetDispCopyYScale (yScale);
    GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst (rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
    GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    guOrtho(p, rmode->efbHeight/2, -(rmode->efbHeight/2), -(rmode->fbWidth/2), rmode->fbWidth/2, 100, 1000);
    GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

  }

#ifndef HW_RVL
  /*** Stop the DVD from causing clicks while playing ***/
  uselessinquiry ();
#endif
}
