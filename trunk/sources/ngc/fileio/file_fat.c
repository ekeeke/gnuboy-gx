/*
 * file_fat.c
 * 
 *   generic FAT loading support
 *
 *   code by Eke-Eke (2008) 
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
 ********************************************************************************/

#include "defs.h"
#include "font.h"
#include "history.h"
#include "unzip.h"
#include "filesel.h"
#include "file_fat.h"

#include "ata.h"
#include "wkf.h"

/* current FAT directory */
static char fatdir[MAXPATHLEN];

/* current FAT device */
static int fat_type   = 0;
static int useHistory = 0;

#ifndef HW_RVL
extern const DISC_INTERFACE* WKF_slot;
#endif
extern const DISC_INTERFACE* IDEA_slot;
extern const DISC_INTERFACE* IDEB_slot;

/***************************************************************************
 * FAT_UpdateDir
 *
 * Update FAT current root directory
 ***************************************************************************/ 
int FAT_UpdateDir(int go_up)
{
  int size=0;
  char *test;
  char temp[1024];

  /* current directory doesn't change */
  if (strcmp(filelist[selection].filename,".") == 0) return 1;

  /* go up to parent directory */
  if (strcmp(filelist[selection].filename,"..") == 0)
  {
    /* determine last subdirectory namelength */
    sprintf(temp,"%s",fatdir);
    test= strtok(temp,"/");
    while (test != NULL)
    {
      size = strlen(test);
      test = strtok(NULL,"/");
    }

    /* remove last subdirectory name */
    size = strlen(fatdir) - size;
    fatdir[size-1] = 0;

    /* restore previous selector state */
    selection = old_selection;
    offset = old_offset;

    /* reset old selection */
    old_selection = 0;
    old_offset = 0;
  }
  else if (go_up)
  {
    /* root has no parent directory */
    return 0;
  }
  else
  {
    /* by default, simply append folder name */
    sprintf(fatdir, "%s%s/",fatdir, filelist[selection].filename);

    /* save current selector state */
    old_selection = selection;
    old_offset = offset;

    /* reset selection */
    selection = 0;
    offset = 0;
  }
  return 1;
}

/***************************************************************************
 * FAT_ParseDirectory
 *
 * List files into one FAT directory
 ***************************************************************************/ 
int FAT_ParseDirectory()
{
  int nbfiles = 0;
  char filename[MAXPATHLEN];
  char filename1[MAXPATHLEN];

  /* open directory */
  DIR* dir = opendir (fatdir);
  if (dir == NULL) 
  {
    sprintf(filename, "Error opening %s", fatdir);
    WaitPrompt (filename);
    return 0;
  }

  struct dirent *entry = readdir(dir);
  struct stat filestat;

  while ((entry != NULL)&& (nbfiles < MAXFILES) ){         // list entries
     if (strcmp(filename,".") != 0) {
        memset(&filelist[nbfiles], 0, sizeof (FILEENTRIES));    
        sprintf(filelist[nbfiles].filename,"%s", entry->d_name);         
        sprintf(filename1, "%s%s", fatdir, filelist[nbfiles].filename); 
        stat(filename1, &filestat);  
        filelist[nbfiles].length = filestat.st_size;
        filelist[nbfiles].flags  = (filestat.st_mode & S_IFDIR) ? 1 : 0;
        nbfiles++;
     }

     entry = readdir(dir);                                  // next entry
  }
  closedir(dir);

  /* Sort the file list */
  qsort(filelist, nbfiles, sizeof(FILEENTRIES), FileSortCallback);

  return nbfiles;
}

/****************************************************************************
 * FAT_LoadFile
 *
 * This function will load a BIN, SMD or ZIP file from DVD into the ROM buffer.
 * This functions return the actual size of data copied into the buffer
 *
 ****************************************************************************/ 
int FAT_LoadFile (u8 *buffer) 
{
  /* If loading from history then we need to setup a few more things. */
  if(useHistory)
  {  
    /* Get the parent folder for the file. */
    strncpy(fatdir, history.entries[selection].filepath, MAXJOLIET-1);
    fatdir[MAXJOLIET-1] = '\0';

    /* Get the length of the file. This has to be done
     * before calling LoadFile().  */
    char filepath[MAXJOLIET];
    struct stat filestat;
    snprintf(filepath, MAXJOLIET-1, "%s%s", history.entries[selection].filepath, history.entries[selection].filename);
    filepath[MAXJOLIET-1] = '\0';
    if(stat(filepath, &filestat) == 0)
    {
      filelist[selection].length = filestat.st_size;
    }
  }

  /* file size */
  int length = filelist[selection].length;

  if (length > 0)
  {
    /* Add/move the file to the top of the history. */
    if(!useHistory) history_add_file(fatdir, filelist[selection].filename);

    /* full filename */
    char fname[MAXPATHLEN];
    sprintf(fname, "%s%s",fatdir,filelist[selection].filename);

    /* open file */
    FILE *sdfile = fopen(fname, "rb");
    if (sdfile == NULL)
    {
      WaitPrompt ("Unable to open file!");
      haveFATdir = 0;
      return 0;
    }

    /* Read first data chunk */
    unsigned char temp[2048];
    fread(temp, 1, 2048, sdfile);
    fclose(sdfile);

    /* determine file type */
    if (!IsZipFile ((char *) temp))
    {
      /* re-open and read file */
      sdfile = fopen(fname, "rb");
      if (sdfile)
      {
        char msg[50];
        sprintf(msg,"Loading %d bytes...", length);
        ShowAction(msg);
        fread(buffer, 1, length, sdfile);
        fclose(sdfile);
        return length;
      }
    }
    else
    {
      /* unzip file */
      return UnZipBuffer (buffer, 0, fname);
    }
  }

  return 0;
}

/****************************************************************************
 * OpenFAT
 *
 * Function to load a FAT directory and display to user.
 ****************************************************************************/ 
int FAT_Open(int type, u8 *buffer)
{
  int max = 0;
  char root[10] = "";
  useFAT = 1;
  
  /* FAT header */
  if (type == TYPE_IDE)
  {
     if ( IDEA_slot->startup() && fatMountSimple("IDEA", IDEA_slot) ) sprintf (root, "IDEA:"); 
     else if ( IDEB_slot->startup() && fatMountSimple("IDEB", IDEB_slot) ) sprintf (root, "IDEB:"); 
     else { WaitPrompt ("IDE-EXI not initialized"); return 0; }
  }
#ifdef HW_RVL
  else if (type == TYPE_SD) sprintf (root, "sd:");
  else if (type == TYPE_USB) sprintf (root, "usb:");
#else
  else if (type == TYPE_WKF)
  {
     if ( WKF_slot->startup() && fatMountSimple("WKF", WKF_slot) )  sprintf (root, "WKF:");
     else { WaitPrompt ("WKF not initialized"); return 0; }
  }
#endif


  /* if FAT device type changed, reload filelist */
  if (fat_type != type) 
  {
    haveFATdir = 0;
  }
  fat_type = type;


  /* update filelist */
  if (haveFATdir == 0)
  {
    useHistory = 0;
    if (type == TYPE_RECENT)
    {
      /* fetch history list */
      useHistory = 1;
      int i;
      for(i=0; i < NUM_HISTORY_ENTRIES; i++)
      {
        if(history.entries[i].filepath[0] > 0)
        {
          filelist[i].offset = 0;
          filelist[i].length = 0;
          filelist[i].flags = 0;
          filelist[i].filename_offset = 0;
          strncpy(filelist[i].filename, history.entries[i].filename, MAXJOLIET-1);
          filelist[i].filename[MAXJOLIET-1] = '\0';
          max++;
        }
        else
        {
          /* Found the end of the list. */
          break;
        }
      }
    }
    else
    {
      /* reset root directory */
      sprintf (fatdir, "%s%s/roms/", root, DEFAULT_PATH);

      /* if directory doesn't exist, use root as default */
      DIR* dir = opendir(fatdir);
      if (dir == NULL) sprintf (fatdir, "%s/", root);
      else closedir(dir);

      /* parse root directory */
      ShowAction("Reading Directory ...");
      max = FAT_ParseDirectory ();
    }

    if (max)
    {
      /* FAT is default */
      haveFATdir = 1;
      haveDVDdir = 0;

      /* reset file selection */
      maxfiles      = max;
      offset        = 0;
      selection     = 0;
      old_offset    = 0;
      old_selection = 0;
      return FileSelector (buffer);
    }
    else
    {
      /* no entries found */
      WaitPrompt ("no files found !");
      return 0;
    }
  }

  return FileSelector (buffer);
}

