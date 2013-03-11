#include "pcsx-rearmed-bb10.h"

void qnx_init(void* screen_ctx, char * group, char* win_id);
void showTouchControls();
void hideTouchControls();
void qnx_cleanup();

extern char file_name[MAXPATHLEN];
extern volatile bool shutdown_emu;
extern int analog_enabled;
extern SettingsBB10 cfg_bb10;
//extern int g_maemo_opts;

#define DEFAULT_MEM_CARD_1_BB10 "shared/misc/pcsx-rearmed-bb/memcards/card1.mcd"
#define DEFAULT_MEM_CARD_2_BB10 "shared/misc/pcsx-rearmed-bb/memcards/card2.mcd"
#define MEMCARD_DIR_BB10 "shared/misc/pcsx-rearmed-bb/memcards/"
#define PLUGINS_DIR_BB10 "shared/misc/pcsx-rearmed-bb/plugins"
#define PLUGINS_CFG_DIR_BB10 "shared/misc/pcsx-rearmed-bb/plugins/cfg/"
#define PCSX_DOT_DIR_BB10 "shared/misc/pcsx-rearmed-bb/"
#define STATES_DIR_BB10 "shared/misc/pcsx-rearmed-bb/sstates/"
#define CHEATS_DIR_BB10 "shared/misc/pcsx-rearmed-bb/cheats/"
#define PATCHES_DIR_BB10 "shared/misc/pcsx-rearmed-bb/patches/"
#define BIOS_DIR_BB10 "shared/misc/pcsx-rearmed-bb/bios/"
#define ISO_DIR_BB10 "shared/misc/pcsx-rearmed-bb/iso/"
#define BOXART_DIR_BB10 "shared/misc/pcsx-rearmed-bb/.boxart/"

#define EXPORT __attribute__ ((visibility ("default")))
