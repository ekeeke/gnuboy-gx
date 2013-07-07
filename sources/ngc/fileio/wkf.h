/* deviceHandler-wiikeyfusion.h
	- device implementation for Wiikey Fusion
	by emu_kidid
 */


#ifndef WKF_H
#define WKF_H

#include <gccore.h>
#include <ogc/disc_io.h>

#include "font.h"

#define DEVICE_TYPE_GC_WKF	(('W'<<24)|('K'<<16)|('F'<<8)|'!')

void wkfWriteRam(int offset, int data);
void wkfWriteOffset(int offset);
int wkfSpiRead(unsigned char *buf, unsigned int addr, int len);
void wkfRead(void* dst, int len, u64 offset);
unsigned int __wkfSpiReadId();
void wkfReinit();


extern bool wkfIsInserted(int chn);
extern void __wkfReset();

extern void WaitPrompt (char *msg);
extern void ShowAction (char *msg);

#endif