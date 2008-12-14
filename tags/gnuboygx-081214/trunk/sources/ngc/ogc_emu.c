/*
 * Gnuboy EMU Support for the Nintendo Gamecube
 * Original Code by Laguna & Gilgamesh - Modified by Eke-Eke 2007
 * This file may be distributed under the terms of the GNU GPL.
 */

#include "defs.h"
#include "regs.h"
#include "hw.h"
#include "cpu.h"
#include "mem.h"
#include "lcd.h"

void emu_init()
{}

/*
 * emu_reset is called to initialize the state of the emulated
 * system. It should set cpu registers, hardware registers, etc. to
 * their appropriate values at powerup time.
 */

void emu_reset()
{
  hw_reset();
  lcd_reset();
  cpu_reset();
  mbc_reset();
  sound_reset();
}

void emu_step()
{
  cpu_emulate(cpu.lcdc);
}

extern int new_game;
extern int frameticker;
extern long long gettime();
extern long long prev;

void emu_run()
{
  for (;;)
  {
    vid_begin();
    lcd_begin();
    new_game = 0;
    frameticker = 0;
    prev = gettime();

    for (;;)
    {
      cpu_emulate(2280);
      while (R_LY > 0 && R_LY < 144) emu_step();
      vid_end();
      rtc_tick();
      sound_mix();
      pcm_submit();

      ev_poll();
      if (new_game) break;
      
      vid_begin();
      if (!(R_LCDC & 0x80))cpu_emulate(32832);
      while (R_LY > 0) emu_step();/* wait for next frame */      
    }
  }
}

