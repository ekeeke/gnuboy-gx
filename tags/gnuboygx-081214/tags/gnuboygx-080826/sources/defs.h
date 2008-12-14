#ifndef __DEFS_H__
#define __DEFS_H__

typedef unsigned char byte;
typedef unsigned char un8;
typedef unsigned short un16;
typedef unsigned int un32;
typedef signed char n8;
typedef signed short n16;
typedef signed int n32;
typedef un16 word;
typedef word addr;

#ifndef NGC
#define NGC 1
#endif

#ifdef NGC
#ifdef IS_LITTLE_ENDIAN
#undef IS_LITTLE_ENDIAN
#endif

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <malloc.h>
#include <math.h>
#include <limits.h>
#include <zlib.h>

typedef struct
{
	unsigned long l, u;
} tb_t;

/* sys */
void vid_init();
void vid_reset();
void vid_begin();
void vid_end();
void vid_setpal(int i, int r, int g, int b);
void pcm_init();
void pcm_reset();
void pcm_close();
int pcm_submit();
void ev_poll();
void *sys_timer();
int sys_elapsed(tb_t *p);
void sys_sleep(int us);

/* emu.c */
void emu_init();
void emu_reset();
void emu_run();

/* loader.c */
int rom_load();
void loader_unload();

/* palette.c */
void pal_set332();
void pal_expire();
void pal_release(byte n);
byte pal_getcolor(int c, int r, int g, int b);

/* refresh.c */
void refresh_1(byte *dest, byte *src, byte *pal, int cnt);
void refresh_2(un16 *dest, byte *src, un16 *pal, int cnt);
void refresh_3(byte *dest, byte *src, un32 *pal, int cnt);
void refresh_4(un32 *dest, byte *src, un32 *pal, int cnt);
void refresh_1_2x(byte *dest, byte *src, byte *pal, int cnt);
void refresh_2_2x(un16 *dest, byte *src, un16 *pal, int cnt);
void refresh_3_2x(byte *dest, byte *src, un32 *pal, int cnt);
void refresh_4_2x(un32 *dest, byte *src, un32 *pal, int cnt);
void refresh_2_3x(un16 *dest, byte *src, un16 *pal, int cnt);
void refresh_3_3x(byte *dest, byte *src, un32 *pal, int cnt);
void refresh_4_3x(un32 *dest, byte *src, un32 *pal, int cnt);
void refresh_3_4x(byte *dest, byte *src, un32 *pal, int cnt);
void refresh_4_4x(un32 *dest, byte *src, un32 *pal, int cnt);

/* hw.c */
void hw_interrupt(byte i, byte mask);
void hw_dma(byte b);
void hw_hdma_cmd(byte c);
void hw_hdma();
void pad_refresh();
void pad_press(byte k);
void pad_release(byte k);
void pad_set(byte k, int st);
void hw_reset();

/* lcd.c */
void lcd_reset();
void lcd_begin();
void lcd_refreshline();
void pal_write_dmg(int i, int mapnum, byte d);
void pal_write(int i, byte b);
void pal_dirty();
void vram_write(int a, byte b);
void vram_dirty();

/* lcdc.c */
void lcdc_trans();
void lcdc_change(byte b);
void stat_write(byte b);
void stat_trigger();

/* cpu.c */
void cpu_timers(int cnt);
int cpu_emulate(int cycles);
int cpu_step(int max);
void cpu_reset();

/* rtc.c */
void rtc_latch(byte b);
void rtc_write(byte b);
void rtc_tick();
void rtc_load_internal(u8 *buffer);
void rtc_save_internal(u8 *buffer);

/* mem.c */
void mem_init ();
void mem_updatemap();
void mem_write(int a, byte b);
byte mem_read(int a);
void mbc_reset();

/* sound.c */
void sound_mix();
void sound_dirty();
void sound_off();
void sound_reset();
byte sound_read(byte r);
void sound_write(byte r, byte b);

#endif


#ifdef IS_LITTLE_ENDIAN
#define LO 0
#define HI 1
#else
#define LO 1
#define HI 0
#endif


#endif

