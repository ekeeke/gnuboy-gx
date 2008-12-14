/*
 * Gnuboy Main for the Nintendo Gamecube
 * Eke-Eke 2007
 * This file may be distributed under the terms of the GNU GPL.
 */
 
#include "defs.h"
#include "mem.h"
#include "lcd.h"
#include "font.h"
#include "config.h"

#include <malloc.h>

#define MAXROMSIZE 8388608

int gbromsize;
u8 *gbrom;
int new_game;

extern void MainMenu ();
extern void legal();

void reload_rom()
{
  loader_unload();
  mem_init ();
  rom_load();
  pcm_reset();
  vid_reset();
  emu_reset();
  new_game = 1;
}

int main(int argc, char *argv[])
{
  vid_init();
  pcm_init();
  mem_init();
  legal();

  gbrom = memalign(32,MAXROMSIZE);
  gbromsize = 0;
  
  /* try to load Config File */
  config_load();

  /* Wait for loaded rom */
  while (gbromsize == 0) MainMenu();
  
  /* go to Main Loop */
  emu_run();
 
  /* never reached */
  return 0;
}











