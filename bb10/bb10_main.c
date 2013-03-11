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

int compare( const void* op1, const void* op2 )
{
	const char **p1 = (const char **) op1;
	const char **p2 = (const char **) op2;

	return( strcmp( *p1, *p2 ) );
}

int dialog_select_game(char * isofilename){
	char path[MAXPATHLEN];
	int i, rc;

	rc = snprintf(path, MAXPATHLEN, "%s", ISO_DIR_BB10);
	if ((rc == -1) || (rc >= MAXPATHLEN)) {
		return -1;
	}

	dialog_instance_t dialog = 0;
	bps_event_t *event;
	dialog_create_popuplist(&dialog);

	DIR *dirp;
	struct dirent* direntp;
	int count=0, domain=0;
	const char ** list = 0;
	const char * label;

	dirp = opendir(ISO_DIR_BB10);
	if( dirp != NULL ) {
		for(;;) {
			direntp = readdir( dirp );
			if( direntp == NULL ) break;
			printf( "%s\n", direntp->d_name );
			count++;
		}
		fflush(stdout);
		rewinddir(dirp);

		if(count==2){
			printf("No ISO's found!");
		}

		list = (const char**)malloc(count*sizeof(char*));
		count = 0;

		for(;;){
			direntp = readdir( dirp );
			if( direntp == NULL ) break;
			list[count] = (char*)direntp->d_name;
			count++;
		}

		int len;
		int j = 0, m;
		int disabled[count];

		//If a cue exists, disable the bin for easier readability
		for(i=0; i<count;i++){

			if( strcmp(list[i], ".") == 0 || strcmp(list[i], "..") == 0 ){
				disabled[j++] = i;
				continue;
			}

			//Check if current index is a cue
			if( strncasecmp(list[i]+strlen(list[i])-4, ".cue", 4) == 0 ){
				//Check the list for the matching bin
				for(m=0;m<count;m++){
					if( i == m ){
						continue;
					}

					len = max(strlen(list[i]),strlen(list[m]))-4;
					if( (strncmp(list[i], list[m], len) == 0) && (strncasecmp(list[m]+strlen(list[m])-4, ".bin", 4) == 0) ){
						//if we find a matching file, and it's a bin
						disabled[j++] = m;
						break;
					}
				}

				//if there is no matching bin, hide the cue too
				if(m==count){
					disabled[j++] = i;
				}
			}
		}

		fflush(stdout);
		int k = 0;
		const char * compact[count-j+2];
		compact[k++] = "Enter BIOS";
		compact[k++] = "Games";

		//For each index
		for(i=0;i<count;i++){
			//search disabled for a match
			for(m=0;m<j;m++){
				if(i==disabled[m]){
					//If we do find that i is disabled don't copy
					break;
				}
			}
			if(m==j){
				compact[k++] = list[i];
			}
		}

		//Sort compact list
		qsort( compact+2, k-2, sizeof(char *), compare );

		int indice = 1;
		dialog_set_popuplist_items(dialog, compact, k);
		dialog_set_popuplist_separator_indices(dialog, &indice, 1);
		char test[MAXPATHLEN];
		strcpy(test, "shared/misc/pcsx-rearmed-bb/bios/");
		strcat(test, Config.Bios);
		if(access(test,F_OK) != 0){
			indice = 0;
			dialog_set_popuplist_disabled_indices(dialog, &indice, 1);
		}
		char* cancel_button_context = "Canceled";
		char* okay_button_context = "Okay";
		dialog_add_button(dialog, DIALOG_CANCEL_LABEL, true, cancel_button_context, true);
		dialog_add_button(dialog, DIALOG_OK_LABEL, true, okay_button_context, true);
		dialog_set_popuplist_multiselect(dialog, false);
		dialog_show(dialog);

		while(1){
			bps_get_event(&event, -1);

			if (event) {
				domain = bps_event_get_domain(event);
				if (domain == dialog_get_domain()) {
					int *response[1];
					int num;

					label = dialog_event_get_selected_label(event);

					if(strcmp(label, DIALOG_OK_LABEL) == 0){
						dialog_event_get_popuplist_selected_indices(event, (int**)response, &num);
						if(num != 0){
							printf("%s\n", compact[*response[0]]);fflush(stdout);
							strcpy(isofilename, compact[*response[0]]);
						}
						bps_free(response[0]);
					} else {
						printf("User has canceled ISO dialog.");
						free(list);
						closedir(dirp);
						//emu_set_action(SACTION_NONE);
						return -1;
					}
					break;
				}
			}
		}

		free(list);
		closedir(dirp);
	}

	if(strcmp("Enter BIOS", isofilename) == 0 ){
		return 1;
	}

	if (strlen(path) + strlen(isofilename) + 1 < MAXPATHLEN) {
		strcat(path, isofilename);
		strcpy(isofilename, path);
	} else
		isofilename[0] = 0;
	return 0;
}

void load_newiso()
{

	ready_to_go = 0;
	const char *cdfile = NULL;
	char isofilename[MAXPATHLEN];
	int rc = 0;

	rc = dialog_select_game(isofilename);

	//Dialog canceled
	if( rc == -1 ){
		return;
	} else if(rc == 1){
		cdfile = NULL;
		CdromId[0] = '\0';
		CdromLabel[0] = '\0';
	} else {
		cdfile = isofilename;
	}

	if (GPU_close != NULL) {
	int ret = GPU_close();
	if (ret)
		fprintf(stderr, "Warning: GPU_close returned %d\n", ret);
	}

	new_dynarec_clear_full();

	ready_to_go = 0;
	ClosePlugins();

	set_cd_image(cdfile);
	LoadPlugins();

	if (OpenPlugins() == -1) {
		return;
	}
	plugin_call_rearmed_cbs();

	// always autodetect, menu_sync_config will override as needed
	Config.PsxAuto = 1;

	if(cdfile != NULL){
		if (CheckCdrom() == -1) {
			// Only check the CD if we are starting the console with a CD
			ClosePlugins();
			return;
		}
	}

	SysReset();

	if(cdfile != NULL){
		if (LoadCdrom() == -1) {
			ClosePlugins();
			return;
		}
	}
	ready_to_go = 1;

	if (Config.Cdda && cdfile != NULL)
		CDR_stop();

	// push config to GPU plugin
	plugin_call_rearmed_cbs();

	if (GPU_open != NULL) {
		int ret = GPU_open(&gpuDisp, "PCSX", NULL);
		if (ret)
			fprintf(stderr, "Warning: GPU_open returned %d\n", ret);
	}

	dfinput_activate();
	//emu_set_action(SACTION_NONE);
}

void handle_cfg(){
	FILE *fd;
	char read[128];
	char *ptr;
	if(access("shared/misc/pcsx-rearmed-bb/cfg/pcsx.cfg", F_OK) == 0){
		fd = fopen("shared/misc/pcsx-rearmed-bb/cfg/pcsx.cfg","r");

		while(fgets(read, 128, fd) != NULL){
			if((ptr = strtok(read, "=")) != NULL ){
				//Check for bios
				if(strcmp(ptr,"bios") == 0){
					if((ptr = strtok(NULL,"=")) != NULL){
						ptr[(int)strlen(ptr)-1] = '\0';
						sprintf(Config.Bios, "%s", ptr);
					}
				//Xa Audio
				}else if(strcmp(ptr,"xa") == 0){
					if((ptr = strtok(NULL,"=")) != NULL){
						if(strncmp(ptr,"1", strlen(ptr)-1) == 0)
							Config.Xa = 1;
						else
							Config.Xa = 0;
					}
				//Cdda Audio
				}else if(strcmp(ptr,"cdda") == 0){
					if((ptr = strtok(NULL,"=")) != NULL){
						if(strncmp(ptr,"1", strlen(ptr)-1) == 0)
							Config.Cdda = 1;
						else
							Config.Cdda = 0;
					}
				}
			}
		}
	}else{
		fd = fopen("shared/misc/pcsx-rearmed-bb/cfg/pcsx.cfg","w");
		//Write these lines out to file
		fputs("bios=SCPH1001.BIN\n",fd);
		sprintf(Config.Bios, "%s", "SCPH1001.BIN");
		fputs("xa=1\n",fd);
		Config.Xa = 1;
		fputs("cdda=1\n",fd);
		Config.Cdda = 1;
	}
	fclose(fd);
}

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
	emu_set_action(SACTION_SAVE_STATE);
}

EXPORT void bb10_pcsx_load_state(){
	state_slot = 1;
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
	sprintf(Config.Bios, "%s", cfg_bb10.bios_name);
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

	strcpy(Config.BiosDir, BIOS_DIR_BB10);
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
	free(cdfile);

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
		//Wait on a msg when we hit play
		if(cfg_bb10.controllers[0].device == 1)
			hideTouchControls();
		while(1){
			rcvid = MsgReceive(cfg_bb10.chid, &msg, sizeof(msg), 0);

			if(rcvid <= 0){
				return;
			} else if (msg == 1) {
				break;
			}
		}

		MsgReply(rcvid, 0, NULL, 0);

		update_settings();
		if(cfg_bb10.controllers[0].device == 1)
			hideTouchControls();

		switch(emu_custom_code) {
		case 1:

			new_dynarec_clear_full();

			run_cd_image(cdfile);
			break;
		case 2:
			printf("Swapping dics...\n");fflush(stdout);
			//Swap discs
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
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return;
}
