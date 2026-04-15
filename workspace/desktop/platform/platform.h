// desktop/platform/platform.h

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include "sdl.h"

///////////////////////////////

#define	BUTTON_UP		BUTTON_NA
#define	BUTTON_DOWN		BUTTON_NA
#define	BUTTON_LEFT		BUTTON_NA
#define	BUTTON_RIGHT	BUTTON_NA

#define	BUTTON_SELECT	BUTTON_NA
#define	BUTTON_START	BUTTON_NA

#define	BUTTON_A		BUTTON_NA
#define	BUTTON_B		BUTTON_NA
#define	BUTTON_X		BUTTON_NA
#define	BUTTON_Y		BUTTON_NA

#define	BUTTON_L1		BUTTON_NA
#define	BUTTON_R1		BUTTON_NA
#define	BUTTON_L2		BUTTON_NA
#define	BUTTON_R2		BUTTON_NA
#define BUTTON_L3 		BUTTON_NA
#define BUTTON_R3 		BUTTON_NA

#define	BUTTON_MENU		BUTTON_NA
#define	BUTTON_POWER	BUTTON_NA
#define	BUTTON_PLUS		BUTTON_NA
#define	BUTTON_MINUS	BUTTON_NA

///////////////////////////////

#define CODE_UP			103
#define CODE_DOWN		108
#define CODE_LEFT		105
#define CODE_RIGHT		106

#define CODE_SELECT		1
#define CODE_START		28

#define CODE_A			29
#define CODE_B			56
#define CODE_X			57
#define CODE_Y			42

#define CODE_L1			15
#define CODE_R1			14
#define CODE_L2			104
#define CODE_R2			109
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		102
#define CODE_MENU_ALT	107
#define CODE_POWER		116
#define CODE_POWEROFF	68

#define CODE_PLUS		78
#define CODE_MINUS		74

///////////////////////////////

#define JOY_UP			11
#define JOY_DOWN		12
#define JOY_LEFT		13
#define JOY_RIGHT		14

#define JOY_SELECT		6
#define JOY_START		7

#define JOY_A			0
#define JOY_B			1
#define JOY_X			2
#define JOY_Y			3

#define JOY_L1			4
#define JOY_R1			5
#define JOY_L2			JOY_NA
#define JOY_R2			JOY_NA
#define JOY_L3			9
#define JOY_R3			10

#define JOY_MENU		8
#define JOY_POWER		JOY_NA
#define JOY_PLUS		JOY_START
#define JOY_MINUS		JOY_SELECT

#define AXIS_L2			2
#define AXIS_R2			5
#define AXIS_LX			0
#define AXIS_LY			1
#define AXIS_RX			3
#define AXIS_RY			4

///////////////////////////////

#define BTN_RESUME			BTN_X
#define BTN_SLEEP 			BTN_POWER
#define BTN_WAKE 			BTN_POWER
#define BTN_MOD_VOLUME 		BTN_NONE
#define BTN_MOD_BRIGHTNESS 	BTN_MENU
#define BTN_MOD_PLUS 		BTN_PLUS
#define BTN_MOD_MINUS 		BTN_MINUS

///////////////////////////////

#define FIXED_SCALE 	2
#define FIXED_WIDTH		1280
#define FIXED_HEIGHT	720
#define DISPLAY_WIDTH	720
#define DISPLAY_HEIGHT	1280
#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)

///////////////////////////////

#define MAIN_ROW_COUNT (FIXED_HEIGHT / FIXED_BPP)
#define PADDING 10

///////////////////////////////

#define SDCARD_PATH "/mnt/sdcard"
#define MUTE_VOLUME_RAW 0

///////////////////////////////

#endif


