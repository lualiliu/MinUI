// desktop
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////

void InitSettings(void){}
void QuitSettings(void){}

int GetBrightness(void) { return 5; }
int GetVolume(void) { return 10; }

void SetRawBrightness(int value) {}
void SetRawVolume(int value){}

void SetBrightness(int value) {}
void SetVolume(int value) {}

int GetJack(void) { return 0; }
void SetJack(int value) {}

int GetHDMI(void) { return 0; }
void SetHDMI(int value) {}

int GetMute(void) { return 0; }
void SetMute(int value) {}

///////////////////////////////

void PLAT_initInput(void) {
	// buh
}

void PLAT_quitInput(void) {
	// buh
}

///////////////////////////////

static struct VID_Context {
	SDL_Surface* screen;
	GFX_Renderer* renderer;
} vid;

SDL_Surface* PLAT_initVideo(void) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	vid.screen = SDL_SetVideoMode(FIXED_WIDTH, FIXED_HEIGHT, FIXED_DEPTH, SDL_HWSURFACE | SDL_DOUBLEBUF);
	if (!vid.screen) {
		fprintf(stderr, "Failed to create video mode: %s\n", SDL_GetError());
		exit(1);
	}
	
	PLAT_clearVideo(vid.screen);
	
	return vid.screen;
}

void PLAT_quitVideo(void) {
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* IGNORED) {
	memset(vid.screen->pixels, 0, FIXED_SIZE);
}

void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
}

void PLAT_setVsync(int vsync) {
	// buh
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	PLAT_clearVideo(vid.screen);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}

void PLAT_setNearestNeighbor(int enabled) {
	// buh
}

void PLAT_setSharpness(int sharpness) {
	// buh
}

void PLAT_setEffectColor(int color) {
	// buh
}

void PLAT_setEffect(int effect) {
	// buh
}

void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	GFX_freeAAScaler();
	switch (renderer->scale) {
		case  6: return scale6x6_c16;
		case  5: return scale5x5_c16;
		case  4: return scale4x4_c16;
		case  3: return scale3x3_c16;
		case  2: return scale2x2_c16;
		case -1: return GFX_getAAScaler(renderer);
		default: return scale1x1_c16;
	}
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	void* src = renderer->src + (renderer->src_y * renderer->src_p) + (renderer->src_x * FIXED_BPP);
	void* dst = renderer->dst + (renderer->dst_y * renderer->dst_p) + (renderer->dst_x * FIXED_BPP);
	((scaler_t)renderer->blit)(src,dst,renderer->src_w,renderer->src_h,renderer->src_p,renderer->dst_w,renderer->dst_h,renderer->dst_p);
}

void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	SDL_Flip(vid.screen);
}

///////////////////////////////

#define OVERLAY_WIDTH PILL_SIZE
#define OVERLAY_HEIGHT PILL_SIZE
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP)
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}

void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}

void PLAT_enableOverlay(int enable) {
	// buh
}

///////////////////////////////

static int online = 1;

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = 1;
	*charge = 100;
	return;
}

void PLAT_enableBacklight(int enable) {
	// buh
}

void PLAT_powerOff(void) {
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	// buh
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "Desktop";
}

int PLAT_isOnline(void) {
	return online;
}

int PLAT_supportsOverscan(void) {
	return 0;
}
