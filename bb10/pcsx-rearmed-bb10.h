#ifndef __PCSX_REARMED_BB10_H__
#define __PCSX_REARMED_BB10_H__

#include <screen/screen.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int device;
	int type;
	char id[64];
	screen_device_t handle;
	int buttons[16];
	int gamepad[16];
	int analogCount;
	int analog0[3];
	int analog1[3];
} Controller;

typedef struct {
	int gpu_neon_enhancement;
	int gpu_neon_enhancement_no_main;
	int soft_filter;
	int frameskip; //= -1, 1, 0

	char *bios; //Default SCPH1001.BIN
	char *biosDir;

	int audio_xa;
	int audio_cdda;
	int analog_enabled;
	int chid;
	Controller *controllers;
} SettingsBB10;

//For the frontend to emulator interface
enum {
	FRONTEND_MAP_BUTTON,
	FRONTEND_PLAY,
	FRONTEND_EXIT,
	FRONTEND_RESUME
};

//Menu actions for the emulator
enum {
	MENU_ENTER_MENU = 1,
	MENU_DISC_SWAP
};

int bb10_main(void* screen_ctx, const char * group, const char* win_id);
void check_profile(void);
void bb10_pcsx_set_rom(char* rom);
void bb10_pcsx_save_state();
void bb10_pcsx_load_state();
void bb10_pcsx_stop_emulator();
void bb10_pcsx_enter_menu(int code);
void bb10_pcsx_set_settings(SettingsBB10*);

#ifdef __cplusplus
}
#endif

#endif
