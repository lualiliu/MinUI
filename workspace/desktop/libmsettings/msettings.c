#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#include "msettings.h"

///////////////////////////////////////

#define SETTINGS_VERSION 2
typedef struct Settings {
	int version;
	int brightness;
	int headphones;
	int speaker;
	int unused[2];
	int jack; 
	int hdmi; 
} Settings;

static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 5,
	.headphones = 10,
	.speaker = 10,
	.jack = 0,
	.hdmi = 0,
};

static Settings* settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);

void InitSettings(void) {	
	const char* userdata_path = getenv("USERDATA_PATH");
	if (!userdata_path) userdata_path = "/tmp/minui_userdata";
	
	sprintf(SettingsPath, "%s/msettings.bin", userdata_path);
	
	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644);
	if (shm_fd==-1 && errno==EEXIST) {
		puts("Settings client");
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	}
	else {
		puts("Settings host");
		is_host = 1;
		ftruncate(shm_fd, shm_size);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		
		int fd = open(SettingsPath, O_RDONLY);
		if (fd>=0) {
			read(fd, settings, shm_size);
			close(fd);
		}
		else {
			memcpy(settings, &DefaultSettings, shm_size);
		}
	}
	
	printf("brightness: %i\nspeaker: %i\n", settings->brightness, settings->speaker); 
	fflush(stdout);
}

void QuitSettings(void) {
	munmap(settings, shm_size);
	if (is_host) shm_unlink(SHM_KEY);
}

static inline void SaveSettings(void) {
	int fd = open(SettingsPath, O_CREAT|O_WRONLY, 0644);
	if (fd>=0) {
		write(fd, settings, shm_size);
		close(fd);
		sync();
	}
}

int GetBrightness(void) {
	return settings->brightness;
}

void SetBrightness(int value) {
	if (value < 0) value = 0;
	if (value > 10) value = 10;
	settings->brightness = value;
	SaveSettings();
}

int GetVolume(void) {
	return settings->jack ? settings->headphones : settings->speaker;
}

void SetVolume(int value) {
	if (value < 0) value = 0;
	if (value > 20) value = 20;
	
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;
	
	SaveSettings();
}

void SetRawBrightness(int val) {
	// desktop: no-op
}

void SetRawVolume(int val) {
	// desktop: no-op
}

int GetJack(void) {
	return settings->jack;
}

void SetJack(int value) {
	settings->jack = value;
	SetVolume(GetVolume());
}

int GetHDMI(void) {	
	return settings->hdmi;
}

void SetHDMI(int value) {
	settings->hdmi = value;
}

int GetMute(void) { 
	return 0; 
}

void SetMute(int value) { }


