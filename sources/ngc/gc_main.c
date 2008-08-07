/*
 * Gnuboy Main for the Nintendo Gamecube
 * Original Code by Laguna & Gilgamesh - Modified by Eke-Eke 2007
 * This file may be distributed under the terms of the GNU GPL.
 */
 
#include "defs.h"
#include "mem.h"
#include "lcd.h"

#define MAXROMSIZE 8388608

int gbromsize;
u8 *gbrom;

int ConfigRequested;
int new_game;
extern void MainMenu ();
extern void legal();
extern void file_autoload(u8 type);

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

  gbrom = malloc(MAXROMSIZE+32);
  gbromsize = 0;
	
  /* try to load Config File */
  file_autoload(0);

  /* Wait for loaded rom */
  while (gbromsize == 0) MainMenu();
		
  /* go to Main Loop */
  emu_run();
 
  /* never reached */
  return 0;
}











