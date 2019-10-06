#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>
#include <SDL.h>

extern "C" {

extern void m68k_reset_irq();

extern unsigned char* romBIOS;
extern unsigned char* romCartridge;
extern unsigned char* ramVideo;
extern unsigned char* ramInternal;

extern SDL_Window* sdlWindow;

extern int keyScan;

extern int gfxFade;
extern long ticks;

extern FILE* diskFile;

extern void RenderLine(int);
extern void VBlank();
extern int InitVideo();
extern int UninitVideo();

extern void HandleHdma(int line);
extern int InitMemory();

extern int InitSound(int device);
extern int UninitSound();

}
