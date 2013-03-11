/*
 * (C) CatalystG, 2012
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <assert.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <fcntl.h>
#include <screen/screen.h>
#include <sys/keycodes.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "plugin_lib.h"
#include "../plugins/gpulib/gpu.h"
#include "../plugins/gpulib/cspace.h"

#include "touchcontroloverlay.h"

#include "main.h"
#include "menu.h"
#include "plat.h"
#include "psemu_plugin_defs.h"
#include "libpicofe/readpng.h"
#include "qnx_common.h"

#define X_RES           1024
#define Y_RES           600
#define D_WIDTH			800
#define D_HEIGHT		600

int g_layer_x = (X_RES - D_WIDTH) / 2;
int g_layer_y = (Y_RES - D_HEIGHT) / 2;
int g_layer_w = D_WIDTH, g_layer_h = D_HEIGHT;

screen_context_t screen_ctx;
screen_window_t screen_win;
screen_buffer_t screen_buf[2];
screen_pixmap_t screen_pix;
screen_buffer_t screen_pbuf;

int analog_enabled = 0;

int size[2] = {0,0};
int rect1[4] = { 0, 0, 0, 0 };
int old_bpp = 0;
int stride = 0;
int handleKeyFunc(int sym, int mod, int scancode, uint16_t unicode, int event);
int handleDPadFunc(int angle, int event);
void pl_plat_blit_qnx(int doffs, const void *src, int w, int h, int sstride, int bgr24);
void pl_plat_clear_qnx(void);

struct tco_callbacks cb = {handleKeyFunc, handleDPadFunc, NULL, NULL, NULL, NULL};
tco_context_t tco_ctx;

volatile bool shutdown_emu;

#define APAD_X 0
#define APAD_Y 320
#define APAD_W 300
#define APAD_H 300
#define APAD_CENTRE_X APAD_X+(APAD_W/2)
#define APAD_CENTRE_Y APAD_Y+(APAD_H/2)
#define APAD_MAX 255
static int finger = -1;
int process_analog_pad(screen_event_t screen_event);
int process_keyboard_event(screen_event_t screen_event);

static void
handle_screen_event(bps_event_t *event)
{
    //int screen_val;
	int consume = 0;
	int type;

    screen_event_t screen_event = screen_event_get_event(event);
    screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &type);

    switch(type){
	case SCREEN_EVENT_MTOUCH_TOUCH:
	case SCREEN_EVENT_MTOUCH_RELEASE:
	case SCREEN_EVENT_MTOUCH_MOVE:
		//x="0" y="320" width="300" height="300"
		if(cfg_bb10.controllers[0].device == 1){
			if(analog_enabled)
			{
				consume = process_analog_pad(screen_event);
			}

			if(!consume)
			{
				tco_touch(tco_ctx, screen_event);
			}
		}
		break;
	case SCREEN_EVENT_KEYBOARD:
		if(cfg_bb10.controllers[0].device == 2){
			process_keyboard_event(screen_event);
		}
		break;
    }
}

int process_analog_pad(screen_event_t screen_event) {
	int type;
	int contactId;
	int pos[2];
	int consume = 0;
	//Values between 0 and 255
	int a_x=127, a_y=127;

	//The finger used on analog pad, -1 is no touch


	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &type);
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TOUCH_ID, (int*)&contactId);
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_SOURCE_POSITION, pos);

	//printf("Touch: posx: %d, poxy:%d\n", pos[0], pos[1]);fflush(stdout);

	//within the pad, consume event
	if(pos[0]>=APAD_X && pos[0]<=APAD_X+APAD_W && pos[1]>=APAD_Y && pos[1]<=APAD_Y+APAD_H) {
		consume = 1;
		if(finger == -1) {
			if(type == SCREEN_EVENT_MTOUCH_TOUCH) {
				//printf("First Touch in Analog Pad\n");fflush(stdout);
				finger = contactId;
				//Set analog values
				a_x = pos[0] - APAD_X;
				a_y = pos[1] - APAD_Y;
			}
		} else if (type == SCREEN_EVENT_MTOUCH_RELEASE) {
			//printf("Release in Analog Pad\n");fflush(stdout);
			//reset analog stick
			finger = -1;
			in_a1[0] = 127;
			in_a1[1] = 127;
		} else {
			//printf("Moving in Analog Pad\n");fflush(stdout);
			//set analog values
			a_x = pos[0] - APAD_X;
			a_y = pos[1] - APAD_Y;
		}
	} else {
		//Outside pad
		if(finger == contactId){
			consume = 1;
			if(type == SCREEN_EVENT_MTOUCH_RELEASE) {
				//printf("Release outside Analog Pad\n");fflush(stdout);
				finger = -1;
				//reset stick
				in_a1[0] = 127;
				in_a1[1] = 127;
			} else {
				//printf("Move outside Analog Pad\n");fflush(stdout);
				//set analog value
				a_x = pos[0] - APAD_X;
				a_y = pos[1] - APAD_Y;
			}
		}
	}

	if(finger > -1){
		if (a_x<0)a_x=0;
		if (a_x>APAD_W)a_x=APAD_W;

		if (a_y<0)a_y=0;
		if (a_y>APAD_H)a_y=APAD_H;


		in_a1[0] = ((float)a_x/APAD_W)*APAD_MAX;
		in_a1[1] = ((float)a_y/APAD_H)*APAD_MAX;

		//printf("Touch: x: %d, y:%d\n", in_a1[0], in_a1[1]);fflush(stdout);
	}

	return consume;
}

int process_keyboard_event(screen_event_t screen_event)
{
	int sym = 0;
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_SYM, &sym);
	int modifiers = 0;
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_MODIFIERS, &modifiers);
	int flags = 0;
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_FLAGS, &flags);
	int scan = 0;
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_SCAN, &scan);
	int cap = 0;
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_CAP, &cap);

	int c, b;

	printf("Keyboard sym: %d\n", sym&0xFF);fflush(stdout);

	//Single player only so far
	for( b = 0; b < 16; b++ )
	{
		if(cfg_bb10.controllers[0].buttons[b] == (sym&0xFF)){
			if(flags & KEY_DOWN){
				in_keystate |= 1 << b;
			} else {
				in_keystate &= ~(1<<b);
				//emu_set_action(SACTION_NONE);
			}
		}

	}
}

int handleKeyFunc(int sym, int mod, int scancode, uint16_t unicode, int event){

	//printf("sym: %d\n", sym);fflush(stdout);
	switch(sym){
	case DKEY_CIRCLE:
	case DKEY_SQUARE:
	case DKEY_CROSS:
	case DKEY_TRIANGLE:
	case DKEY_START:
	case DKEY_SELECT:
	case DKEY_R1:
	case DKEY_R2:
	case DKEY_L1:
	case DKEY_L2:
		switch(event){
		case TCO_KB_DOWN:
			in_keystate |= 1 << sym;
			break;
		case TCO_KB_UP:
			in_keystate &= ~(1<<sym);
			//emu_set_action(SACTION_NONE);
			break;
		default:
			break;
		}
		break;
		/*
	case 16:
		if(event == TCO_KB_DOWN){
			state_slot = 1;
			emu_set_action(SACTION_SAVE_STATE);
			in_keystate |= 1 << DKEY_START;
			//emu_save_state(1);
		} else {
			in_keystate &= ~(1<<DKEY_START);
		}
		break;
	case 17:
		if(event == TCO_KB_DOWN){
			state_slot = 1;
			emu_set_action(SACTION_LOAD_STATE);
			in_keystate |= 1 << DKEY_START;
			//emu_load_state(1);
		} else {
			in_keystate &= ~(1<<DKEY_START);
		}
		break;
		*/
	default:
		break;
	}

	return 0;
}

int handleDPadFunc(int angle, int event){
	static int key1 = -1, key2 = -1, old_key1 = -1, old_key2 = -1;

	//printf("angle: %d, key1: %d, key2: %d\n", angle, key1, key2);fflush(stdout);

	switch(event){
	case TCO_KB_DOWN:
		if (angle <= -68 && angle >= -113){
			key1 = DKEY_UP;
			key2 = -1;
		}
		else if (angle >= -22 && angle <= 22){
			key1 = DKEY_RIGHT;
			key2 = -1;
		}
		else if (angle >= 67 && angle <= 113){
			key1 = DKEY_DOWN;
			key2 = -1;
		}
		else if ((angle <= -157 && angle >= -180) ||
				(angle >= 157 && angle <= 180)){
			key1 = DKEY_LEFT;
			key2 = -1;
		}
		else if ((angle < -113) && (angle > -157)){
			key1 = DKEY_UP;
			key2 = DKEY_LEFT;
		} else if ((angle < 157) && (angle > 113)){
			key1 = DKEY_DOWN;
			key2 = DKEY_LEFT;
		} else if ((angle < 67) && (angle > 22)){
			key1 = DKEY_DOWN;
			key2 = DKEY_RIGHT;
		} else if ((angle < -22) && (angle > -68)){
			key1 = DKEY_UP;
			key2 = DKEY_RIGHT;
		}

		if((key1 == old_key1)&&(key2 == old_key2))
			return 0;

		in_keystate &= ~((1<<DKEY_UP)|(1<<DKEY_DOWN)|(1<<DKEY_LEFT)|(1<<DKEY_RIGHT));
		if (key1 >= 0)
			in_keystate |= 1 << key1;
		if (key2 >= 0)
			in_keystate |= 1 << key2;

		old_key1 = key1;
		old_key2 = key2;
		break;
	case TCO_KB_UP:
		in_keystate &= ~((1<<DKEY_UP)|(1<<DKEY_DOWN)|(1<<DKEY_LEFT)|(1<<DKEY_RIGHT));
		old_key1 = -1;
		old_key2 = -1;
		//emu_set_action(SACTION_NONE);
		break;
	default:
		break;
	}

	return 0;
}

void showTouchControls(){
	tco_showlabels(tco_ctx, screen_win);
}

void hideTouchControls(){
	tco_hidelabels(tco_ctx, screen_win);
}

static void
handle_navigator_event(bps_event_t *event) {
	navigator_window_state_t state;
	bps_event_t *event_pause = NULL;
	int rc;

    switch (bps_event_get_code(event)) {
    case NAVIGATOR_SWIPE_DOWN:
        fprintf(stderr,"Swipe down event");
        emu_set_action(SACTION_ENTER_MENU);
        break;
    case NAVIGATOR_EXIT:
        fprintf(stderr,"Exit event");
		shutdown_emu = true;
        /* Clean up */
		/*
		screen_stop_events(screen_ctx);
		bps_shutdown();
		tco_shutdown(tco_ctx);
		screen_destroy_window(screen_win);
		screen_destroy_context(screen_ctx);
		*/
        break;
    case NAVIGATOR_WINDOW_STATE:
    	state = navigator_event_get_window_state(event);

    	switch(state){
    	case NAVIGATOR_WINDOW_THUMBNAIL:
    		for(;;){
    			rc = bps_get_event(&event_pause, -1);
    			assert(rc==BPS_SUCCESS);

    			if(bps_event_get_code(event_pause) == NAVIGATOR_WINDOW_STATE){
    				state = navigator_event_get_window_state(event_pause);
    				if(state == NAVIGATOR_WINDOW_FULLSCREEN){
    					break;
    				}
    			} else if (bps_event_get_code(event_pause) == NAVIGATOR_EXIT){
    				fprintf(stderr,"Exit event");
					shutdown_emu = true;
					emu_set_action(SACTION_ENTER_MENU);
					break;
    			}
    		}
    		break;
    	case NAVIGATOR_WINDOW_FULLSCREEN:
			in_keystate &= ~(1<<DKEY_START);
    		break;
    	case NAVIGATOR_WINDOW_INVISIBLE:
    		break;
    	case NAVIGATOR_ORIENTATION_CHECK:
			//Signal navigator that we intend to resize
			navigator_orientation_check_response(event, false);
			break;
    	}
    	break;
    default:
        break;
    }
    fprintf(stderr,"\n");
}

static void
handle_event()
{
    int rc, domain;

    while(true){
    	bps_event_t *event = NULL;
		rc = bps_get_event(&event, 0);
		if(rc == BPS_SUCCESS)
		{
			if (event) {
				domain = bps_event_get_domain(event);
				if (domain == navigator_get_domain()) {
					handle_navigator_event(event);
				} else if (domain == screen_get_domain()) {
					handle_screen_event(event);
				}
			} else {
				break;
			}
		}
    }
}

void
qnx_init(void * screen_ctx_vp, char * group, char* win_id)
{
    const int usage = SCREEN_USAGE_NATIVE | SCREEN_USAGE_WRITE | SCREEN_USAGE_READ;
    int rc;
    char dir[255];
    char file[255];
    screen_ctx = *(screen_context_t*)screen_ctx_vp;

    //screen_create_context(&screen_ctx, 0);
    screen_create_window_type(&screen_win, screen_ctx, SCREEN_CHILD_WINDOW);

    screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage);

    const char *env = getenv("WIDTH");

	if (0 == env) {
		perror("failed getenv for WIDTH");
	}

	int width = atoi(env);

	env = getenv("HEIGHT");

	if (0 == env) {
		perror("failed getenv for HEIGHT");
	}

	int height = atoi(env);
	//rect1[2] = width;
	//rect1[3] = height;
	rect1[3] = width;
	rect1[2] = height;

	rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, rect1+2);
	if (rc) {
		perror("screen_set_window_property_iv");
	}

	rc = screen_create_window_buffers(screen_win, 2);
	if (rc) {
		perror("screen_create_window_buffers");
	}


    //screen_create_window_buffers(screen_win, 2);

	screen_join_window_group(screen_win, group);
    screen_set_window_property_cv(screen_win, SCREEN_PROPERTY_ID_STRING, strlen(win_id), win_id);

    screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)screen_buf);
    //screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, rect+2);



	int bg[] = { SCREEN_BLIT_COLOR, 0x00000000, SCREEN_BLIT_END };
	screen_fill(screen_ctx, screen_buf[0], bg);
	screen_fill(screen_ctx, screen_buf[1], bg);

    screen_create_pixmap( &screen_pix, screen_ctx );

    int format = SCREEN_FORMAT_RGB565;
    screen_set_pixmap_property_iv(screen_pix, SCREEN_PROPERTY_FORMAT, &format);

    int pix_usage = SCREEN_USAGE_WRITE | SCREEN_USAGE_NATIVE;
    screen_set_pixmap_property_iv(screen_pix, SCREEN_PROPERTY_USAGE, &pix_usage);

    rc = tco_initialize(&tco_ctx, screen_ctx, cb);
    if(rc != TCO_SUCCESS){
    	printf("TCO: Init error\n");
    }

    //PlayBook
    printf("Detected Resolution: W:%d, H:%d\n", rect1[2], rect1[3]);
    if(rect1[2] == 1024){
    	printf("Loading PlayBook Controls...\n");
    	strcpy(file, "controls_1024_600.xml");
    //L-Series
    } else if (rect1[2] == 1280){
    	printf("Loading L-Series Controls...\n");
    	strcpy(file, "controls_1280_720.xml");
    }
    fflush(stdout);

    strcpy(dir, "shared/misc/pcsx-rearmed-bb/cfg/");
	strcat(dir, file);

    if(access(dir, F_OK) != 0){
    	strcpy(dir, "app/native/");
    	strcat(dir, file);
    }
    rc = tco_loadcontrols(tco_ctx, dir);

    if (rc != TCO_SUCCESS){
    	printf("TCO: Load Controls Error\n");fflush(stdout);
    }

    tco_showlabels(tco_ctx, screen_win);

    int z = 5;
	if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &z) != 0) {
		return;
	}

	int vis = 1;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_VISIBLE, &vis);

    screen_post_window(screen_win, screen_buf[0], 1, rect1, 0);

    /* Signal bps library that navigator and screen events will be requested */
    //bps_initialize();
    screen_request_events(screen_ctx);
    navigator_request_events(0);

    pl_rearmed_cbs.only_16bpp = 1;
    pl_plat_blit = pl_plat_blit_qnx;
    pl_plat_clear = pl_plat_clear_qnx;

    int idleMode = SCREEN_IDLE_MODE_KEEP_AWAKE;
    screen_set_window_property_iv( screen_win, SCREEN_PROPERTY_IDLE_MODE, &idleMode);

    return;
}

void plat_finish()
{
	//Force quit?
}

void menu_loop(void)
{
}

extern int pl_vout_scale;
void pl_plat_blit_qnx(int doffs, const void *src, int w_, int h_, int sstride, int bgr24){

	int fb_offs = 0;
	uint8_t *dest;
	int i = h_;

	dest = (uint8_t *)pl_vout_buf;

	if(pl_vout_buf != NULL){
		if (bgr24){
			for (; i-- > 0; dest += stride, fb_offs += sstride)
			{
				bgr888_to_rgb565(dest, (uint16_t*)src + fb_offs, w_ * 3);
			}
#ifdef __ARM_NEON__
		} else if (soft_filter == SOFT_FILTER_SCALE2X && pl_vout_scale == 2) {
			neon_scale2x_16_16(src, (void *)dest, w_,
						sstride*2, stride, h_);
		} else if (soft_filter == SOFT_FILTER_EAGLE2X && pl_vout_scale == 2) {
			neon_eagle2x_16_16(src, (void *)dest, w_,
						sstride*2, stride, h_);
#endif
		} else {
			for (; i-- > 0; dest += stride, fb_offs += sstride)
			{
				bgr555_to_rgb565(dest, (uint16_t*)src + fb_offs, w_ * 2);
			}
		}
	}
}

void pl_plat_clear_qnx(void){
	int bg[] = { SCREEN_BLIT_COLOR, 0x00000000, SCREEN_BLIT_END };
	screen_fill(screen_ctx, pl_vout_buf, bg);
}

void *plat_gvideo_set_mode(int *w_, int *h_, int *bpp_)
{
	printf("plat_gvideo_set_mode: w:%d, h:%d, bpp:%d\n", *w_, *h_, *bpp_);fflush(stdout);

	if((*w_ == size[0]) && (*h_ == size[1]) && (old_bpp == *bpp_)){
		return pl_vout_buf;
	} else {
		screen_destroy_pixmap_buffer( screen_pix );
	}

	size[0] = *w_;
	size[1] = *h_;
	old_bpp = *bpp_;

	screen_set_pixmap_property_iv(screen_pix, SCREEN_PROPERTY_BUFFER_SIZE, size);

	int format;

	//if(bpp == 16)
		format = SCREEN_FORMAT_RGB565;
	//else if(bpp == 24)
		//format = SCREEN_FORMAT_RGBX8888;
	screen_set_pixmap_property_iv(screen_pix, SCREEN_PROPERTY_FORMAT, &format);
	screen_create_pixmap_buffer(screen_pix);
	screen_get_pixmap_property_pv(screen_pix, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_pbuf);

	screen_get_buffer_property_iv(screen_pbuf, SCREEN_PROPERTY_STRIDE, &stride);
	screen_get_buffer_property_pv(screen_pbuf, SCREEN_PROPERTY_POINTER, (void **)&pl_vout_buf);

	return pl_vout_buf;
}

void *plat_gvideo_flip(void)
{
	int hg[] = {
			SCREEN_BLIT_SOURCE_X, 0,
			SCREEN_BLIT_SOURCE_Y, 0,
			SCREEN_BLIT_SOURCE_WIDTH, size[0],
			SCREEN_BLIT_SOURCE_HEIGHT, size[1],
			SCREEN_BLIT_DESTINATION_X, rect1[0]+112,
			SCREEN_BLIT_DESTINATION_Y, rect1[1],
			SCREEN_BLIT_DESTINATION_WIDTH, rect1[2]-(112*2),
			SCREEN_BLIT_DESTINATION_HEIGHT, rect1[3],
			//SCREEN_BLIT_SCALE_QUALITY,SCREEN_QUALITY_FASTEST,
			SCREEN_BLIT_END
		};

	screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)screen_buf);
	screen_blit(screen_ctx, screen_buf[0], screen_pbuf, hg);

	screen_post_window(screen_win, screen_buf[0], 1, rect1, 0);

	handle_event();

	return pl_vout_buf;
}

int omap_enable_layer(int enabled)
{
	return 0;
}

void menu_notify_mode_change(int w, int h, int bpp)
{
}

void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
	return NULL;
}

void plat_step_volume(int is_up)
{
}

void plat_trigger_vibrate(void)
{
}

void plat_minimize(void)
{
}

void plat_gvideo_open(int is_pal)
{
}

void plat_gvideo_close(void)
{
}

void qnx_cleanup(){
	screen_stop_events(screen_ctx);
	bps_shutdown();
	tco_shutdown(tco_ctx);
	//screen_destroy_window(screen_win);
	screen_destroy_context(screen_ctx);
}
