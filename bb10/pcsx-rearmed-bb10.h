#ifndef __PCSX_REARMED_BB10_H__
#define __PCSX_REARMED_BB10_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int device;
	int buttons[16];
} Controller;

typedef struct {
	int gpu_neon_enhancement;
	int gpu_neon_enhancement_no_main;
	int soft_filter;
	int frameskip; //= -1, 1, 0

	char *bios_name; //Default SCPH1001.BIN

	int audio_xa;
	int audio_cdda;
	int analog_enabled;
	int chid;
	Controller *controllers;
} SettingsBB10;

enum {
	FRONTEND_MAP_BUTTON,
	FRONTEND_START_EMULATOR,
	FRONTEND_EXIT,
	FRONTEND_DISC_SWAP
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
