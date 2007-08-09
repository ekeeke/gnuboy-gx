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
 ***************************************************************************/
#include <sdcard.h>
#include "defs.h"
#include "dvd.h"
#include "font.h"
#include "mem.h"
#include "saveicon.h"	/*** Nice little icon - thanks brakken! ***/

extern void loadstate (u8 *buffer);
extern int savestate (u8 *buffer);
extern void sram_load (u8 *srambuf);
extern int sram_save (u8 *srambuf);
extern int gbromsize;

/**
 * libOGC System Work Area
 */
static u8 SysArea[CARD_WORKAREA] ATTRIBUTE_ALIGN (32);

/**
 * DMA Transfer Area.
 * Must be 32-byte aligned.
 * 64k SRAM + 2k Icon
 */
u8 savebuffer[0x30000] ATTRIBUTE_ALIGN (32);

card_dir CardDir;
card_file CardFile;
card_stat CardStatus;
extern int CARDSLOT;
extern int use_SDCARD;

int ManageSRAM (int direction);
int ManageState (int direction);

/****************************************************************************
 * FILE autoload (SRAM or Config File)
 *
 * the detection order is the following:
 *		MCARD (SLOTA) > MCARD (SLOTB) > SDCARD (SLOTA) > SDCARD (SLOTB)
 *
 *****************************************************************************/
extern u8 SILENT;
extern int LoadConfigFile();
extern int SaveConfigFile();

void file_autoload(u8 type)
{
	int ret = 0;
	int i = 0;
	int old_cardslot = CARDSLOT;
	int old_sdcard = use_SDCARD;

	SILENT = 1; /* this should be transparent to the user */
	while ((i < 4) && (!ret))
	{
		use_SDCARD = (i&2) ? 1 : 0;
		CARDSLOT   = (i&1) ? CARD_SLOTB : CARD_SLOTA;
		if (type) ret = ManageSRAM(1);
		else ret = LoadConfigFile();
		i++;
	}
	if (!ret)
	{
		/* restore old settings if not found */
		CARDSLOT = old_cardslot; 
		use_SDCARD = old_sdcard;
	}
	SILENT = 0;
}

/****************************************************************************
 * SDCARD Access functions
 *
 * We use the same buffer as for Memory Card manager
 * Function returns TRUE on success.
 *****************************************************************************/
int SD_ManageFile(char *filename, int direction, int filetype)
{
    char name[1024];
	sd_file *handle;
    int len = 0;
    int offset = 0;
	int filesize;
	
	/* build complete SDCARD filename */
	sprintf (name, "dev%d:\\gnuboy\\saves\\%s", CARDSLOT, filename);

	/* open file */
	handle = direction ? SDCARD_OpenFile (name, "rb") :
						 SDCARD_OpenFile (name, "wb");

	if (handle == NULL)
	{
		sprintf (filename, "Error opening %s", name);
		WaitPrompt(filename);
		return 0;
	}
	
	switch (direction)
	{
		case 0: /* SAVING */

			if (filetype) filesize = sram_save(&savebuffer[0]); /* SRAM */
			else filesize = savestate(&savebuffer[0]); /* STATE */

			len = SDCARD_WriteFile (handle, savebuffer, filesize);
			SDCARD_CloseFile (handle);

			if (len != filesize)
			{
				sprintf (filename, "Error writing %s", name);
				WaitPrompt (filename);
				return 0;
			}
			
			sprintf (filename, "Saved %d bytes successfully", filesize);
			WaitPrompt (filename);
			return 1;
		
		case 1: /* LOADING */
		
			while ((len = SDCARD_ReadFile (handle, &savebuffer[offset], 2048)) > 0) offset += len;
			if (filetype) sram_load(&savebuffer[0]); /* SRAM */
			else loadstate(&savebuffer[0]); /* STATE */
            SDCARD_CloseFile (handle);

			sprintf (filename, "Loaded %d bytes successfully", offset);
			WaitPrompt (filename);
			return 1;
	}
	
	return 0; 
}

/****************************************************************************
 * MountTheCard
 *
 * libOGC provides the CARD_Mount function, and it should be all you need.
 * However, experience with previous emulators has taught me that you are
 * better off doing a little bit more than that!
 *
 * Function returns TRUE on success.
 *****************************************************************************/
int MountTheCard ()
{
  int tries = 0;
  int CardError;
  while (tries < 10)
  {
      *(unsigned long *) (0xcc006800) |= 1 << 13;		/*** Disable Encryption ***/
      uselessinquiry ();
      VIDEO_WaitVSync ();
      CardError = CARD_Mount (CARDSLOT, SysArea, NULL);			/*** Don't need or want a callback ***/
      if (CardError == 0) return 1;
 	  else EXI_ProbeReset ();
      tries++;
  }
  return 0;
}

/****************************************************************************
 * CardFileExists
 *
 * Wrapper to search through the files on the card.
 * Returns TRUE if found.
 ****************************************************************************/
int CardFileExists (char *filename)
{
  int CardError = CARD_FindFirst (CARDSLOT, &CardDir, TRUE);
  while (CardError != CARD_ERROR_NOFILE)
  {
      CardError = CARD_FindNext (&CardDir);
      if (strcmp ((char *) CardDir.filename, filename) == 0) return 1;
  }
  return 0;
}

/****************************************************************************
 * ManageSRAM
 *
 * Here is the main SRAM Management stuff.
 * The output file contains an icon (2K), 64 bytes comment and the SRAM (64k).
 * As memcards are allocated in blocks of 8k or more, you have a around
 * 6k bytes to save/load any other data you wish without altering any of the
 * main save / load code.
 *
 * direction == 0 save, 1 load.
 ****************************************************************************/
int ManageSRAM (int direction)
{
  char filename[128];
  char action[80];
  int CardError;
  unsigned int SectorSize;
  int blocks;
  char comment[2][32] = { {"GnuboyGX 1.4"}, {"SRAM"} };
  int outbytes = 0;
  int sbo;
  int sram_size = 0;

  if (!gbromsize) return 0;

  if (direction) ShowAction ("Loading SRAM & RTC ...");
  else ShowAction ("Saving SRAM & RTC ...");

  /*** No idea what the damn ROM is, so just use CRC32 ***/
  sprintf (filename, "%s.srm", rom.name);
  strcpy (comment[1], filename);

  memset(&savebuffer, 0, 0x30000);
	
  /* device is SDCARD, let's go */
  if (use_SDCARD) return SD_ManageFile(filename,direction,1);

  /* device is MCARD, we continue */
  if (direction == 0) /*** Saving ***/
  {	
	/*** Build the output buffer ***/
    memcpy(&savebuffer, &icon, 2048);
    memcpy(&savebuffer[2048], &comment[0], 64);
    sram_size = sram_save(&savebuffer[2112]);
  }

  outbytes = 2048 + 64 + sram_size;

  /*** Initialise the CARD system ***/
  memset (&SysArea, 0, CARD_WORKAREA);
  CARD_Init ("GNUB", "01");

  /*** Attempt to mount the card ***/
  CardError = MountTheCard ();

  if (CardError)
  {
	  /*** Retrieve the sector size ***/
      CardError = CARD_GetSectorSize (CARDSLOT, &SectorSize);

      switch (direction)
	  {
		case 0: /*** Saving ***/
			/*** Determine number of blocks on this card ***/
	  		blocks = (outbytes / SectorSize) * SectorSize;
	  		if (outbytes % SectorSize) blocks += SectorSize;

			/*** Check if a previous save exists ***/
	  		if (CardFileExists (filename))
	    	{
	      		CardError = CARD_Open (CARDSLOT, filename, &CardFile);
	      		if (CardError)
				{
		  			sprintf (action, "Error Open : %d", CardError);
		  			WaitPrompt (action);
					CARD_Unmount (CARDSLOT);
					  return 0;
				}

				int size = CardFile.len;
				CARD_Close (&CardFile);

				if (size < blocks)
				{
					/* new size is bigger: check if there is enough space left */
				    CardError = CARD_Create (CARDSLOT, "TEMP", blocks-size, &CardFile);
					if (CardError)
					{
						sprintf (action, "Error Update : %d", CardError);
						WaitPrompt (action);
						CARD_Unmount (CARDSLOT);
						return 0;
					}
					CARD_Close (&CardFile);
					CARD_Delete(CARDSLOT, "TEMP");
				}

				/* always delete existing slot */
				CARD_Delete(CARDSLOT, filename);
		    }

			/*** Create a new slot ***/
	      	CardError = CARD_Create (CARDSLOT, filename, blocks, &CardFile);
			if (CardError)
			{
		  		sprintf (action, "Error create : %d %d", CardError, CARDSLOT);
		  		WaitPrompt (action);
				CARD_Unmount (CARDSLOT);
		 		 return 0;
			}
	  		
			/*** Continue and save ***/
			CARD_GetStatus (CARDSLOT, CardFile.filenum, &CardStatus);
			CardStatus.icon_addr = 0x0;
			CardStatus.icon_fmt = 2;
			CardStatus.icon_speed = 1;
			CardStatus.comment_addr = 2048;
			CARD_SetStatus (CARDSLOT, CardFile.filenum, &CardStatus);

			/*** And write the blocks out ***/
			sbo = 0;
			while (outbytes > 0)
	    	{
	      		CardError = CARD_Write (&CardFile, &savebuffer[sbo], SectorSize, sbo);
	      		outbytes -= SectorSize;
	      		sbo += SectorSize;
	    	}

	  		CARD_Close (&CardFile);
	  		CARD_Unmount (CARDSLOT);
			sprintf (action, "Saved %d bytes successfully", blocks);
	  		WaitPrompt (action);
	  		return 1;

		default: /*** Loading ***/
	  		if (!CardFileExists (filename))
	    	{
				WaitPrompt ("No SRAM File Found");
				CARD_Unmount (CARDSLOT);
	            return 0;
	        }

	  	    memset (&CardFile, 0, sizeof (CardFile));
			CardError = CARD_Open (CARDSLOT, filename, &CardFile);
			if (CardError)
			{
				sprintf (action, "Error Open : %d", CardError);
				WaitPrompt (action);
				CARD_Unmount (CARDSLOT);
				return 0;
			}

			blocks = CardFile.len;
			if (blocks < SectorSize) blocks = SectorSize;
			if (blocks % SectorSize) blocks++;

			/*** Just read the file back in ***/
			sbo = 0;
			int size = blocks;
			while (blocks > 0)
			{
				CARD_Read (&CardFile, &savebuffer[sbo], SectorSize, sbo);
				sbo += SectorSize;
				blocks -= SectorSize;
			}
			CARD_Close (&CardFile);
			CARD_Unmount (CARDSLOT);

			sram_load(&savebuffer[2112]);

			/*** Inform user ***/
			sprintf (action, "Loaded %d bytes successfully", size);
			WaitPrompt (action);
			return 1;
		}
  }
  else WaitPrompt ("Unable to mount memory card");
  return 0;		/*** Signal failure ***/
}

/****************************************************************************
 * ManageState
 *
 * Here is the main Freeze File Management stuff.
 * The output file contains an icon (2K), 64 bytes comment and the STATE (~128k)
 *
 * direction == 0 save, 1 load.
 ****************************************************************************/
int ManageState (int direction)
{
  char filename[128];
  char action[80];
  int CardError;
  unsigned int SectorSize;
  int blocks;
  char comment[2][32] = { {"GnuboyGX 1.4"}, {"STATE"} };
  int outbytes = 0;
  int sbo;
  int state_size = 0;

  if (!gbromsize) return 0;

  if (direction) ShowAction ("Loading State ...");
  else ShowAction ("Saving State ...");

  /*** No idea what the damn ROM is, so just use CRC32 ***/
  sprintf (filename, "%s.gbz", rom.name);
  strcpy (comment[1], filename);

  memset(&savebuffer, 0, 0x30000);
	
  /* device is SDCARD, let's go */
  if (use_SDCARD) return SD_ManageFile(filename,direction,0);

  /* device is MCARD, we continue */
  if (direction == 0) /* Saving */
  {	
	/* Build the output buffer */
    memcpy(&savebuffer, &icon, 2048);
    memcpy(&savebuffer[2048], &comment[0], 64);
    state_size = savestate(&savebuffer[2112]);
  }

  outbytes = 2048 + 64 + state_size;

  /*** Initialise the CARD system ***/
  memset (&SysArea, 0, CARD_WORKAREA);
  CARD_Init ("GNUB", "01");

  /*** Attempt to mount the card ***/
  CardError = MountTheCard ();

  if (CardError)
  {
	  /*** Retrieve the sector size ***/
      CardError = CARD_GetSectorSize (CARDSLOT, &SectorSize);

      switch (direction)
	  {
		case 0: /*** Saving ***/
			/*** Determine number of blocks on this card ***/
	  		blocks = (outbytes / SectorSize) * SectorSize;
	  		if (outbytes % SectorSize) blocks += SectorSize;

			/*** Check if a previous save exists ***/
	  		if (CardFileExists (filename))
	    	{
	      		CardError = CARD_Open (CARDSLOT, filename, &CardFile);
	      		if (CardError)
				{
		  			sprintf (action, "Error Open : %d", CardError);
		  			WaitPrompt (action);
					CARD_Unmount (CARDSLOT);
					  return 0;
				}

				int size = CardFile.len;
				CARD_Close (&CardFile);

				if (size < blocks)
				{
					/* new size is bigger: check if there is enough space left */
				    CardError = CARD_Create (CARDSLOT, "TEMP", blocks-size, &CardFile);
					if (CardError)
					{
						sprintf (action, "Error Update : %d", CardError);
						WaitPrompt (action);
						CARD_Unmount (CARDSLOT);
						return 0;
					}
					CARD_Close (&CardFile);
					CARD_Delete(CARDSLOT, "TEMP");
				}

				/* always delete existing slot */
				CARD_Delete(CARDSLOT, filename);
		    }
			
			/*** Create a new slot ***/
	      	CardError = CARD_Create (CARDSLOT, filename, blocks, &CardFile);
			if (CardError)
			{
		  		sprintf (action, "Error create : %d %d", CardError, CARDSLOT);
		  		WaitPrompt (action);
				CARD_Unmount (CARDSLOT);
		 		 return 0;
			}
	  		
			/*** Continue and save ***/
			CARD_GetStatus (CARDSLOT, CardFile.filenum, &CardStatus);
			CardStatus.icon_addr = 0x0;
			CardStatus.icon_fmt = 2;
			CardStatus.icon_speed = 1;
			CardStatus.comment_addr = 2048;
			CARD_SetStatus (CARDSLOT, CardFile.filenum, &CardStatus);

			/*** And write the blocks out ***/
			sbo = 0;
			while (outbytes > 0)
	    	{
	      		CardError = CARD_Write (&CardFile, &savebuffer[sbo], SectorSize, sbo);
	      		outbytes -= SectorSize;
	      		sbo += SectorSize;
	    	}

	  		CARD_Close (&CardFile);
	  		CARD_Unmount (CARDSLOT);
			sprintf (action, "Saved %d bytes successfully", blocks);
	  		WaitPrompt (action);
	  		return 1;

		default: /*** Loading ***/
	  		if (!CardFileExists (filename))
	    	{
				WaitPrompt ("No Savestate Found");
				CARD_Unmount (CARDSLOT);
	            return 0;
	        }

	  	    memset (&CardFile, 0, sizeof (CardFile));
			CardError = CARD_Open (CARDSLOT, filename, &CardFile);
			if (CardError)
			{
				sprintf (action, "Error Open : %d", CardError);
				WaitPrompt (action);
				CARD_Unmount (CARDSLOT);
				return 0;
			}

			blocks = CardFile.len;
			if (blocks < SectorSize) blocks = SectorSize;
			if (blocks % SectorSize) blocks++;

			/*** Just read the file back in ***/
			sbo = 0;
			int size = blocks;
			while (blocks > 0)
			{
				CARD_Read (&CardFile, &savebuffer[sbo], SectorSize, sbo);
				sbo += SectorSize;
				blocks -= SectorSize;
			}
			CARD_Close (&CardFile);
			CARD_Unmount (CARDSLOT);

			loadstate(&savebuffer[2112]);

			/*** Inform user ***/
			sprintf (action, "Loaded %d bytes successfully", size);
			WaitPrompt (action);
			return 1;
		}
  }
  else WaitPrompt ("Unable to mount memory card");
  return 0;		/*** Signal failure ***/
}
