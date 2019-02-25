/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"

#if XASH_SOUND == SOUND_CTR

#include <3ds.h>
#include "sound.h"

#define SAMPLE_RATE   	SOUND_DMA_SPEED
#define NUM_SAMPLES 	4096
#define SAMPLE_SIZE		4
#define BUFFER_SIZE 	NUM_SAMPLES * SAMPLE_SIZE

static int sound_initialized = 0;
static byte *audio_buffer;
static ndspWaveBuf wave_buf;

extern dma_t dma;

qboolean SNDDMA_Init( void *hInst )
{
	sound_initialized = 0;

  	if(ndspInit() != 0)
    	return false;

    audio_buffer = linearAlloc(BUFFER_SIZE);

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnReset(0);
	ndspChnWaveBufClear(0);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, (float)SAMPLE_RATE);
	ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

	memset(&wave_buf, 0, sizeof(wave_buf));
	wave_buf.data_vaddr = audio_buffer;
	wave_buf.nsamples 	= BUFFER_SIZE / 4;
	wave_buf.looping	= 1;

	/* Fill the audio DMA information block */

	dma.format.width = 2;
	dma.format.speed = SAMPLE_RATE;
	dma.format.channels = 2;
	dma.samples = BUFFER_SIZE / 2;
	dma.buffer = audio_buffer;
	dma.sampleframes = dma.samples / dma.format.channels;

	ndspChnWaveBufAdd(0, &wave_buf);

	dma.initialized = true;
	
	sound_initialized = 1;

	return true;
}

int SNDDMA_GetDMAPos(void)
{
	if(!sound_initialized)
		return 0;

	return ndspChnGetSamplePos(0);
}

int SNDDMA_GetSoundtime( void )
{
	static int	buffers, oldsamplepos;
	int		samplepos, fullsamples;
	
	fullsamples = dma.samples / 2;

	// it is possible to miscount buffers
	// if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if( samplepos < oldsamplepos )
	{
		buffers++; // buffer wrapped

		if( paintedtime > 0x40000000 )
		{	
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds();
		}
	}

	oldsamplepos = samplepos;

	return (buffers * fullsamples + samplepos / 2);
}

/*
==============
SNDDMA_BeginPainting

Makes sure dma.buffer is valid
===============
*/
void SNDDMA_BeginPainting( void )
{
	// TODO: add a mutex here or something
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
Also unlocks the dsound buffer
===============
*/
void SNDDMA_Submit( void )
{
	if(sound_initialized)
		DSP_FlushDataCache(audio_buffer, BUFFER_SIZE);
}

/*
===========
S_PrintDeviceName
===========
*/
void S_PrintDeviceName( void )
{
	Msg( "Audio: 3DS\n" );
}

/*
===========
S_Activate
Called when the main window gains or loses focus.
The window have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void S_Activate( qboolean active )
{

}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
}

#endif
