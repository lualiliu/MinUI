// desktop
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////

void InitSettings(void){}
void QuitSettings(void){}

#define DESKTOP_BRIGHTNESS_PATH "/sys/class/leds/lcd_backlight0/brightness"
#define BRIGHTNESS_MIN_LEVEL 0
#define BRIGHTNESS_MAX_LEVEL 10
#define BRIGHTNESS_MAX_RAW 255
#define DESKTOP_BATTERY_CAPACITY_PATH "/sys/class/power_supply/Battery/capacity"
#define DESKTOP_BATTERY_STATUS_PATH "/sys/class/power_supply/Battery/status"

static int desktop_brightness_level = 5;

int GetBrightness(void) {
	int raw = getInt(DESKTOP_BRIGHTNESS_PATH);
	if (raw < 0) return desktop_brightness_level;
	if (raw > BRIGHTNESS_MAX_RAW) raw = BRIGHTNESS_MAX_RAW;
	
	int level = (raw * BRIGHTNESS_MAX_LEVEL + (BRIGHTNESS_MAX_RAW / 2)) / BRIGHTNESS_MAX_RAW;
	if (level < BRIGHTNESS_MIN_LEVEL) level = BRIGHTNESS_MIN_LEVEL;
	if (level > BRIGHTNESS_MAX_LEVEL) level = BRIGHTNESS_MAX_LEVEL;
	desktop_brightness_level = level;
	return desktop_brightness_level;
}
int GetVolume(void) { return 10; }

void SetRawBrightness(int value) {
	if (value < 0) value = 0;
	if (value > BRIGHTNESS_MAX_RAW) value = BRIGHTNESS_MAX_RAW;
	putInt(DESKTOP_BRIGHTNESS_PATH, value);
}
void SetRawVolume(int value){}

void SetBrightness(int value) {
	if (value < BRIGHTNESS_MIN_LEVEL) value = BRIGHTNESS_MIN_LEVEL;
	if (value > BRIGHTNESS_MAX_LEVEL) value = BRIGHTNESS_MAX_LEVEL;
	desktop_brightness_level = value;
	int raw = (value * BRIGHTNESS_MAX_RAW) / BRIGHTNESS_MAX_LEVEL;
	SetRawBrightness(raw);
}
void SetVolume(int value) {}

int GetJack(void) { return 0; }
void SetJack(int value) {}

int GetHDMI(void) { return 0; }
void SetHDMI(int value) {}

int GetMute(void) { return 0; }
void SetMute(int value) {}

///////////////////////////////

static SDL_Joystick* joystick = NULL;
static int joystick_count = -1;
static int joystick_probe_count = -1;
static uint32_t joystick_scan_at = 0;
static uint32_t joystick_force_rescan_at = 0;
static int joystick_index = -1;
#define JOYSTICK_SCAN_INTERVAL_ACTIVE_MS 8000
#define JOYSTICK_SCAN_INTERVAL_IDLE_MS 8000
#define JOYSTICK_FORCE_RESCAN_INTERVAL_MS 30000

static int PLAT_countJoystickDevices(void) {
	DIR* dir = opendir("/dev/input");
	if (!dir) return -1;

	int count = 0;
	struct dirent* entry = NULL;
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, "js", 2) != 0) continue;
		char* end = NULL;
		long idx = strtol(entry->d_name + 2, &end, 10);
		if (end && *end == '\0' && idx >= 0) count++;
	}

	closedir(dir);
	return count;
}

static int PLAT_countEventDevices(void) {
	DIR* dir = opendir("/dev/input");
	if (!dir) return -1;

	int count = 0;
	struct dirent* entry = NULL;
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, "event", 5) != 0) continue;
		char* end = NULL;
		long idx = strtol(entry->d_name + 5, &end, 10);
		if (end && *end == '\0' && idx >= 0) count++;
	}

	closedir(dir);
	return count;
}

static void PLAT_resetJoystickSubsystem(void) {
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
}

static void PLAT_closeJoystick(void) {
	if (joystick) {
		SDL_JoystickClose(joystick);
		joystick = NULL;
		joystick_index = -1;
	}
}

static void PLAT_openJoystick(int device_index) {
	int count = SDL_NumJoysticks();
	if (device_index < 0 || device_index >= count) return;
	PLAT_closeJoystick();
	joystick = SDL_JoystickOpen(device_index);
	if (joystick) joystick_index = device_index;
}

static void PLAT_refreshJoystickHotplug(int force) {
	uint32_t now = SDL_GetTicks();
	if (!force && now < joystick_scan_at) return;
	uint32_t interval = joystick ? JOYSTICK_SCAN_INTERVAL_ACTIVE_MS : JOYSTICK_SCAN_INTERVAL_IDLE_MS;
	joystick_scan_at = now + interval;

	// 先做轻量级探测，优先检测 event 节点变化（旧内核兼容性更好）。
	int event_count = PLAT_countEventDevices();
	int js_count = PLAT_countJoystickDevices();
	int probe_count = event_count;
	if (probe_count < 0) probe_count = js_count;
	if (probe_count < 0) probe_count = SDL_NumJoysticks();
	if (probe_count < 0) probe_count = 0;

	// 轻量检测可能在部分环境不可靠，定期执行一次完整重扫兜底。
	int need_full_rescan = force || (probe_count != joystick_probe_count) || (now >= joystick_force_rescan_at);
	if (!need_full_rescan) return;

	// SDL 1.2 通常需要重置子系统才能识别热插拔变化。
	PLAT_closeJoystick();
	PLAT_resetJoystickSubsystem();

	int count = SDL_NumJoysticks();
	if (count < 0) count = 0;

	joystick_probe_count = probe_count;
	joystick_count = count;
	joystick_force_rescan_at = now + JOYSTICK_FORCE_RESCAN_INTERVAL_MS;
	if (count > 0) PLAT_openJoystick(0);
}

void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	PLAT_refreshJoystickHotplug(1);
}

void PLAT_quitInput(void) {
	joystick_count = -1;
	joystick_probe_count = -1;
	joystick_scan_at = 0;
	joystick_force_rescan_at = 0;
	PLAT_closeJoystick();
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////

static struct VID_Context {
	SDL_Surface* screen;
	SDL_Surface* canvas;
	GFX_Renderer* renderer;
} vid;

SDL_Surface* PLAT_initVideo(void) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	// screen: actual output framebuffer (portrait)
	vid.screen = SDL_SetVideoMode(DISPLAY_WIDTH, DISPLAY_HEIGHT, FIXED_DEPTH, SDL_SWSURFACE);
	if (!vid.screen) {
		fprintf(stderr, "Failed to create video mode: %s\n", SDL_GetError());
		exit(1);
	}

	// canvas: logical render target (landscape), rotated when presenting
	vid.canvas = SDL_CreateRGBSurface(SDL_SWSURFACE, FIXED_WIDTH, FIXED_HEIGHT, FIXED_DEPTH, 0, 0, 0, 0);
	if (!vid.canvas) {
		fprintf(stderr, "Failed to create render surface: %s\n", SDL_GetError());
		exit(1);
	}
	
	PLAT_clearVideo(vid.canvas);
	
	return vid.canvas;
}

void PLAT_quitVideo(void) {
	if (vid.canvas) SDL_FreeSurface(vid.canvas);
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* IGNORED) {
	if (vid.canvas) SDL_FillRect(vid.canvas, NULL, 0);
}

void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
}

void PLAT_setVsync(int vsync) {
	// buh
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	PLAT_clearVideo(vid.screen);
	return vid.canvas;
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
	PLAT_refreshJoystickHotplug(0);
	if (remaining>0) SDL_Delay(remaining);
}

// from https://github.com/jdgleaver/ReGBA/blob/master/source/opendingux/draw.c#L972
/***************************************************************************
 *   Scaler copyright (C) 2013 by Paul Cercueil                            *
 *   paul@crapouillou.net                                                  *
 ***************************************************************************/

// Explaining the magic constants:
// F7DEh is the mask to remove the lower bit of all color
// components before dividing them by 2. Otherwise, the lower bit
// would bleed into the high bit of the next component.

// RRRRR GGGGGG BBBBB        RRRRR GGGGGG BBBBB
// 11110 111110 11110 [>> 1] 01111 011111 01111

// 0821h is the mask to gather the low bits again for averaging
// after discarding them.

// RRRRR GGGGGG BBBBB       RRRRR GGGGGG BBBBB
// 00001 000001 00001 [+ X] 00010 000010 00010

// E79Ch is the mask to remove the lower 2 bits of all color
// components before dividing them by 4. Otherwise, the lower bits
// would bleed into the high bits of the next component.

// RRRRR GGGGGG BBBBB        RRRRR GGGGGG BBBBB
// 11100 111100 11100 [>> 2] 00111 001111 00111

// 1863h is the mask to gather the low bits again for averaging
// after discarding them.

// RRRRR GGGGGG BBBBB       RRRRR GGGGGG BBBBB
// 00011 000011 00011 [+ X] 00110 000110 00110

/* Calculates the average of two RGB565 pixels. The source of the pixels is
 * the lower 16 bits of both parameters. The result is in the lower 16 bits.
 */

 #define MAGIC_VAL1 0xF7DEu
 #define MAGIC_VAL2 0x0821u
 #define MAGIC_VAL3 0xE79Cu
 #define MAGIC_VAL4 0x1863u
 #define MAGIC_DOUBLE(n) ((n << 16) | n)
 #define MAGIC_VAL1D MAGIC_DOUBLE(MAGIC_VAL1)
 #define MAGIC_VAL2D MAGIC_DOUBLE(MAGIC_VAL2)
 #define Average(A, B) ((((A) & MAGIC_VAL1) >> 1) + (((B) & MAGIC_VAL1) >> 1) + ((A) & (B) & MAGIC_VAL2))
 
 /* Calculates the average of two RGB565 pixels. The source of the pixels is
  * the lower 16 bits of both parameters. The result is in the lower 16 bits.
  * The average is weighted so that the first pixel contributes 3/4 of its
  * color and the second pixel contributes 1/4. */
 #define AverageQuarters3_1(A, B) ( (((A) & MAGIC_VAL1) >> 1) + (((A) & MAGIC_VAL3) >> 2) + (((B) & MAGIC_VAL3) >> 2) + ((( (( ((A) & MAGIC_VAL4) + ((A) & MAGIC_VAL2) ) << 1) + ((B) & MAGIC_VAL4) ) >> 2) & MAGIC_VAL4) )
 
 #define RED_FROM_NATIVE(rgb565) (((rgb565) >> 11) & 0x1F)
 #define RED_TO_NATIVE(r) (((r) & 0x1F) << 11)
 #define GREEN_FROM_NATIVE(rgb565) (((rgb565) >> 5) & 0x3F)
 #define GREEN_TO_NATIVE(g) (((g) & 0x3F) << 5)
 #define BLUE_FROM_NATIVE(rgb565) ((rgb565) & 0x1F)
 #define BLUE_TO_NATIVE(b) ((b) & 0x1F)
 
 
 #ifdef __GNUC__
 #	define likely(x)       __builtin_expect((x),1)
 #	define unlikely(x)     __builtin_expect((x),0)
 #	define prefetch(x, y)  __builtin_prefetch((x),(y))
 #else
 #	define likely(x)       (x)
 #	define unlikely(x)     (x)
 #   define prefetch(x, y)
 #endif

/*
 * Approximately bilinear scalers
 *
 * Copyright (C) 2019 hi-ban, Nebuleon <nebuleon.fumika@gmail.com>
 *
 * This function and all auxiliary functions are free software; you can
 * redistribute them and/or modify them under the terms of the GNU Lesser
 * General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * These functions are distributed in the hope that they will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

//from RGB565
#define cR(A) (((A) & 0xf800) >> 11)
#define cG(A) (((A) & 0x7e0) >> 5)
#define cB(A) ((A) & 0x1f)
//to RGB565
#define Weight1_1(A, B)  ((((cR(A) + cR(B)) >> 1) & 0x1f) << 11 | (((cG(A) + cG(B)) >> 1) & 0x3f) << 5 | (((cB(A) + cB(B)) >> 1) & 0x1f))
#define Weight1_2(A, B)  ((((cR(A) + (cR(B) << 1)) / 3) & 0x1f) << 11 | (((cG(A) + (cG(B) << 1)) / 3) & 0x3f) << 5 | (((cB(A) + (cB(B) << 1)) / 3) & 0x1f))
#define Weight2_1(A, B)  ((((cR(B) + (cR(A) << 1)) / 3) & 0x1f) << 11 | (((cG(B) + (cG(A) << 1)) / 3) & 0x3f) << 5 | (((cB(B) + (cB(A) << 1)) / 3) & 0x1f))
#define Weight1_3(A, B)  ((((cR(A) + (cR(B) * 3)) >> 2) & 0x1f) << 11 | (((cG(A) + (cG(B) * 3)) >> 2) & 0x3f) << 5 | (((cB(A) + (cB(B) * 3)) >> 2) & 0x1f))
#define Weight3_1(A, B)  ((((cR(B) + (cR(A) * 3)) >> 2) & 0x1f) << 11 | (((cG(B) + (cG(A) * 3)) >> 2) & 0x3f) << 5 | (((cB(B) + (cB(A) * 3)) >> 2) & 0x1f))
#define Weight1_4(A, B)  ((((cR(A) + (cR(B) << 2)) / 5) & 0x1f) << 11 | (((cG(A) + (cG(B) << 2)) / 5) & 0x3f) << 5 | (((cB(A) + (cB(B) << 2)) / 5) & 0x1f))
#define Weight4_1(A, B)  ((((cR(B) + (cR(A) << 2)) / 5) & 0x1f) << 11 | (((cG(B) + (cG(A) << 2)) / 5) & 0x3f) << 5 | (((cB(B) + (cB(A) << 2)) / 5) & 0x1f))
#define Weight2_3(A, B)  (((((cR(A) << 1) + (cR(B) * 3)) / 5) & 0x1f) << 11 | ((((cG(A) << 1) + (cG(B) * 3)) / 5) & 0x3f) << 5 | ((((cB(A) << 1) + (cB(B) * 3)) / 5) & 0x1f))
#define Weight3_2(A, B)  (((((cR(B) << 1) + (cR(A) * 3)) / 5) & 0x1f) << 11 | ((((cG(B) << 1) + (cG(A) * 3)) / 5) & 0x3f) << 5 | ((((cB(B) << 1) + (cB(A) * 3)) / 5) & 0x1f))
#define Weight1_1_1_1(A, B, C, D)  ((((cR(A) + cR(B) + cR(C) + cR(D)) >> 2) & 0x1f) << 11 | (((cG(A) + cG(B) + cG(C) + cG(D)) >> 2) & 0x3f) << 5 | (((cB(A) + cB(B) + cB(C) + cB(D)) >> 2) & 0x1f))

static inline uint16_t SUBPIXEL_3_1(uint16_t A, uint16_t B) {
	return RED_TO_NATIVE(RED_FROM_NATIVE(A))
	     | GREEN_TO_NATIVE(GREEN_FROM_NATIVE(A) * 3 / 4 + GREEN_FROM_NATIVE(B) * 1 / 4)
	     | BLUE_TO_NATIVE(BLUE_FROM_NATIVE(A) * 1 / 4 + BLUE_FROM_NATIVE(B) * 3 / 4);
}

static inline uint16_t SUBPIXEL_1_1(uint16_t A, uint16_t B) {
	return RED_TO_NATIVE(RED_FROM_NATIVE(A) * 3 / 4 + RED_FROM_NATIVE(B) * 1 / 4)
	     | GREEN_TO_NATIVE(GREEN_FROM_NATIVE(A) * 1 / 2 + GREEN_FROM_NATIVE(B) * 1 / 2)
	     | BLUE_TO_NATIVE(BLUE_FROM_NATIVE(A) * 1 / 4 + BLUE_FROM_NATIVE(B) * 3 / 4);
}

static inline uint16_t SUBPIXEL_1_3(uint16_t A, uint16_t B) {
	return RED_TO_NATIVE(RED_FROM_NATIVE(B) * 1 / 4 + RED_FROM_NATIVE(A) * 3 / 4)
	     | GREEN_TO_NATIVE(GREEN_FROM_NATIVE(B) * 3 / 4 + GREEN_FROM_NATIVE(A) * 1 / 4)
	     | BLUE_TO_NATIVE(BLUE_FROM_NATIVE(B));
}

static inline uint16_t SUBPIXEL_2_1(uint16_t A, uint16_t B) {
	return RED_TO_NATIVE(RED_FROM_NATIVE(A))
	     | GREEN_TO_NATIVE(GREEN_FROM_NATIVE(A) * 2 / 3 + GREEN_FROM_NATIVE(B) * 1 / 3)
	     | BLUE_TO_NATIVE(BLUE_FROM_NATIVE(A) * 1 / 3 + BLUE_FROM_NATIVE(B) * 2 / 3);
}

static inline uint16_t SUBPIXEL_1_2(uint16_t A, uint16_t B) {
	return RED_TO_NATIVE(RED_FROM_NATIVE(B) * 1 / 3 + RED_FROM_NATIVE(A) * 2 / 3)
	     | GREEN_TO_NATIVE(GREEN_FROM_NATIVE(B) * 2 / 3 + GREEN_FROM_NATIVE(A) * 1 / 3)
	     | BLUE_TO_NATIVE(BLUE_FROM_NATIVE(B));
}


static inline uint16_t AVERAGE_1_1(uint16_t A, uint16_t B) {
	return ((((A) & MAGIC_VAL1) >> 1) + (((B) & MAGIC_VAL1) >> 1) + ((A) & (B) & MAGIC_VAL2));
}
static inline uint16_t AVERAGE_3_1(uint16_t A, uint16_t B) {
	return ( (((A) & MAGIC_VAL1) >> 1) + (((A) & MAGIC_VAL3) >> 2) + (((B) & MAGIC_VAL3) >> 2) + ((( (( ((A) & MAGIC_VAL4) + ((A) & MAGIC_VAL2) ) << 1) + ((B) & MAGIC_VAL4) ) >> 2) & MAGIC_VAL4) );
}

// TODO: not sure how to create AVERAGE_2_1() using the above MAGIC values
#define AVERAGE_2_1 Weight2_1 // TODO: the WeightX_Y macros serve a similar (the same?) purpose
#define AVERAGE_1_2 Weight1_2 // TODO: the WeightX_Y macros serve a similar (the same?) purpose

static inline void scale_240x160_320x213(void* __restrict src_, void* __restrict dst_, uint32_t src_w, uint32_t src_h, uint32_t src_pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	uint16_t* src = src_;
	uint16_t* dst = dst_;
	
	const uint32_t src_skip = src_pitch - src_w * FIXED_BPP;
	const uint32_t dst_skip = dst_pitch - dst_w * FIXED_BPP;
	
	// upscale 3x3 pixel chunks to 4x4
	for (uint_fast16_t chunk_y=0; chunk_y<src_h/3; chunk_y++) {
		for (uint_fast16_t chunk_x=0; chunk_x<src_w/3; chunk_x++) {
			uint_fast16_t r = (chunk_x==src_w/3-1) ? 4 : 6;

			// a b c
			// e f g
			// i j k
			
			// read src
			uint16_t aaa = (*(uint16_t*)((uint8_t*)src              ));
			uint16_t bbb = (*(uint16_t*)((uint8_t*)src            +2));
			uint16_t ccc = (*(uint16_t*)((uint8_t*)src            +4));

			uint16_t eee = (*(uint16_t*)((uint8_t*)src+src_pitch    ));
			uint16_t fff = (*(uint16_t*)((uint8_t*)src+src_pitch  +2));
			uint16_t ggg = (*(uint16_t*)((uint8_t*)src+src_pitch  +4));

			uint16_t iii = (*(uint16_t*)((uint8_t*)src+src_pitch*2  ));
			uint16_t jjj = (*(uint16_t*)((uint8_t*)src+src_pitch*2+2));
			uint16_t kkk = (*(uint16_t*)((uint8_t*)src+src_pitch*2+4));
			
			// blend columns
			uint16_t abb = likely(aaa==bbb) ? aaa : SUBPIXEL_1_2(aaa,bbb);
			uint16_t bbc = likely(bbb==ccc) ? bbb : SUBPIXEL_2_1(bbb,ccc);

			uint16_t eff = likely(eee==fff) ? eee : SUBPIXEL_1_2(eee,fff);
			uint16_t ffg = likely(fff==ggg) ? fff : SUBPIXEL_2_1(fff,ggg);

			uint16_t ijj = likely(iii==jjj) ? iii : SUBPIXEL_1_2(iii,jjj);
			uint16_t jjk = likely(jjj==kkk) ? jjj : SUBPIXEL_2_1(jjj,kkk);
			
			// write dst
			// (aaa) (abb) (bbc) (ccc)
			// (111) (122) (223) (333)
			*(uint16_t*)((uint8_t*)dst              ) = aaa;
			*(uint16_t*)((uint8_t*)dst            +2) = abb;
			*(uint16_t*)((uint8_t*)dst            +4) = bbc;
			*(uint16_t*)((uint8_t*)dst            +6) = ccc;
			
			// blend (while writing) rows
			*(uint16_t*)((uint8_t*)dst+dst_pitch    ) = likely(aaa==eee) ? aaa : AVERAGE_2_1(eee,aaa);
			*(uint16_t*)((uint8_t*)dst+dst_pitch  +2) = likely(eff==abb) ? eff : AVERAGE_2_1(eff,abb);
			*(uint16_t*)((uint8_t*)dst+dst_pitch  +4) = likely(ffg==bbc) ? ffg : AVERAGE_2_1(ffg,bbc);
			*(uint16_t*)((uint8_t*)dst+dst_pitch  +6) = likely(ggg==ccc) ? ggg : AVERAGE_2_1(ggg,ccc);

			*(uint16_t*)((uint8_t*)dst+dst_pitch*2  ) = likely(eee==iii) ? eee : AVERAGE_2_1(eee,iii);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*2+2) = likely(eff==ijj) ? eff : AVERAGE_2_1(eff,ijj);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*2+4) = likely(ffg==jjk) ? ffg : AVERAGE_2_1(ffg,jjk);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*2+6) = likely(ggg==kkk) ? ggg : AVERAGE_2_1(ggg,kkk);
			
			*(uint16_t*)((uint8_t*)dst+dst_pitch*3  ) = iii;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*3+2) = ijj;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*3+4) = jjk;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*3+6) = kkk;
			
			src += 3; // skip 3 columns
			dst += 4; // skip 4 columns
		}
		src = (uint16_t*)((uint8_t*)src + src_skip + (3-1) * src_pitch); // skip 3 rows
		dst = (uint16_t*)((uint8_t*)dst + dst_skip + (4-1) * dst_pitch); // skip 4 rows
	}
	
	if (src_h%3==1) {
		for (uint_fast16_t chunk_x=0; chunk_x<src_w/3; chunk_x++) {
			uint_fast16_t r = (chunk_x==src_w/3-1) ? 4 : 6;

			// a b c
			
			// read src
			uint16_t aaa = (*(uint16_t*)((uint8_t*)src  ));
			uint16_t bbb = (*(uint16_t*)((uint8_t*)src+2));
			uint16_t ccc = (*(uint16_t*)((uint8_t*)src+4));
			
			// blend columns
			uint16_t abb = likely(aaa==bbb) ? aaa : SUBPIXEL_1_2(aaa,bbb);
			uint16_t bbc = likely(bbb==ccc) ? bbb : SUBPIXEL_2_1(bbb,ccc);
			
			// write dst
			// (aaa) (abb) (bbc) (ccc)
			*(uint16_t*)((uint8_t*)dst  ) = aaa;
			*(uint16_t*)((uint8_t*)dst+2) = abb;
			*(uint16_t*)((uint8_t*)dst+4) = bbc;
			*(uint16_t*)((uint8_t*)dst+6) = ccc;
			
			src += 3; // skip 3 columns
			dst += 4; // skip 4 columns
		}
	}
}

static inline void scale_160x144_266x240(void* __restrict src_, void* __restrict dst_, uint32_t src_w, uint32_t src_h, uint32_t src_pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	uint16_t* src = src_;
	uint16_t* dst = dst_;
	
	const uint32_t src_skip = src_pitch - src_w * FIXED_BPP;
	const uint32_t dst_skip = dst_pitch - dst_w * FIXED_BPP;
	
	// upscale 3x3 pixel chunks to 5x5
	for (uint_fast16_t chunk_y=0; chunk_y<src_h; chunk_y+=3) {
		for (uint_fast16_t chunk_x=0; chunk_x<src_w; chunk_x+=3) {
			// a b c
			// d e f
			// g h i
			
			// read src
			uint16_t aaa = (*(uint16_t*)((uint8_t*)src            ));
			uint16_t bbb = (*(uint16_t*)((uint8_t*)src          +2));
			uint16_t ccc = (*(uint16_t*)((uint8_t*)src          +4));

			uint16_t ddd = (*(uint16_t*)((uint8_t*)src+src_pitch  ));
			uint16_t eee = (*(uint16_t*)((uint8_t*)src+src_pitch+2));
			uint16_t fff = (*(uint16_t*)((uint8_t*)src+src_pitch+4));

			uint16_t ggg = (*(uint16_t*)((uint8_t*)src+src_pitch*2  ));
			uint16_t hhh = (*(uint16_t*)((uint8_t*)src+src_pitch*2+2));
			uint16_t iii = (*(uint16_t*)((uint8_t*)src+src_pitch*2+4));
			
			// blend columns
			uint16_t aab = likely(aaa==bbb) ? aaa : SUBPIXEL_2_1(aaa,bbb);
			uint16_t bcc = likely(bbb==ccc) ? bbb : SUBPIXEL_1_2(bbb,ccc);
			uint16_t dde = likely(ddd==eee) ? ddd : SUBPIXEL_2_1(ddd,eee);
			uint16_t eff = likely(eee==fff) ? eee : SUBPIXEL_1_2(eee,fff);
			uint16_t ggh = likely(ggg==hhh) ? ggg : SUBPIXEL_2_1(ggg,hhh);
			uint16_t hii = likely(hhh==iii) ? hhh : SUBPIXEL_1_2(hhh,iii);
			
			// write dst
			
			// special case odd last column
			if (src_w-chunk_x<3) {
				*(uint16_t*)((uint8_t*)dst            ) = aaa;
				*(uint16_t*)((uint8_t*)dst+dst_pitch  ) = likely(aaa==ddd) ? aaa : AVERAGE_2_1(aaa,ddd);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*2) = ddd;
				*(uint16_t*)((uint8_t*)dst+dst_pitch*3) = likely(ddd==ggg) ? ddd : AVERAGE_2_1(ggg,ddd);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*4) = ggg;
				
				src += 1; // skip 1 column
				dst += 1; // skip 1 column
				continue;
			}
			
			// (aaa) (aab) (bbb) (bcc) (ccc)
			// (111) (112) (222) (233) (333)
			*(uint16_t*)((uint8_t*)dst              ) = aaa;
			*(uint16_t*)((uint8_t*)dst            +2) = aab;
			*(uint16_t*)((uint8_t*)dst            +4) = bbb;
			*(uint16_t*)((uint8_t*)dst            +6) = bcc;
			*(uint16_t*)((uint8_t*)dst            +8) = ccc;
			
			*(uint16_t*)((uint8_t*)dst+dst_pitch    ) = likely(aaa==ddd) ? aaa : AVERAGE_2_1(aaa,ddd);
			*(uint16_t*)((uint8_t*)dst+dst_pitch  +2) = likely(aab==dde) ? aab : AVERAGE_2_1(aab,dde);
			*(uint16_t*)((uint8_t*)dst+dst_pitch  +4) = likely(bbb==eee) ? bbb : AVERAGE_2_1(bbb,eee);
			*(uint16_t*)((uint8_t*)dst+dst_pitch  +6) = likely(bcc==eff) ? bcc : AVERAGE_2_1(bcc,eff);
			*(uint16_t*)((uint8_t*)dst+dst_pitch  +8) = likely(ccc==fff) ? ccc : AVERAGE_2_1(ccc,fff);

			*(uint16_t*)((uint8_t*)dst+dst_pitch*2  ) = ddd;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*2+2) = dde;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*2+4) = eee;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*2+6) = eff;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*2+8) = fff;
			
			*(uint16_t*)((uint8_t*)dst+dst_pitch*3  ) = likely(ddd==ggg) ? ddd : AVERAGE_2_1(ggg,ddd);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*3+2) = likely(dde==ggh) ? dde : AVERAGE_2_1(ggh,dde);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*3+4) = likely(eee==hhh) ? eee : AVERAGE_2_1(hhh,eee);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*3+6) = likely(eff==hii) ? eff : AVERAGE_2_1(hii,eff);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*3+8) = likely(fff==iii) ? fff : AVERAGE_2_1(iii,fff);

			*(uint16_t*)((uint8_t*)dst+dst_pitch*4  ) = ggg;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*4+2) = ggh;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*4+4) = hhh;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*4+6) = hii;
			*(uint16_t*)((uint8_t*)dst+dst_pitch*4+8) = iii;
			
			src += 3; // skip 3 columns
			dst += 5; // skip 5 columns
		}
		src = (uint16_t*)((uint8_t*)src + src_skip + (3-1) * src_pitch); // skip 3 rows
		dst = (uint16_t*)((uint8_t*)dst + dst_skip + (5-1) * dst_pitch); // skip 5 rows
	}
}

static inline void scale_256x224_320x238(void* __restrict src_, void* __restrict dst_, uint32_t src_w, uint32_t src_h, uint32_t src_pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	uint16_t* src = src_;
	uint16_t* dst = dst_;
	
	// yeesh (correct size and offset)
	// TODO: creates disagreement with minarch menu preview
	dst -= (320 - dst_w)/2;
	dst += dst_pitch * (dst_h-238)/2;
	dst_w = 320;
	dst_h = 238;
	
	const uint32_t src_skip = src_pitch - src_w * FIXED_BPP;
	const uint32_t dst_skip = dst_pitch - dst_w * FIXED_BPP;
	
	// upscale 4x16 pixel chunks to 5x17
	for (uint_fast16_t chunk_y=0; chunk_y<src_h; chunk_y+=16) {
		for (uint_fast16_t chunk_x=0; chunk_x<src_w; chunk_x+=4) {
			
			// TODO: this is probably a terrible idea
			uint_fast16_t row;
			uint16_t a,b,c,d,e; // previous rows columns
			for (row=0; row<3; row++) {
				// read src
				uint16_t aaaa = (*(uint16_t*)((uint8_t*)src+src_pitch*row ));
				uint16_t bbbb = (*(uint16_t*)((uint8_t*)src+src_pitch*row+2));
				uint16_t cccc = (*(uint16_t*)((uint8_t*)src+src_pitch*row+4));
				uint16_t dddd = (*(uint16_t*)((uint8_t*)src+src_pitch*row+6));
				
				// blend columns
				uint16_t abbb = likely(aaaa==bbbb) ? aaaa : SUBPIXEL_1_3(aaaa,bbbb);
				uint16_t bbcc = likely(bbbb==cccc) ? bbbb : SUBPIXEL_1_1(bbbb,cccc);
				uint16_t cccd = likely(cccc==dddd) ? cccc : SUBPIXEL_3_1(cccc,dddd);
				
				// write dst
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row  ) = a = aaaa;
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+2) = b = abbb;
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+4) = c = bbcc;
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+6) = d = cccd;
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+8) = e = dddd;
			}
			
			for (row=3; row<8; row++) {
				// read src
				uint16_t aaaa = (*(uint16_t*)((uint8_t*)src+src_pitch*row ));
				uint16_t bbbb = (*(uint16_t*)((uint8_t*)src+src_pitch*row+2));
				uint16_t cccc = (*(uint16_t*)((uint8_t*)src+src_pitch*row+4));
				uint16_t dddd = (*(uint16_t*)((uint8_t*)src+src_pitch*row+6));
				
				// blend columns
				uint16_t abbb = likely(aaaa==bbbb) ? aaaa : SUBPIXEL_1_3(aaaa,bbbb);
				uint16_t bbcc = likely(bbbb==cccc) ? bbbb : SUBPIXEL_1_1(bbbb,cccc);
				uint16_t cccd = likely(cccc==dddd) ? cccc : SUBPIXEL_3_1(cccc,dddd);
				
				// write dst
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row  ) = AVERAGE_3_1(aaaa,a);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+2) = AVERAGE_3_1(abbb,b);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+4) = AVERAGE_3_1(bbcc,c);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+6) = AVERAGE_3_1(cccd,d);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+8) = AVERAGE_3_1(dddd,e);
				
				// store src row
				a = aaaa;
				b = abbb;
				c = bbcc;
				d = cccd;
				e = dddd;
			}
			
			// read src
			uint16_t aaaa = (*(uint16_t*)((uint8_t*)src+src_pitch*row ));
			uint16_t bbbb = (*(uint16_t*)((uint8_t*)src+src_pitch*row+2));
			uint16_t cccc = (*(uint16_t*)((uint8_t*)src+src_pitch*row+4));
			uint16_t dddd = (*(uint16_t*)((uint8_t*)src+src_pitch*row+6));
			
			// blend columns
			uint16_t abbb = likely(aaaa==bbbb) ? aaaa : SUBPIXEL_1_3(aaaa,bbbb);
			uint16_t bbcc = likely(bbbb==cccc) ? bbbb : SUBPIXEL_1_1(bbbb,cccc);
			uint16_t cccd = likely(cccc==dddd) ? cccc : SUBPIXEL_3_1(cccc,dddd);
			
			// write dst
			*(uint16_t*)((uint8_t*)dst+dst_pitch*row  ) = AVERAGE_3_1(a,aaaa);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*row+2) = AVERAGE_3_1(b,abbb);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*row+4) = AVERAGE_3_1(c,bbcc);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*row+6) = AVERAGE_3_1(d,cccd);
			*(uint16_t*)((uint8_t*)dst+dst_pitch*row+8) = AVERAGE_3_1(e,dddd);
			
			// store src row
			a = aaaa;
			b = abbb;
			c = bbcc;
			d = cccd;
			e = dddd;
			
			for (row=9; row<14; row++) {
				// read src
				uint16_t aaaa = (*(uint16_t*)((uint8_t*)src+src_pitch*row ));
				uint16_t bbbb = (*(uint16_t*)((uint8_t*)src+src_pitch*row+2));
				uint16_t cccc = (*(uint16_t*)((uint8_t*)src+src_pitch*row+4));
				uint16_t dddd = (*(uint16_t*)((uint8_t*)src+src_pitch*row+6));
				
				// blend columns
				uint16_t abbb = likely(aaaa==bbbb) ? aaaa : SUBPIXEL_1_3(aaaa,bbbb);
				uint16_t bbcc = likely(bbbb==cccc) ? bbbb : SUBPIXEL_1_1(bbbb,cccc);
				uint16_t cccd = likely(cccc==dddd) ? cccc : SUBPIXEL_3_1(cccc,dddd);
				
				// write dst
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row  ) = AVERAGE_3_1(a,aaaa);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+2) = AVERAGE_3_1(b,abbb);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+4) = AVERAGE_3_1(c,bbcc);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+6) = AVERAGE_3_1(d,cccd);
				*(uint16_t*)((uint8_t*)dst+dst_pitch*row+8) = AVERAGE_3_1(e,dddd);
				
				// store src row
				a = aaaa;
				b = abbb;
				c = bbcc;
				d = cccd;
				e = dddd;
			}
			
			for (row=13; row<16; row++) {
				// read src
				uint16_t aaaa = (*(uint16_t*)((uint8_t*)src+src_pitch*row ));
				uint16_t bbbb = (*(uint16_t*)((uint8_t*)src+src_pitch*row+2));
				uint16_t cccc = (*(uint16_t*)((uint8_t*)src+src_pitch*row+4));
				uint16_t dddd = (*(uint16_t*)((uint8_t*)src+src_pitch*row+6));
				
				// blend columns
				uint16_t abbb = likely(aaaa==bbbb) ? aaaa : SUBPIXEL_1_3(aaaa,bbbb);
				uint16_t bbcc = likely(bbbb==cccc) ? bbbb : SUBPIXEL_1_1(bbbb,cccc);
				uint16_t cccd = likely(cccc==dddd) ? cccc : SUBPIXEL_3_1(cccc,dddd);
				
				*(uint16_t*)((uint8_t*)dst+dst_pitch*(row+1)  ) = aaaa;
				*(uint16_t*)((uint8_t*)dst+dst_pitch*(row+1)+2) = abbb;
				*(uint16_t*)((uint8_t*)dst+dst_pitch*(row+1)+4) = bbcc;
				*(uint16_t*)((uint8_t*)dst+dst_pitch*(row+1)+6) = cccd;
				*(uint16_t*)((uint8_t*)dst+dst_pitch*(row+1)+8) = dddd;
			}
			
			src += 4; // skip 4 columns
			dst += 5; // skip 5 columns
		}
		src = (uint16_t*)((uint8_t*)src + src_skip + (16-1) * src_pitch); // skip 16 rows
		dst = (uint16_t*)((uint8_t*)dst + dst_skip + (17-1) * dst_pitch); // skip 17 rows
	}
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	GFX_freeAAScaler();
	switch (renderer->scale) {
		case  6: return scale6x6_c16;
		case  5: return scale5x5_c16;
		case  4: return scale4x4_c16;
		case  3: return scale3x3_c16;
		case  2: return scale2x2_c16;
		case -1: {
			if (renderer->src_w==256 && renderer->src_h==224) {
				// TODO: can I fudge this here to fix minarch menu scaled preview? nope.
				// renderer->dst_x = 0;
				// renderer->dst_y = 1;
				// renderer->dst_w = 320;
				// renderer->dst_h = 238;
				return scale_256x224_320x238;
			}
			if (renderer->src_w==240 && renderer->src_h==160) return renderer->dst_h==240 ? GFX_getAAScaler(renderer) : scale_240x160_320x213;
			if (renderer->src_w==160 && renderer->src_h==144) return renderer->dst_w==320 ? GFX_getAAScaler(renderer) : scale_160x144_266x240;
			else return GFX_getAAScaler(renderer);
		}
		default: return scale1x1_c16;
	}
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	void* src = renderer->src + (renderer->src_y * renderer->src_p) + (renderer->src_x * FIXED_BPP);
	int dst_w = FIXED_WIDTH;
	int dst_h = FIXED_HEIGHT;
	int center_x = 0;
	int center_y = 0;

	// aspect == -1 表示 Fullscreen 拉伸，直接铺满目标画布。
	if (renderer->aspect != -1) {
		// 计算等比例缩放，尽可能填满屏幕
		double scale_x = (double)FIXED_WIDTH / renderer->src_w;
		double scale_y = (double)FIXED_HEIGHT / renderer->src_h;
		double scale = (scale_x < scale_y) ? scale_x : scale_y;

		// 计算缩放后的尺寸
		dst_w = (int)(renderer->src_w * scale);
		dst_h = (int)(renderer->src_h * scale);

		// 确保尺寸不为零且不超过屏幕
		if (dst_w <= 0) dst_w = 1;
		if (dst_h <= 0) dst_h = 1;
		if (dst_w > FIXED_WIDTH) dst_w = FIXED_WIDTH;
		if (dst_h > FIXED_HEIGHT) dst_h = FIXED_HEIGHT;

		// 计算居中偏移
		center_x = (FIXED_WIDTH - dst_w) / 2;
		center_y = (FIXED_HEIGHT - dst_h) / 2;
		if (center_x < 0) center_x = 0;
		if (center_y < 0) center_y = 0;
	}
	
	// 如果缩放比例不是整数倍，需要使用抗锯齿缩放器
	// 检查是否需要重新选择缩放器
	double scale = (double)dst_w / renderer->src_w;
	int scale_int = (int)(scale + 0.5); // 四舍五入到最近的整数
	double scale_diff = scale - scale_int;
	scaler_t blit_func = renderer->blit;
	
	// 如果缩放比例明显不是整数倍（差值大于0.1），或目标尺寸已改变，使用抗锯齿缩放器
	if (scale_diff > 0.1 || scale_diff < -0.1 || dst_w != renderer->dst_w || dst_h != renderer->dst_h) {
		// 临时修改 renderer 的尺寸以获取正确的抗锯齿缩放器
		int old_dst_w = renderer->dst_w;
		int old_dst_h = renderer->dst_h;
		renderer->dst_w = dst_w;
		renderer->dst_h = dst_h;
		GFX_freeAAScaler(); // 释放之前的抗锯齿缩放器
		blit_func = GFX_getAAScaler(renderer);
		renderer->dst_w = old_dst_w;
		renderer->dst_h = old_dst_h;
	}

	void* dst = renderer->dst + (center_y * vid.canvas->pitch) + (center_x * FIXED_BPP);
	blit_func(src,dst,renderer->src_w,renderer->src_h,renderer->src_p,dst_w,dst_h,vid.canvas->pitch);
}

static inline void rotate_90_cw_16bpp(SDL_Surface* src, SDL_Surface* dst) {
	if (!src || !dst) return;
	if (dst->w != src->h || dst->h != src->w) return;

	int src_pitch = src->pitch / FIXED_BPP;
	int dst_pitch = dst->pitch / FIXED_BPP;
	uint16_t* src_pixels = (uint16_t*)src->pixels;
	uint16_t* dst_pixels = (uint16_t*)dst->pixels;

	for (int y = 0; y < src->h; y++) {
		for (int x = 0; x < src->w; x++) {
			int dx = src->h - 1 - y;
			int dy = x;
			dst_pixels[dy * dst_pitch + dx] = src_pixels[y * src_pitch + x];
		}
	}
}

void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	rotate_90_cw_16bpp(vid.canvas, vid.screen);
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
	char status[64] = {0};
	int capacity = getInt(DESKTOP_BATTERY_CAPACITY_PATH);
	
	getFile(DESKTOP_BATTERY_STATUS_PATH, status, sizeof(status));
	trimTrailingNewlines(status);
	
	*is_charging = exactMatch(status, "Charging") || exactMatch(status, "Full");
	
	if (capacity < 0) capacity = 100;
	if (capacity > 100) capacity = 100;
	*charge = capacity;
}

void PLAT_enableBacklight(int enable) {
	return;
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
	//return online;
	return 0;
}

int PLAT_supportsOverscan(void) {
	return 0;
}
