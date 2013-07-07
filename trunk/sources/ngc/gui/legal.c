/*
 * Gnuboy EMU Support for the Nintendo Gamecube
 * Original Code by Laguna & Gilgamesh - Modified by Eke-Eke 2007
 * This file may be distributed under the terms of the GNU GPL.
 */

#include "defs.h"
#include "font.h"
#include "dkpro.h"

extern u32 *xfb[2];
extern int whichfb;

/*
 * Unpack the devkit pro logo
 */
u32 *dkproraw;

int dkunpack ()
{
  unsigned long res, inbytes, outbytes;

  inbytes = dkpro_COMPRESSED;
  outbytes = dkpro_RAW;
  dkproraw = malloc (dkpro_RAW + 16);
  res = uncompress ((Bytef *) dkproraw, &outbytes, (Bytef *) &dkpro[0], inbytes);
  if (res == Z_OK) return 1;
  free (dkproraw);
  return 0;
}

/* 
 * This is the legal stuff - which must be shown at program startup 
 * Any derivative work MUST include the same textual output.
 *
 * In other words, play nice and give credit where it's due.
 */
void legal ()
{
  int ypos = 64;

  whichfb ^= 1;
  VIDEO_ClearFrameBuffer(&TVNtsc480IntDf, xfb[whichfb], COLOR_WHITE);
  back_framewidth = 640;

  WriteCentre (ypos, "Gnuboy Gameboy/Gameboy Color Emulator");
  ypos += fheight;
  WriteCentre (ypos, "Based on CVS version 1.04");
  ypos += fheight;
  WriteCentre (ypos, "Original Code by Laguna and Gilgamesh");
  ypos += fheight;
  WriteCentre (ypos, "This is free software, and you are welcome to");
  ypos += fheight;
  WriteCentre (ypos, "redistribute it under the conditions of the");
  ypos += fheight;
  WriteCentre (ypos, "GNU GENERAL PUBLIC LICENSE Version 2");
  ypos += fheight;
  WriteCentre (ypos, "Additionally, the developers of this port");
  ypos += fheight;
  WriteCentre (ypos, "disclaims all copyright interests in the ");
  ypos += fheight;
  WriteCentre (ypos, "Nintendo Gamecube Porting code.");
  ypos += fheight;
  WriteCentre (ypos, "You are free to use it as you wish.");
  ypos += 6 * fheight;

  if (dkunpack ())
  {
      int w, h, p, dispoffset;
      p = 0;
      dispoffset = (316 * 320) + ((640 - dkpro_WIDTH) >> 2);

      for (h = 0; h < dkpro_HEIGHT; h++)
    {
      for (w = 0; w < dkpro_WIDTH >> 1; w++)
      xfb[whichfb][dispoffset + w] = dkproraw[p++];

      dispoffset += 320;
    }

      free (dkproraw);
  }
  else WriteCentre (ypos, "Developed with DevkitPPC and libOGC");

  WriteCentre (ypos, "Press A to continue");
  SetScreen ();
  WaitButtonA ();
}
