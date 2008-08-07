/****************************************************************************
* This part has been adapted from HugoGC
* Original code by Softdev (@tehskeen.com)
* Adapted for Gnuboy Port by Eke-Eke (@tehskeen.com)
* ****************************************************************************/
/*****************************************************************************
 * IPL FONT Engine
 *
 * Based on Qoob MP3 Player Font
 * Added IPL font extraction
 *****************************************************************************/

extern void init_font(void);
extern void WriteCentre_HL( int y, const unsigned char *string);
extern void WriteCentre (int y, const unsigned char *text);
extern void write_font (int x, int y, const unsigned char *text);
extern void WaitPrompt (char *msg);
extern void ShowAction (char *msg);
extern void WaitButtonA ();
extern void unpackBackdrop ();
extern void ClearScreen ();
extern void SetScreen ();
extern void fntDrawBoxFilled (int x1, int y1, int x2, int y2, int color);
extern int fheight;
extern int font_size[256];
extern u16 back_framewidth;
