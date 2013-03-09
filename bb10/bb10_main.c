/*
 * (C) notaz, 2010-2011
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <bps/dialog.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <dirent.h>

#include "main.h"
#include "menu.h"
#include "plugin.h"
#include "plugin_lib.h"
#include "../libpcsxcore/misc.h"
#include "../libpcsxcore/cdrom.h"
#include "../libpcsxcore/cdriso.h"
#include "../libpcsxcore/new_dynarec/new_dynarec.h"
#include "../plugins/dfinput/main.h"
#include "qnx_common.h"

#include "pcsx-rearmed-bb10.h"
#include <sys/neutrino.h>

int g_opts = 0;
//int g_maemo_opts;
char file_name[MAXPATHLEN];
int g_scaler, soft_filter;
int g_menuscreen_w, g_menuscreen_h;

enum sched_action emu_action;
void do_emu_action(void);

static void make_path(char *buf, size_t size, const char *dir, const char *fname)
{
	if (fname)
		snprintf(buf, size, "%s%s", dir, fname);
	else
		snprintf(buf, size, "%s", dir);
}

#define MAKE_PATH(buf, dir, fname) \
	make_path(buf, sizeof(buf), dir, fname)

static void create_profile_dir(const char *directory) {
	char path[MAXPATHLEN];

	MAKE_PATH(path, directory, NULL);
	mkdir(path, S_IRWXU | S_IRWXG);
}

EXPORT void check_profile(void) {

	create_profile_dir(PCSX_DOT_DIR_BB10);

	create_profile_dir(BIOS_DIR_BB10);
	create_profile_dir(MEMCARD_DIR_BB10);
	create_profile_dir(STATES_DIR_BB10);
	create_profile_dir(PLUGINS_DIR_BB10);
	create_profile_dir(PLUGINS_CFG_DIR_BB10);
	create_profile_dir(CHEATS_DIR_BB10);
	create_profile_dir(PATCHES_DIR_BB10);
	create_profile_dir(PCSX_DOT_DIR_BB10 "cfg");
	create_profile_dir(ISO_DIR_BB10);
	create_profile_dir(BOXART_DIR_BB10);
}

static void check_memcards(void)
{
	char buf[MAXPATHLEN];
	FILE *f;
	int i;

	for (i = 1; i <= 9; i++) {
		snprintf(buf, sizeof(buf), "%scard%d.mcd", MEMCARD_DIR_BB10, i);

		f = fopen(buf, "rb");
		if (f == NULL) {
			SysPrintf("Creating memcard: %s\n", buf);
			CreateMcd(buf);
		}
		else
			fclose(f);
	}
}

int get_state_filename(char *buf, int size, int i) {
	return get_gameid_filename(buf, size,
		STATES_DIR_BB10 "%.32s-%.9s.%3.3d", i);
}

EXPORT void bb10_pcsx_stop_emulator() {
	stop = 1;
	shutdown_emu = 1;
}

EXPORT void bb10_pcsx_save_state(){
	state_slot = 1;
	emu_set_action(SACTION_NONE);
	emu_set_action(SACTION_SAVE_STATE);
}

EXPORT void bb10_pcsx_load_state(){
	state_slot = 1;
	emu_set_action(SACTION_NONE);
	emu_set_action(SACTION_LOAD_STATE);
}
int emu_custom_code;
EXPORT void bb10_pcsx_enter_menu(int code){
	printf("Enter Menu...\n");fflush(stdout);
	emu_custom_code = code;

	emu_set_action(SACTION_NONE);
	emu_set_action(SACTION_ENTER_MENU);
}

char *cdfile = NULL;
EXPORT void bb10_pcsx_set_rom(char* rom) {
	cdfile = strdup(rom);
}

/*
int gpu_neon_enhancement;
int gpu_neon_enhancement_no_main;
int soft_filter;
int frameskip; //= -1, 1, 0

char *bios_name; //Default SCPH1001.BIN

int audio_xa;
int audio_cdda;
*/
SettingsBB10 cfg_bb10;
void bb10_pcsx_set_settings(SettingsBB10* cfg) {
	memcpy(&cfg_bb10, cfg, sizeof(SettingsBB10));
}

static void update_settings(){
	//Set the settings from the frontend
	pl_rearmed_cbs.gpu_neon.enhancement_enable = cfg_bb10.gpu_neon_enhancement;
	pl_rearmed_cbs.gpu_neon.enhancement_no_main = cfg_bb10.gpu_neon_enhancement_no_main;
	soft_filter = cfg_bb10.soft_filter;
	pl_rearmed_cbs.frameskip = cfg_bb10.frameskip;//= AUTO:-1, OFF:0 ON:1
	//BIOS, find bios with picker
	sprintf(Config.Bios, "%s", cfg_bb10.bios);
	sprintf(Config.BiosDir, "%s", cfg_bb10.biosDir);
	//AUDIO
	Config.Xa = cfg_bb10.audio_xa;
	Config.Cdda = cfg_bb10.audio_cdda;
	analog_enabled = cfg_bb10.analog_enabled;

	printf("Settings:\n");
	printf("************\n");
	printf("GPU Neon enhancement: %d\n", pl_rearmed_cbs.gpu_neon.enhancement_enable);
	printf("GPU Neon enhancement hack: %d\n", pl_rearmed_cbs.gpu_neon.enhancement_no_main);
	printf("Softfilter: %d\n", soft_filter);
	printf("Frameskip: %d\n", pl_rearmed_cbs.frameskip);
	printf("Xa: %d\n", Config.Xa);
	printf("Cdda: %d\n", Config.Cdda);
	printf("Analog Pad: %d\n", analog_enabled);
	printf("Controller 1st button: %d\n", cfg_bb10.controllers[0].buttons[0]);
	printf("Bios: %s%s\n", Config.BiosDir, Config.Bios);

	if(analog_enabled) {
		in_type1 = PSE_PAD_TYPE_ANALOGPAD;
	} else {
		in_type1 = PSE_PAD_TYPE_STANDARD;
	}
}

EXPORT int bb10_main(void* screen_ctx, const char * group, const char* win_id)
{
	int loadst = 0;
	int rc;

	emu_core_preinit();

	MAKE_PATH(Config.Mcd1, MEMCARD_DIR_BB10, "card1.mcd");
	MAKE_PATH(Config.Mcd2, MEMCARD_DIR_BB10, "card2.mcd");

	//strcpy(Config.BiosDir, BIOS_DIR_BB10);
	strcpy(Config.PluginsDir, PLUGINS_DIR_BB10);
	snprintf(Config.PatchesDir, sizeof(Config.PatchesDir), PATCHES_DIR_BB10);
	Config.PsxAuto = 1;
	
	pl_init();

	check_profile();
	check_memcards();

	update_settings();

	emu_core_init();

	//Pick the ISO to load using QNX BPS dialogs
	bps_initialize();
	dialog_request_events(0);

	qnx_init(screen_ctx, group, win_id);

	if(cfg_bb10.controllers[0].device != 1){
		hideTouchControls();
	}

	set_cd_image(cdfile);

	if (LoadPlugins() == -1) {
		SysMessage("Failed loading plugins!");
		return 1;
	}

	if (OpenPlugins() == -1) {
		return 1;
	}
	plugin_call_rearmed_cbs();

	if(cdfile != NULL){
		CheckCdrom();
	}
	SysReset();

	if (cdfile) {
		if (LoadCdrom() == -1) {
			ClosePlugins();
			printf(_("Could not load CD-ROM!\n"));
			return -1;
		}
		emu_on_new_cd(0);
		ready_to_go = 1;
	}

	if (!ready_to_go) {
		printf ("something goes wrong, maybe you forgot -cdfile ? \n");
		return 1;
	}
	fflush(stdout);
	// If a state has been specified, then load that
	if (loadst) {
		int ret = emu_load_state(loadst - 1);
		printf("%s state %d\n", ret ? "failed to load" : "loaded", loadst);
	}

	if (GPU_open != NULL) {
		int ret = GPU_open(&gpuDisp, "PCSX", NULL);
		if (ret)
			fprintf(stderr, "Warning: GPU_open returned %d\n", ret);
	}

	dfinput_activate();
	pl_timing_prepare(Config.PsxType);

	while (!shutdown_emu)
	{
		stop = 0;
		emu_action = SACTION_NONE;

		psxCpu->Execute();
		if (emu_action != SACTION_NONE && shutdown_emu != 1)
			do_emu_action();
	}

	ClosePlugins();
	SysClose();
	qnx_cleanup();
	//free(cdfile);

	return 0;
}

int reload_plugins(const char *cdimg)
{
	pl_vout_buf = NULL;

	ClosePlugins();

	set_cd_image(cdimg);
	LoadPlugins();

	NetOpened = 0;
	if (OpenPlugins() == -1) {
		return -1;
	}
	plugin_call_rearmed_cbs();

	cdrIsoMultidiskCount = 1;
	CdromId[0] = '\0';
	CdromLabel[0] = '\0';

	return 0;
}

int run_cd_image(const char *fname)
{
	ready_to_go = 0;
	reload_plugins(fname);

	// always autodetect, menu_sync_config will override as needed
	Config.PsxAuto = 1;

	if (CheckCdrom() == -1) {
		// Only check the CD if we are starting the console with a CD
		ClosePlugins();
		return -1;
	}

	SysReset();

	// Read main executable directly from CDRom and start it
	if (LoadCdrom() == -1) {
		ClosePlugins();
		return -1;
	}

	emu_on_new_cd(1);
	ready_to_go = 1;

	return 0;
}

void do_emu_action_custom(enum sched_action emu_action){
	int msg;
	int rcvid;

	printf("Custom Action...\n");fflush(stdout);

	switch(emu_action){
	case SACTION_ENTER_MENU:

		if(cfg_bb10.controllers[0].device == 1) {
			hideTouchControls();
		}

		//Wait on a msg when we hit play
		while(1){
			rcvid = MsgReceive(cfg_bb10.chid, &msg, sizeof(msg), 0);

			if(rcvid <= 0){
				MsgReply(rcvid, 0, NULL, 0);
				return;
			}

			MsgReply(rcvid, 0, NULL, 0);

			if ((msg == FRONTEND_PLAY) || (msg == FRONTEND_RESUME)) {
				break;
			} else if (msg == FRONTEND_EXIT){
				bb10_pcsx_stop_emulator();
				return;
			}
		}

		update_settings();
		if(cfg_bb10.controllers[0].device == 1) {
			hideTouchControls();
		}

		if((emu_custom_code == MENU_ENTER_MENU) && (msg == FRONTEND_PLAY)) {
			new_dynarec_clear_full();
			run_cd_image(cdfile);
		} else if ((emu_custom_code == MENU_DISC_SWAP) || (msg == FRONTEND_RESUME)) {
			printf("Swapping dics...\n");fflush(stdout);

			CdromId[0] = '\0';
			CdromLabel[0] = '\0';

			set_cd_image(cdfile);
			if (ReloadCdromPlugin() < 0) {
				return;
			}
			if (CDR_open() < 0) {
				return;
			}

			SetCdOpenCaseTime(time(NULL) + 2);
			LidInterrupt();
		}

		break;
	default:
		break;
	}
	return;
}
