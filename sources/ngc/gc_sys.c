/*
 * Gnuboy SYS Support for the Nintendo Gamecube
 * Timer Support & LibOGC generic code by Sofdev (@tehskeen.com)
 * Copyright 2007 Eke-Eke
 * This file may be distributed under the terms of the GNU GPL.
 */
#include <sdcard.h>
#include "font.h"
#include "defs.h"
#include "mem.h"
#include "pcm.h"
#include "fb.h"
#include "hw.h"
#include "dvd.h"

extern int ConfigRequested;
int PADCAL = 70;

extern void legal ();
extern void MainMenu ();
extern int ManageSRAM (int direction);

/****************************************************************************
 ****************************************************************************
 * 
 * Gamecube Timer Support (actually unused)
 *
 ****************************************************************************
 ****************************************************************************/

#define TB_CLOCK  40500000
#define mftb(rval) ({unsigned long u; do { \
         asm volatile ("mftbu %0" : "=r" (u)); \
         asm volatile ("mftb %0" : "=r" ((rval)->l)); \
         asm volatile ("mftbu %0" : "=r" ((rval)->u)); \
         } while(u != ((rval)->u)); })


unsigned long tb_diff_msec(tb_t *end, tb_t *start)
{
	unsigned long upper, lower;
	upper = end->u - start->u;
	if (start->l > end->l) upper--;
	lower = end->l - start->l;
	return ((upper*((unsigned long)0x80000000/(TB_CLOCK/2000))) + (lower/(TB_CLOCK/1000)));
}

void *sys_timer()
{
	tb_t *c1;
	c1 = malloc(sizeof *c1);
	mftb(c1);
	return c1;
}

int sys_elapsed(tb_t *c1)
{
	tb_t now;
	int usecs;

	mftb(&now);
	usecs = tb_diff_msec( &now, c1 ) * 1000;
	*c1 = now;
	return usecs;
}

void sys_sleep(int us)
{
	tb_t start;
	tb_t now;
	if (us <= 0) return;
	mftb(&start);
	mftb(&now);
	while(tb_diff_msec( &now, &start ) < (us/1000)) mftb(&now);
}

/****************************************************************************
 ****************************************************************************
 * 
 * Gamecube VIDEO Support
 *
 ****************************************************************************
 ****************************************************************************/

/*** 2D Video ***/
unsigned int *xfb[2];	/*** Double buffered ***/
int whichfb = 0;		/*** Switch ***/
GXRModeObj *vmode;		/*** General video mode ***/

/*** GX ***/
#define TEX_WIDTH 640
#define TEX_HEIGHT 480
#define TEXSIZE ( (TEX_WIDTH * TEX_HEIGHT) * 2 )
#define DEFAULT_FIFO_SIZE 256 * 1024
#define HASPECT 80
#define VASPECT 60

static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
static u8 texturemem[TEXSIZE] ATTRIBUTE_ALIGN (32);
GXTexObj texobj;
static Mtx view;
int vwidth, vheight, oldvwidth, oldvheight;

/* New texture based scaler */
typedef struct tagcamera
{
  Vector pos;
  Vector up;
  Vector view;
} camera;

/* Square Matrix
   This structure controls the size of the image on the screen.
   Think of the output as a -80 x 80 by -60 x 60 graph.
 */
s16 square[] ATTRIBUTE_ALIGN (32) =
{
	/*
	 * X,   Y,  Z
	 * Values set are for roughly 4:3 aspect
	 */
	-HASPECT, VASPECT, 0,	// 0
	HASPECT, VASPECT, 0,	// 1
	HASPECT, -VASPECT, 0,	// 2
	-HASPECT, -VASPECT, 0,	// 3
};

static camera cam = { {0.0F, 0.0F, 0.0F},
{0.0F, 0.5F, 0.0F},
{0.0F, 0.0F, -0.5F}
};

/****************************************************************************
 * framestart
 *
 * Simply increment the tick counter
 ****************************************************************************/
int frameticker = 0;
static void framestart()
{
	frameticker++;
}

/****************************************************************************
 * Scaler Support Functions
 ****************************************************************************/
static void draw_init (void)
{
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));
  GX_SetNumTexGens (1);
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  GX_InvalidateTexAll ();
  GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
}

static void draw_vert (u8 pos, u8 c, f32 s, f32 t)
{
  GX_Position1x8 (pos);
  GX_Color1x8 (c);
  GX_TexCoord2f32 (s, t);
}

static void draw_square (Mtx v)
{
  Mtx m;  // model matrix.
  Mtx mv; // modelview matrix.

  guMtxIdentity (m);
  guMtxTransApply (m, m, 0, 0, -100);
  guMtxConcat (v, m, mv);
  GX_LoadPosMtxImm (mv, GX_PNMTX0);
  GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
  draw_vert (0, 0, 0.0, 0.0);
  draw_vert (1, 0, 1.0, 0.0);
  draw_vert (2, 0, 1.0, 1.0);
  draw_vert (3, 0, 0.0, 1.0);
  GX_End ();
}

/****************************************************************************
 * StartGX
 *
 * This function initialises the GX.
 * Based on texturetest from libOGC examples.
 ****************************************************************************/
void StartGX (void)
{
  Mtx p;

  GXColor gxbackground = { 0, 0, 0, 0xff };

	/*** Clear out FIFO area ***/
  memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
  GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
  GX_SetCopyClear (gxbackground, 0x00ffffff);

  GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
  GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
  GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
  GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE,
		    vmode->vfilter);
  GX_SetFieldMode (vmode->field_rendering,
		   ((vmode->viHeight ==
		     2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetCullMode (GX_CULL_NONE);
  GX_CopyDisp (xfb[whichfb ^ 1], GX_TRUE);
  GX_SetDispCopyGamma (GX_GM_1_0);

  guPerspective (p, 60, 1.33F, 10.0F, 1000.0F);
  GX_LoadProjectionMtx (p, GX_PERSPECTIVE);
  memset (texturemem, 0, TEX_WIDTH * TEX_HEIGHT * 2);
  vwidth = 100;
  vheight = 100;
}

/****************************************************************************
 * InitGCVideo
 *
 * This function MUST be called at startup.
 ****************************************************************************/
u8 isWII = 0;

void InitGCVideo ()
{
  /*
   * Before doing anything else under libogc,
   * Call VIDEO_Init
   */

  VIDEO_Init ();

  PAD_Init ();

  /*
   * Reset the video mode
   * This is always set to 60hz
   * Whether your running PAL or NTSC
   */
  vmode = &TVNtsc480IntDf;
  VIDEO_Configure (vmode);

  /*** Now configure the framebuffer. 
	     Really a framebuffer is just a chunk of memory
	     to hold the display line by line.
   **/

  xfb[0] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(vmode));

  /*** I prefer also to have a second buffer for double-buffering.
	     This is not needed for the console demo.
   ***/
  xfb[1] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(vmode));

  /*** Define a console ***/
  console_init(xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * 2);

  /*** Clear framebuffer to black ***/
  VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

  /*** Set the framebuffer to be displayed at next VBlank ***/
  VIDEO_SetNextFramebuffer(xfb[0]);

  /*** Increment frameticker and timer ***/
  VIDEO_SetPreRetraceCallback(framestart);

  /*** Get the PAD status updated by libogc ***/
  VIDEO_SetPostRetraceCallback(PAD_ScanPads);
  VIDEO_SetBlack (FALSE);
  
  /*** Update the video for next vblank ***/
  VIDEO_Flush();

  VIDEO_WaitVSync(); /*** Wait for VBL ***/
  if (vmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

  DVD_Init ();
  SDCARD_Init ();
  unpackBackdrop ();
  init_font();
  StartGX ();

  /* Wii drive detection for 4.7Gb support */
  int driveid = dvd_inquiry();
  if ((driveid == 4) || (driveid == 6) || (driveid == 8)) isWII = 0;
  else isWII = 1;

}

/* Gnuboy FrameBuffer */
struct fb fb; 

/* Gnuboy Video Support Functions */
void vid_end()
{
  int h, w;

  long long int *dst = (long long int *) texturemem;
  long long int *src1 = (long long int *) fb.ptr;
  long long int *src2 = (long long int *) (fb.ptr + 1280);
  long long int *src3 = (long long int *) (fb.ptr + 2560);
  long long int *src4 = (long long int *) (fb.ptr + 3840);

  /* 
     gnuboy automatically scale the 160*144 display 
	 to the desired resolution (640*480)
     scale factor is set to 3 in lcd.c 
  */
  vwidth  = 640;
  vheight = 480;
  whichfb ^= 1;

  if ((oldvheight != vheight) || (oldvwidth != vwidth))
  {
	  /* Update scaling */
      oldvwidth = vwidth;
      oldvheight = vheight;
      draw_init ();
      memset (&view, 0, sizeof (Mtx));
      guLookAt (view, &cam.pos, &cam.up, &cam.view);
      GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
  }

  GX_InvVtxCache ();
  GX_InvalidateTexAll ();
  GX_SetTevOp (GX_TEVSTAGE0, GX_DECAL);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

  for (h = 0; h < vheight; h += 4)
  {
    for (w = 0; w < 160; w++)
	{
	  *dst++ = *src1++;
	  *dst++ = *src2++;
	  *dst++ = *src3++;
	  *dst++ = *src4++;
	}

    src1 += 480;
    src2 += 480;
    src3 += 480;
    src4 += 480;
  }

  DCFlushRange (texturemem, TEXSIZE);
  GX_SetNumChans (1);
  GX_LoadTexObj (&texobj, GX_TEXMAP0);
  draw_square (view);
  GX_DrawDone ();
  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetColorUpdate (GX_TRUE);
  GX_CopyDisp (xfb[whichfb], GX_TRUE);
  GX_Flush ();
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
}

void vid_init ()
{
  InitGCVideo ();

  fb.w = 640;
  fb.h = 480;
  fb.pelsize = 2;
  fb.pitch = 1280;
  fb.ptr = malloc (640 * 480 * 2);
  fb.enabled = 1;
  fb.dirty = 0;
  fb.indexed = 0;
  fb.cc[0].r = 3;
  fb.cc[0].l = 11;
  fb.cc[1].r = 2;
  fb.cc[1].l = 5;
  fb.cc[2].r = 3;
  fb.cc[2].l = 0;
}

void vid_reset()
{
	if (fb.ptr) memset(fb.ptr, 0x00, 640*480*2);
	fb.enabled = 1;
    fb.dirty = 0;
    fb.indexed = 0;
}  
	

void vid_setpal(int i, int r, int g, int b)
{}

void vid_begin()
{}


/****************************************************************************
 ****************************************************************************
 * 
 * Gamecube Audio Support
 *
 ****************************************************************************
 ****************************************************************************/
static u8 soundbuffer[2][3200] ATTRIBUTE_ALIGN(32);
static u8 mixbuffer[16384];
static int mixhead = 0;
static int mixtail = 0;
static int whichab = 0;
int IsPlaying = 0;

struct pcm pcm;

/*** Correct method for promoting signed 8 bit to signed 16 bit samples ***/
/*** Now mixes into mixbuffer, and no collision is possible ***/
void mix_audio8to16( void )
{
	/*** GNUBoy internal audio format is u8pcm 
 	     So use standard upscale. Clipping has already been performed
	     on the original sample, so no further work is required.
        ***/
	int i;
	u16 sample;
	u16 *src = (u16 *)pcm.buf;
	u32 *dst = (u32 *)mixbuffer;

	for( i = 0; i < pcm.pos; i+=2 )
	{
		sample = *src++;
		/*** Promote to 16 bit and invert sign ***/
		dst[mixhead++] = ( ( ( sample ^ 0x80 ) & 0xff ) << 8 ) | ( ( ( sample ^ 0x8000 ) & 0xff00 ) << 16);
		mixhead &= 0xfff;
	}
}

/* Old Conversion Function - less optimized than the one above
 * i'm keeping it for better understanding ;)
 */
void mix_audio16()
{
    int i;
	u32 *dst = (u32 *)mixbuffer;

	s16 l,r;

    for (i=0; i<pcm.pos; i+=2)
    {
		l = ((s16)pcm.buf[i] << 8) - 32768;
        r = ((s16)pcm.buf[i+1] << 8) - 32768;
		dst[mixhead++] = ((l << 16) & 0xFFFF0000) | (r & 0x0000FFFF);
		mixhead &= 0xfff;
    }
}


static int mixercollect( u8 *outbuffer, int len )
{
	u32 *dst = (u32 *)outbuffer;
	u32 *src = (u32 *)mixbuffer;
	int done = 0;

	/*** Always clear output buffer ***/
	memset(outbuffer, 0, len);

	while ( ( mixtail != mixhead ) && ( done < len ) )
	{
		*dst++ = src[mixtail++];
		mixtail &= 0xFFF;
		done += 4;
	}

	/*** Realign to 32 bytes for DMA ***/
	done &= ~0x1f;
	if ( !done ) return len >> 1;

	return done;
}

void AudioSwitchBuffers()
{
	int actuallen;
	AUDIO_StopDMA();
	if ( !ConfigRequested )
	{
		actuallen = mixercollect( soundbuffer[whichab], 3200 );
		DCFlushRange(soundbuffer[whichab], actuallen);
		AUDIO_InitDMA((u32)soundbuffer[whichab], actuallen);
		AUDIO_StartDMA();
		whichab ^= 1;
		IsPlaying = 1;
	}
	else IsPlaying = 0;
}

/* Gnuboy Audio Support Functions */
void pcm_init ()
{
  AUDIO_Init (NULL);
  AUDIO_SetDSPSampleRate (AI_SAMPLERATE_48KHZ);
  AUDIO_RegisterDMACallback (AudioSwitchBuffers);
  pcm.hz = 48000;
  pcm.len = 1600; /* 8-bits stereo samples */
  pcm.buf = malloc(pcm.len);
  pcm.stereo = 1;
}

void pcm_reset()
{
	memset(soundbuffer, 0, 3200*2);
    memset(mixbuffer, 0, 16384);
    if (pcm.buf)
	{
		pcm.pos = 0;
		memset(pcm.buf, 0, pcm.len);
	}
}

void pcm_close()
{
}

int pcm_submit()
{
  if (pcm.pos < pcm.len) return 1;
  mix_audio8to16();
  //mix_audio16();
  pcm.pos = 0;
      
  /* Restart Sound Processing if stopped */
  if (IsPlaying == 0) AudioSwitchBuffers ();
  
  return 1;
}

/****************************************************************************
 ****************************************************************************
 * 
 * Gamecube Input Support
 *
 ****************************************************************************
 ****************************************************************************
 *
 * IMPORTANT
 * If you change the padmap here, be sure to update confjoy to
 * reflect the changes - or confusion will ensue!
 *
 * DEFAULT MAPPING IS:
 *		Gameboy			Gamecube
 *		  A			   A
 *		  B		       B
 *		  START		   START
 *		  SELECT       Y
 *
 */
unsigned int gbpadmap[] = {
  PAD_A, PAD_B,
  PAD_SELECT, PAD_START,
  PAD_UP, PAD_DOWN,
  PAD_LEFT, PAD_RIGHT
};

unsigned short gcpadmap[] = {
  PAD_BUTTON_A, PAD_BUTTON_B,
  PAD_BUTTON_Y, PAD_BUTTON_START,
  PAD_BUTTON_UP, PAD_BUTTON_DOWN,
  PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT
};

extern u8 SILENT;
extern int CARDSLOT;
extern int use_SDCARD;
extern int gbromsize;

int GetJoys (int joynum)
{
  int i;
  signed char x, y;

  /* Save in MC Slot B, when L&R are pressed*/
   if (PAD_ButtonsHeld(0) == (PAD_TRIGGER_L | PAD_TRIGGER_R))
   {
       if (gbromsize > 0 && mbc.batt)
	   {
          ShowAction ("Saving SRAM & RTC ...");
		  SILENT = 1; /* this should be transparent to the user */
		  CARDSLOT = CARD_SLOTB;
		  use_SDCARD = 0;
	      ManageSRAM (0);
		  SILENT = 0;
       }
  }
  
  /* Check for menu request */
  if (PAD_ButtonsHeld(joynum) & PAD_TRIGGER_Z)
  {
    ConfigRequested = 1;
	return 0;
  }
  
  /* PAD Buttons */
  for (i = 0; i < 4; i++)
  {
    if (PAD_ButtonsHeld(joynum) & gcpadmap[i]) pad_press(gbpadmap[i]);
	else pad_release (gbpadmap[i]);
  }

  x = PAD_StickX (joynum);
  y = PAD_StickY (joynum);

  /* PAD Directions */
  if ((x >  PADCAL) || (PAD_ButtonsHeld(joynum) & gcpadmap[7])) pad_press (PAD_RIGHT);
  else pad_release (PAD_RIGHT);

  if ((x < -PADCAL) || (PAD_ButtonsHeld(joynum) & gcpadmap[6])) pad_press (PAD_LEFT);
  else pad_release (PAD_LEFT);

  if ((y >  PADCAL) || (PAD_ButtonsHeld(joynum) & gcpadmap[4])) pad_press (PAD_UP);
  else pad_release (PAD_UP);
  
  if ((y < -PADCAL) || (PAD_ButtonsHeld(joynum) & gcpadmap[5])) pad_press (PAD_DOWN);
  else pad_release (PAD_DOWN);

  return 1;
}

/* Gnuboy Input Support Function */
void ev_poll()
{
	/* Update inputs */
    GetJoys (0);

	if (ConfigRequested)
	{
		AUDIO_StopDMA ();
        IsPlaying = 0;
		MainMenu ();
        ConfigRequested = 0;
	}
}
