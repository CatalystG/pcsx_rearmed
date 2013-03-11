
#include <stdio.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <sys/asoundlib.h>
#include "out.h"

static snd_pcm_t *handle = NULL;
static snd_pcm_channel_params_t params;
static snd_pcm_channel_setup_t setup;
static snd_pcm_channel_status_t status;
static int true_buffer_size;

// SETUP SOUND
static int qnx_init(void)
{

	int err, card, dev;

	if ((err = snd_pcm_open_preferred(&handle, &card, &dev,
	               SND_PCM_OPEN_PLAYBACK)) < 0)
	{
        printf("Audio open error: %s\n", snd_strerror(err));fflush(stdout);
        return -1;
	}

	if((err = snd_pcm_nonblock_mode(handle, 1))<0)
	{
	    printf("Can't set blocking mode: %s\n", snd_strerror(err));fflush(stdout);
	    return -1;
	}

	if ((err = snd_pcm_plugin_set_disable (handle, PLUGIN_DISABLE_MMAP)) < 0)
	{
		printf("Can't disable MMAP plugin: %s\n", snd_strerror(err));fflush(stdout);
	    return -1;
	}

	params.channel = SND_PCM_CHANNEL_PLAYBACK;
	params.mode = SND_PCM_MODE_BLOCK;

	params.format.interleave = 1;
	params.format.format = SND_PCM_SFMT_S16_LE;
	params.format.rate = 44100;
	params.format.voices = 2;

	params.start_mode = SND_PCM_START_DATA;
	//params.start_mode = SND_PCM_START_FULL;
	params.stop_mode = SND_PCM_STOP_STOP;

	params.buf.block.frag_size = 4096;
	//params.buf.block.frag_size = 1024;
	params.buf.block.frags_min = 1;
	params.buf.block.frags_max = 19;


	if ((err = snd_pcm_plugin_params(handle, &params)) < 0)
	{
		printf("Channel Parameter Error: %s\n", snd_strerror(err));fflush(stdout);
		return -1;
	}

	setup.channel = SND_PCM_CHANNEL_PLAYBACK;

	if ((err = snd_pcm_plugin_setup(handle, &setup)) < 0)
	{
		printf("Channel Parameter Read Back Error: %s\n", snd_strerror(err));fflush(stdout);
		return -1;
	}

	true_buffer_size = setup.buf.block.frag_size * 19;
	printf("alsa buf size: %d\n", true_buffer_size);fflush(stdout);


	if ((err = snd_pcm_plugin_prepare(handle, SND_PCM_CHANNEL_PLAYBACK)) < 0)
	{
		printf("Channel Prepare Error: %s\n", snd_strerror(err));fflush(stdout);
		return -1;
	}

	return 0;
}

// REMOVE SOUND
static void qnx_finish(void)
{
     if(handle != NULL) {
         snd_pcm_close(handle);
         handle = NULL;
     }
}

// GET BYTES BUFFERED
static int qnx_busy(void)
{
    unsigned long buffered;
    int err;

    if (handle == NULL)                                 // failed to open?
        return 1;

    if ((err = snd_pcm_plugin_status(handle, &status)) < 0)
    {
    	printf("Error reading status: %s\n", snd_strerror(err));fflush(stdout);
    	return 1;
    }
    //printf("ALSA status: %d\n", status.status);
    //printf("ALSA buffered: %d\n", status.count);
    buffered = status.count;

    if (buffered > (true_buffer_size/2))                           // can we write in at least the half of fragments?
        return 1;                                 // -> no? wait
    else
    	return 0;                                         // -> else go on
}

// FEED SOUND DATA
static void qnx_feed(void *pSound, int lBytes)
{
	int err;

    if (handle == NULL) return;

    if ((err = snd_pcm_plugin_status(handle, &status)) < 0)
    {
	    printf("Error reading status: %s\n", snd_strerror(err));fflush(stdout);
    }

    if(status.status == SND_PCM_STATUS_UNDERRUN ||
       status.status == SND_PCM_STATUS_OVERRUN){
    	//printf("xRUN in FeedAudio: %d\n", status.status);fflush(stdout);
    	if ((err = snd_pcm_plugin_prepare(handle, SND_PCM_CHANNEL_PLAYBACK)) < 0)
		{
			printf("Channel Prepare Error: %s\n", snd_strerror(err));fflush(stdout);
			return;
		}
    }
    //TODO: Should this be lBytes/4?
    if ((err = snd_pcm_plugin_write(handle,pSound,lBytes)) < 0)
	{
		printf("Error writing PCM: %s\n", snd_strerror(err));fflush(stdout);
		return;
	}
}

void out_register_qnx(struct out_driver *drv)
{
	drv->name = "qnx";
	drv->init = qnx_init;
	drv->finish = qnx_finish;
	drv->busy = qnx_busy;
	drv->feed = qnx_feed;
}
