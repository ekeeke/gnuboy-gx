/*
 * Gnuboy SYS Support for the Nintendo Gamecube
 * Timer Support & LibOGC generic code by Sofdev (@tehskeen.com)
 * Copyright 2007 Eke-Eke
 * This file may be distributed under the terms of the GNU GPL.
 */
#include "defs.h"
#include "mem.h"
#include "pcm.h"
#include "fb.h"
#include "hw.h"
#include "config.h"
#include "font.h"
#include "history.h"

#include <fat.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <di/di.h>
#else
#include "dvd.h"
#endif

int Shutdown = 0;
BOOL fat_enabled;


static u8 ConfigRequested;

#ifdef HW_RVL
/* Power Button callback */
void Power_Off(void)
{
  Shutdown = 1;
  ConfigRequested = 1;
}
#endif


/* 576 lines interlaced (PAL 50Hz, scaled) */
GXRModeObj TV50hz_576i = 
{
  VI_TVMODE_PAL_INT,      // viDisplayMode
  640,             // fbWidth
  480,             // efbHeight
  574,             // xfbHeight
  (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
  (VI_MAX_HEIGHT_PAL - 574)/2,        // viYOrigin
  640,             // viWidth
  574,             // viHeight
  VI_XFBMODE_DF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
     8,         // line n-1
     8,         // line n-1
    10,         // line n
    12,         // line n
    10,         // line n
     8,         // line n+1
     8          // line n+1
  }
};

/****************************************************************************
 ****************************************************************************
 * 
 * Gamecube Timer Support (actually unused)
 *
 ****************************************************************************
 ****************************************************************************/
extern u32 diff_usec(long long start,long long end);
extern long long gettime();

void *sys_timer()
{
  return 0;
}

int sys_elapsed(tb_t *c1)
{
  return 0;
}

void sys_sleep(int us)
{
  usleep(us);
}

/****************************************************************************
 ****************************************************************************
 * 
 * Gamecube VIDEO Support
 *
 ****************************************************************************
 ****************************************************************************/

/*** 2D Video ***/
unsigned int *xfb[2];  /*** Double buffered ***/
int whichfb = 0;       /*** Switch ***/
GXRModeObj *vmode;     /*** General video mode ***/

/*** GX ***/
#define TEX_WIDTH 160
#define TEX_HEIGHT 144
#define TEXSIZE ( (TEX_WIDTH * TEX_HEIGHT) * 2 )
#define DEFAULT_FIFO_SIZE 256 * 1024
#define HASPECT 250 // original aspect, scaled to fit screen height
#define VASPECT 224

static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
static u8 texturemem[TEXSIZE] ATTRIBUTE_ALIGN (32);
GXTexObj texobj;
static Mtx view;
int vwidth, vheight, oldvwidth, oldvheight;


typedef struct tagcamera
{
  guVector pos;
  guVector up;
  guVector view;
} camera;

/*** Square Matrix
     This structure controls the size of the image on the screen.
     Think of the output as a -80 x 80 by -60 x 60 graph.
***/
s16 square[] ATTRIBUTE_ALIGN (32) =
{
  /*
   * X,   Y,  Z
   * Values set are for roughly 4:3 aspect
  */
  -HASPECT,  VASPECT, 0,// 0
   HASPECT,  VASPECT, 0,// 1
   HASPECT, -VASPECT, 0,// 2
  -HASPECT, -VASPECT, 0,// 3
};

static camera cam =
{
  {0.0F,  0.0F, -100.0F},
  {0.0F, -1.0F,    0.0F},
  {0.0F,  0.0F,    0.0F}
};

/****************************************************************************
 * Scaler Support Functions
 ****************************************************************************/
/* init rendering */
/* should be called each time you change quad aspect ratio */
void draw_init (void)
{
  /* Clear all Vertex params */
  GX_ClearVtxDesc ();

  /* Set Position Params (set quad aspect ratio) */
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
  GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

  /* Set Tex Coord Params */
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
  GX_SetNumTexGens (1);
  GX_SetNumChans(0);

  /** Set Modelview **/
  memset (&view, 0, sizeof (Mtx));
  guLookAt(view, &cam.pos, &cam.up, &cam.view);
  GX_LoadPosMtxImm (view, GX_PNMTX0);
}

/* vertex rendering */
static void draw_vert (u8 pos, f32 s, f32 t)
{
  GX_Position1x8 (pos);
  GX_TexCoord2f32 (s, t);
}

/* textured quad rendering */
static void draw_square ()
{
  GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
  draw_vert (3, 0.0, 0.0);
  draw_vert (2, 1.0, 0.0);
  draw_vert (1, 1.0, 1.0);
  draw_vert (0, 0.0, 1.0);
  GX_End ();
}

/****************************************************************************
 * StartGX
 *
 * This function initialises the GX.
 * Based on texturetest from libOGC examples.
 ****************************************************************************/
/* initialize GX rendering */
static void StartGX (void)
{
  Mtx p;
  GXColor gxbackground = { 0, 0, 0, 0xff };

  /*** Clear out FIFO area ***/
  memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);

  /*** Initialise GX ***/
  GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
  GX_SetCopyClear (gxbackground, 0x00ffffff);
  GX_SetViewport (0.0F, 0.0F, vmode->fbWidth, vmode->efbHeight, 0.0F, 1.0F);
  GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
  f32 yScale = GX_GetYScaleFactor(vmode->efbHeight, vmode->xfbHeight);
  u16 xfbHeight = GX_SetDispCopyYScale (yScale);
  GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopyDst (vmode->fbWidth, xfbHeight);
  GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
  GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetCullMode (GX_CULL_NONE);
  GX_SetDispCopyGamma (GX_GM_1_0);
  GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_TRUE);
  GX_SetColorUpdate (GX_TRUE);
  guOrtho(p, vmode->efbHeight/2, -(vmode->efbHeight/2), -(vmode->fbWidth/2), vmode->fbWidth/2, 100, 1000);
  GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

  /*** Copy EFB -> XFB ***/
  GX_CopyDisp (xfb[whichfb ^ 1], GX_TRUE);

  /*** Initialize texture data ***/
  memset (texturemem, 0, TEX_WIDTH * TEX_HEIGHT * 2);

  vwidth = 100;
  vheight = 100;
}

/* PreRetrace handler */
int frameticker = 0;
static void framestart()
{
  /* simply increment the tick counter */
  frameticker++;
}

/****************************************************************************
 * InitGCVideo
 *
 * This function MUST be called at startup.
 ****************************************************************************/
extern GXRModeObj TVEurgb60Hz480IntDf;

u8 gc_pal;
void InitGCVideo ()
{
#ifdef HW_RVL
  /* initialize Wii DVD interface first */
  DI_Init();
#endif

  
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
  vmode = VIDEO_GetPreferredMode(NULL);

  /* adjust display settings */
  switch (vmode->viTVMode >> 2)
  {
    case VI_PAL:
      /* display should be centered vertically (borders) */
      vmode = &TVPal576IntDfScale;
      vmode->xfbHeight = 480;
      vmode->viYOrigin = (VI_MAX_HEIGHT_PAL - 480)/2;
      vmode->viHeight = 480;

      /* 50Hz */
      gc_pal = 1;
      break;
    
    default:
      gc_pal = 0;
#ifndef HW_RVL
      /* force 480p on NTSC GameCube if the Component Cable is present */
      int retPAD = 0;
      while(retPAD <= 0) { retPAD = PAD_ScanPads(); usleep(100); }
      if(VIDEO_HaveComponentCable() && !(PAD_ButtonsDown(0) & PAD_TRIGGER_L)) vmode = &TVNtsc480Prog;
#endif
      break;
  }

#ifdef HW_RVL
  /* Widescreen fix */
  if( CONF_GetAspectRatio() )
  {
    vmode->viWidth    = 678;
    vmode->viXOrigin  = (VI_MAX_WIDTH_NTSC - 678)/2;
  }
#endif

  VIDEO_Configure (vmode);

  /* Configure the framebuffers (double-buffering) */
  xfb[0] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(&TV50hz_576i));
  xfb[1] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(&TV50hz_576i));

  /*** Define a console ***/
  console_init(xfb[0], 20, 64, 640, 574, 574 * 2);

  /*** Clear framebuffer to black ***/
  VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

  /*** Set the framebuffer to be displayed at next VBlank ***/
  VIDEO_SetNextFramebuffer(xfb[0]);

  /*** Increment frameticker and timer ***/
  VIDEO_SetPreRetraceCallback(framestart);

  /*** Get the PAD status updated by libogc ***/
  VIDEO_SetBlack (FALSE);
  
  /*** Update the video for next vblank ***/
  VIDEO_Flush();

  VIDEO_WaitVSync(); /*** Wait for VBL ***/
  VIDEO_WaitVSync();

#ifdef HW_RVL
  WPAD_Init();
  WPAD_SetIdleTimeout(60);
  WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
  WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
#else 
  dvd_motor_off();               // lets stop motor on startup to allow DVD swapping if required
#endif

#ifdef HW_RVL
  /* Power Button callback */
  SYS_SetPowerCallback(Power_Off);
#endif

  /* Initialize FAT Interface */
  if (fatInitDefault() == true)  fat_enabled = 1; 

  /* Restore Recent Files list */
  set_history_defaults();
  history_load();

  unpackBackdrop ();
  init_font();
  StartGX ();
}

/* Gnuboy FrameBuffer */
struct fb fb; 

/* Gnuboy Video Support Functions */
void vid_end()
{
  int h, w;

  long long int *dst = (long long int *) texturemem;
  long long int *src1 = (long long int *) fb.ptr;
  long long int *src2 = (long long int *) (fb.ptr + 320);
  long long int *src3 = (long long int *) (fb.ptr + 640);
  long long int *src4 = (long long int *) (fb.ptr + 960);

  vwidth  = 160;
  vheight = 144;
  whichfb ^= 1;

  if ((oldvheight != vheight) || (oldvwidth != vwidth))
  {
      /* Update scaling */
      oldvwidth = vwidth;
      oldvheight = vheight;
      draw_init ();
      
      /* reinitialize texture */
      GX_InvalidateTexAll ();
      GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
  }

  /* Invalidate memory */
  GX_InvVtxCache ();
  GX_InvalidateTexAll ();

  for (h = 0; h < vheight; h += 4)
  {
    for (w = 0; w < 40; w++)
    {
      *dst++ = *src1++;
      *dst++ = *src2++;
      *dst++ = *src3++;
      *dst++ = *src4++;
    }

    src1 += 120;
    src2 += 120;
    src3 += 120;
    src4 += 120;
  }

  /* load texture into GX */
  DCFlushRange (texturemem, vwidth * vheight * 2);
  GX_LoadTexObj (&texobj, GX_TEXMAP0);
  
  /* render textured quad */
  draw_square ();
  GX_DrawDone ();

  /* switch external framebuffers then copy EFB to XFB */
  whichfb ^= 1;
  GX_CopyDisp (xfb[whichfb], GX_TRUE);
  GX_Flush ();

  /* set next XFB */
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
}

void vid_init ()
{
  InitGCVideo ();

  fb.w = 160;
  fb.h = 144;
  fb.pelsize = 2;
  fb.pitch = 320;
  fb.ptr = malloc (fb.pitch*fb.h);
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
  if (fb.ptr) memset(fb.ptr, 0x00, fb.pitch*fb.h);
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
static u8 pcm_buffer[1600]; /* hold 8bits stereo samples for one frame */
static u8 soundbuffer[2][3200] ATTRIBUTE_ALIGN(32);
static u8 mixbuffer[16000];
static int mixhead = 0;
static int mixtail = 0;
static int whichab = 0;
int IsPlaying = 0;

struct pcm pcm;


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
    if (mixtail == 4000) mixtail = 0;
    done += 4;
  }

  /*** Realign to 32 bytes for DMA ***/
  mixtail -= ((done&0x1f)>>2);
  if (mixtail < 0) mixtail += 4000;
  done &= ~0x1f;
  if ( !done ) return len >> 1;

  return done;
}

void AudioSwitchBuffers()
{
  int actuallen;
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
  pcm.buf = &pcm_buffer[0];
  pcm.stereo = 1;
}

void pcm_reset()
{
  memset(soundbuffer, 0, 3200*2);
  memset(mixbuffer, 0, 16000);
  pcm.pos = 0;
  memset(pcm.buf, 0, pcm.len);
}

void pcm_close()
{
}

int pcm_submit()
{
  /* Correct method for promoting signed 8 bit to signed 16 bit samples 
      GNUBoy internal audio format is u8pcm 
      So use standard upscale. Clipping has already been performed
      on the original sample, so no further work is required.
   */
  int i;
  u16 sample;
  u16 *src = (u16 *)pcm.buf;
  u32 *dst = (u32 *)mixbuffer;

  for( i = 0; i < pcm.pos; i+=2 )
  {
    sample = *src++;
    
    /*** Promote to 16 bit and invert sign ***/
    dst[mixhead++] = ( ( ( sample ^ 0x80 ) & 0xff ) << 8 ) | ( ( ( sample ^ 0x8000 ) & 0xff00 ) << 16);
    if (mixhead == 4000) mixhead = 0;
  }
  
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
 *    Gameboy      Gamecube
 *      A         A
 *      B           B
 *      START       START
 *      SELECT       Y
 *
 */
static u32 gbpadmap[8] =
{
  PAD_A, PAD_B,
  PAD_SELECT, PAD_START,
  PAD_UP, PAD_DOWN,
  PAD_LEFT, PAD_RIGHT
};

static u16 padmap[8] =
{
  PAD_BUTTON_A, PAD_BUTTON_B,
  PAD_BUTTON_Y, PAD_BUTTON_START,
  PAD_BUTTON_UP, PAD_BUTTON_DOWN,
  PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT
};

extern void MainMenu ();
extern void memfile_autosave();
extern int Shutdown;

int PADCAL = 30;

#ifdef HW_RVL

#define MAX_HELD_CNT 15
static u32 held_cnt = 0;

static u32 wpadmap[3][8] =
{
  {
    WPAD_BUTTON_2, WPAD_BUTTON_1,
    WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS,
    WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT,
    WPAD_BUTTON_UP, WPAD_BUTTON_DOWN
  },
  {
    WPAD_BUTTON_A, WPAD_BUTTON_B,
    WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS,
    WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
    WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT
  },
  {
    WPAD_CLASSIC_BUTTON_A, WPAD_CLASSIC_BUTTON_B,
    WPAD_CLASSIC_BUTTON_MINUS, WPAD_CLASSIC_BUTTON_PLUS,
    WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_DOWN,
    WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_RIGHT
  }
};

#define PI 3.14159265f

static s8 WPAD_StickX(u8 chan,u8 right)
{
  float mag = 0.0;
  float ang = 0.0;
  WPADData *data = WPAD_Data(chan);

  switch (data->exp.type)
  {
    case WPAD_EXP_NUNCHUK:
    case WPAD_EXP_GUITARHERO3:
      if (right == 0)
      {
        mag = data->exp.nunchuk.js.mag;
        ang = data->exp.nunchuk.js.ang;
      }
      break;

    case WPAD_EXP_CLASSIC:
      if (right == 0)
      {
        mag = data->exp.classic.ljs.mag;
        ang = data->exp.classic.ljs.ang;
      }
      else
      {
        mag = data->exp.classic.rjs.mag;
        ang = data->exp.classic.rjs.ang;
      }
      break;

    default:
      break;
  }

  /* calculate X value (angle need to be converted into radian) */
  if (mag > 1.0) mag = 1.0;
  else if (mag < -1.0) mag = -1.0;
  double val = mag * sin(PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}


static s8 WPAD_StickY(u8 chan, u8 right)
{
  float mag = 0.0;
  float ang = 0.0;
  WPADData *data = WPAD_Data(chan);

  switch (data->exp.type)
  {
    case WPAD_EXP_NUNCHUK:
    case WPAD_EXP_GUITARHERO3:
      if (right == 0)
      {
        mag = data->exp.nunchuk.js.mag;
        ang = data->exp.nunchuk.js.ang;
      }
      break;

    case WPAD_EXP_CLASSIC:
      if (right == 0)
      {
        mag = data->exp.classic.ljs.mag;
        ang = data->exp.classic.ljs.ang;
      }
      else
      {
        mag = data->exp.classic.rjs.mag;
        ang = data->exp.classic.rjs.ang;
      }
      break;

    default:
      break;
  }

  /* calculate X value (angle need to be converted into radian) */
  if (mag > 1.0) mag = 1.0;
  else if (mag < -1.0) mag = -1.0;
  double val = mag * cos(PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}
#endif

u16 getMenuButtons(void)
{
#ifdef HW_RVL
    if (Shutdown)
    {
      /* autosave SRAM/State */
      memfile_autosave();

      /* shutdown Wii */
      DI_Close();
      SYS_ResetSystem(SYS_POWEROFF, 0, 0);
    }
#endif

  /* slowdown input updates */
  VIDEO_WaitVSync();

  /* get gamepad inputs */
  PAD_ScanPads();
  u16 p = PAD_ButtonsDown(0);
  s8 x  = PAD_StickX(0);
  s8 y  = PAD_StickY(0);
  if (x > 70) p |= PAD_BUTTON_RIGHT;
  else if (x < -70) p |= PAD_BUTTON_LEFT;
  if (y > 60) p |= PAD_BUTTON_UP;
  else if (y < -60) p |= PAD_BUTTON_DOWN;

#ifdef HW_RVL
  /* get wiimote + expansions inputs */
  WPAD_ScanPads();
  u32 q = WPAD_ButtonsDown(0);
  u32 h = WPAD_ButtonsHeld(0);
  x = WPAD_StickX(0, 0);
  y = WPAD_StickY(0, 0);

  /* is Wiimote directed toward screen (horizontal/vertical orientation) ? */
  struct ir_t ir;
  WPAD_IR(0, &ir);

  /* wiimote directions */
  if (q & WPAD_BUTTON_UP)         p |= ir.valid ? PAD_BUTTON_UP : PAD_BUTTON_LEFT;
  else if (q & WPAD_BUTTON_DOWN)  p |= ir.valid ? PAD_BUTTON_DOWN : PAD_BUTTON_RIGHT;
  else if (q & WPAD_BUTTON_LEFT)  p |= ir.valid ? PAD_BUTTON_LEFT : PAD_BUTTON_DOWN;
  else if (q & WPAD_BUTTON_RIGHT) p |= ir.valid ? PAD_BUTTON_RIGHT : PAD_BUTTON_UP;
  
  if (h & WPAD_BUTTON_UP)
  {
    held_cnt ++;
    if (held_cnt == MAX_HELD_CNT)
    {
      held_cnt = MAX_HELD_CNT - 2;
      p |= ir.valid ? PAD_BUTTON_UP : PAD_BUTTON_LEFT;
    }
  }
  else if (h & WPAD_BUTTON_DOWN)
  {
    held_cnt ++;
    if (held_cnt == MAX_HELD_CNT)
    {
      held_cnt = MAX_HELD_CNT - 2;
      p |= ir.valid ? PAD_BUTTON_DOWN : PAD_BUTTON_RIGHT;
    }
  }
  else if (h & WPAD_BUTTON_LEFT)
  {
    held_cnt ++;
    if (held_cnt == MAX_HELD_CNT)
    {
      held_cnt = MAX_HELD_CNT - 2;
      p |= ir.valid ? PAD_BUTTON_LEFT : PAD_BUTTON_DOWN;
    }
  }
  else if (h & WPAD_BUTTON_RIGHT)
  {
    held_cnt ++;
    if (held_cnt == MAX_HELD_CNT)
    {
      held_cnt = MAX_HELD_CNT - 2;
      p |= ir.valid ? PAD_BUTTON_RIGHT : PAD_BUTTON_UP;
    }
  }
  else
  {
    held_cnt = 0;
  }

  /* analog sticks */
  if (y > 70)       p |= PAD_BUTTON_UP;
  else if (y < -70) p |= PAD_BUTTON_DOWN;
  if (x < -60)      p |= PAD_BUTTON_LEFT;
  else if (x > 60)  p |= PAD_BUTTON_RIGHT;

  /* classic controller directions */
  if (q & WPAD_CLASSIC_BUTTON_UP)         p |= PAD_BUTTON_UP;
  else if (q & WPAD_CLASSIC_BUTTON_DOWN)  p |= PAD_BUTTON_DOWN;
  if (q & WPAD_CLASSIC_BUTTON_LEFT)       p |= PAD_BUTTON_LEFT;
  else if (q & WPAD_CLASSIC_BUTTON_RIGHT) p |= PAD_BUTTON_RIGHT;

  /* wiimote keys */
  if (q & WPAD_BUTTON_MINUS)  p |= PAD_TRIGGER_L;
  if (q & WPAD_BUTTON_PLUS)   p |= PAD_TRIGGER_R;
  if (q & WPAD_BUTTON_A)      p |= PAD_BUTTON_A;
  if (q & WPAD_BUTTON_B)      p |= PAD_BUTTON_B;
  if (q & WPAD_BUTTON_2)      p |= PAD_BUTTON_A;
  if (q & WPAD_BUTTON_1)      p |= PAD_BUTTON_B;
  if (q & WPAD_BUTTON_HOME)   p |= PAD_TRIGGER_Z;

  /* classic controller keys */
  if (q & WPAD_CLASSIC_BUTTON_FULL_L) p |= PAD_TRIGGER_L;
  if (q & WPAD_CLASSIC_BUTTON_FULL_R) p |= PAD_TRIGGER_R;
  if (q & WPAD_CLASSIC_BUTTON_A)      p |= PAD_BUTTON_A;
  if (q & WPAD_CLASSIC_BUTTON_B)      p |= PAD_BUTTON_B;
  if (q & WPAD_CLASSIC_BUTTON_HOME)   p |= PAD_TRIGGER_Z;

 #endif

  return p;
}

int update_input()
{
  int i;
  s8 x, y;
  u32 p;

  /* update PAD status */
  PAD_ScanPads();
  p = PAD_ButtonsHeld(0);
  x = PAD_StickX (0);
  y = PAD_StickY (0);

  /* Check for menu request */
  if (p & PAD_TRIGGER_L)   // Exit Emulator and Return to Menu
  {
    ConfigRequested = 1;
    return 0;
  }
  
  /* PAD Buttons */
  for (i = 0; i < 4; i++)
  {
    if (p & padmap[i]) pad_press(gbpadmap[i]);
    else pad_release (gbpadmap[i]);
  }

  /* PAD Directions */
  if ((x >  PADCAL) || (p & padmap[7])) pad_press (PAD_RIGHT);
  else pad_release (PAD_RIGHT);

  if ((x < -PADCAL) || (p & padmap[6])) pad_press (PAD_LEFT);
  else pad_release (PAD_LEFT);

  if ((y >  PADCAL) || (p & padmap[4])) pad_press (PAD_UP);
  else pad_release (PAD_UP);
  
  if ((y < -PADCAL) || (p & padmap[5])) pad_press (PAD_DOWN);
  else pad_release (PAD_DOWN);

#ifdef HW_RVL
  u32 exp;

  /* update WPAD status */
  WPAD_ScanPads();
  if (WPAD_Probe(0, &exp) == WPAD_ERR_NONE)
  {
    p = WPAD_ButtonsHeld(0);
    x = WPAD_StickX(0, 0);
    y = WPAD_StickY(0, 0);

      if ((p & WPAD_BUTTON_HOME) || (p & WPAD_CLASSIC_BUTTON_HOME))
      {
        ConfigRequested = 1;
        return 0;
      }

      /* PAD Buttons */
      for (i = 0; i < 4; i++)
      {
        if (p & wpadmap[exp][i]) pad_press(gbpadmap[i]);
        else pad_release (gbpadmap[i]);
      }

      /* PAD Directions */
      if ((x >  PADCAL) || (p & wpadmap[exp][7])) pad_press (PAD_RIGHT);
      else pad_release (PAD_RIGHT);

      if ((x < -PADCAL) || (p & wpadmap[exp][6])) pad_press (PAD_LEFT);
      else pad_release (PAD_LEFT);

      if ((y >  PADCAL) || (p & wpadmap[exp][4])) pad_press (PAD_UP);
      else pad_release (PAD_UP);
      
      if ((y < -PADCAL) || (p & wpadmap[exp][5])) pad_press (PAD_DOWN);
      else pad_release (PAD_DOWN);
  }
#endif

  return 1;
}

extern u32 diff_usec(long long start,long long end);
extern long long gettime();
long long now, prev;

/* Event polling */
void ev_poll()
{
  /* update inputs */
  update_input();

  /* goto menu */
  if (ConfigRequested)
  {
    AUDIO_StopDMA ();
    IsPlaying = 0;
    MainMenu ();
    ConfigRequested = 0;
   
    /* reset frame timings */
    frameticker = 0;
    prev = gettime();
  }

  /* Frame Synchronization */
  if (gc_pal)
  {
    /* PAL 50Hz: use timer */
    now = gettime();
    while (diff_usec(prev, now) < 16666) now = gettime();
    prev = now;
  }
  else
  {
    while (frameticker == 0) usleep(50);
    frameticker--;
  }
}
