/* ================================================================================================
 * File: snddma_ps2.c
 * Brief: Quake II SNDDMA backend for PS2. The stock mixer writes stereo PCM into
 *        an EE ring buffer; completed frames are queued non-blockingly to audsrv,
 *        which streams them through the IOP to SPU2.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "client/client.h"
#include "client/snd_loc.h"

#include <audsrv.h>

#include <string.h>

/* C entry point supplied by ps2/system/iop_boot.cpp. */
extern int PS2_InitAudioIop(void);

/* Power-of-two count of interleaved mono samples: 8192 stereo frames. */
#define PS2_DMA_SAMPLES       16384
#define PS2_DMA_CHANNELS      2
#define PS2_DMA_SAMPLE_BITS   16
#define PS2_BYTES_PER_SAMPLE  (PS2_DMA_SAMPLE_BITS / 8)
#define PS2_BYTES_PER_FRAME   (PS2_DMA_CHANNELS * PS2_BYTES_PER_SAMPLE)

/* Keep each EE -> IOP RPC small. audsrv internally splits larger calls too,
 * but explicit chunks let Submit stop immediately when its queue is full. */
#define PS2_AUDIO_CHUNK_BYTES 2048

static unsigned long long s_submittedFrames;
static int s_audsrvStarted;

qboolean SNDDMA_Init(void)
{
    audsrv_fmt_t format;
    int result;
    int rate;

    memset(&dma, 0, sizeof(dma));
    s_submittedFrames = 0;
    s_audsrvStarted = 0;

    if (!PS2_InitAudioIop())
    {
        Com_Printf("WARNING: PS2 audio modules unavailable; continuing without sound.\n");
        return false;
    }

    result = audsrv_init();
    if (result != AUDSRV_ERR_NOERROR)
    {
        Com_Printf("WARNING: audsrv_init failed (%d: %s); continuing without sound.\n",
                   result, audsrv_get_error_string());
        return false;
    }
    s_audsrvStarted = 1;

    /* Respect Quake II's existing quality selector. Its default is 11 kHz;
     * cinematics temporarily request 22 kHz and restart the sound backend. */
    rate = (s_khz != NULL && s_khz->value >= 22.0F) ? 22050 : 11025;
    format.freq = rate;
    format.bits = PS2_DMA_SAMPLE_BITS;
    format.channels = PS2_DMA_CHANNELS;
    result = audsrv_set_format(&format);
    if (result != AUDSRV_ERR_NOERROR)
    {
        Com_Printf("WARNING: audsrv_set_format failed (%d: %s).\n",
                   result, audsrv_get_error_string());
        SNDDMA_Shutdown();
        return false;
    }

    audsrv_set_volume(MAX_VOLUME);

    dma.channels = PS2_DMA_CHANNELS;
    dma.samples = PS2_DMA_SAMPLES;
    dma.submission_chunk = 64; /* stereo frames; power of two for snd_dma.c */
    dma.samplepos = 0;
    dma.samplebits = PS2_DMA_SAMPLE_BITS;
    dma.speed = rate;
    dma.buffer = Z_Malloc(dma.samples * PS2_BYTES_PER_SAMPLE);
    memset(dma.buffer, 0, dma.samples * PS2_BYTES_PER_SAMPLE);

    Com_Printf("PS2 audio: audsrv PCM %d Hz, stereo 16-bit, %d KB EE ring.\n",
               rate, (dma.samples * PS2_BYTES_PER_SAMPLE) / 1024);
    return true;
}

int SNDDMA_GetDMAPos(void)
{
    unsigned long long submittedBytes;
    unsigned long long playedBytes;
    int queued;

    if (!s_audsrvStarted || dma.buffer == NULL)
        return 0;

    queued = audsrv_queued();
    if (queued < 0)
        queued = 0;

    submittedBytes = s_submittedFrames * PS2_BYTES_PER_FRAME;
    if ((unsigned long long)queued > submittedBytes)
        playedBytes = 0;
    else
        playedBytes = submittedBytes - (unsigned long long)queued;

    dma.samplepos = (int)((playedBytes / PS2_BYTES_PER_SAMPLE) &
                          (unsigned long long)(dma.samples - 1));
    return dma.samplepos;
}

void SNDDMA_Shutdown(void)
{
    if (s_audsrvStarted)
    {
        audsrv_stop_audio();
        audsrv_quit();
        s_audsrvStarted = 0;
    }

    if (dma.buffer != NULL)
    {
        Z_Free(dma.buffer);
        dma.buffer = NULL;
    }
    memset(&dma, 0, sizeof(dma));
    s_submittedFrames = 0;
}

void SNDDMA_BeginPainting(void)
{
}

void SNDDMA_Submit(void)
{
    unsigned long long paintedFrames;
    unsigned long long pendingFrames;
    int availableBytes;

    if (!s_audsrvStarted || dma.buffer == NULL)
        return;

    paintedFrames = (unsigned long long)(unsigned int)paintedtime;
    if (paintedFrames < s_submittedFrames)
    {
        /* Sound restart or Quake's very-long-session time rebasing. */
        s_submittedFrames = paintedFrames;
    }

    pendingFrames = paintedFrames - s_submittedFrames;
    availableBytes = audsrv_available();
    while (pendingFrames > 0 && availableBytes >= PS2_BYTES_PER_FRAME)
    {
        unsigned long long sampleIndex;
        int framesToRingEnd;
        int framesToSend;
        int maxAvailableFrames;
        int sentBytes;
        const char * source;

        sampleIndex = (s_submittedFrames * PS2_DMA_CHANNELS) &
                      (unsigned long long)(dma.samples - 1);
        framesToRingEnd = (dma.samples - (int)sampleIndex) / PS2_DMA_CHANNELS;
        maxAvailableFrames = availableBytes / PS2_BYTES_PER_FRAME;
        framesToSend = (pendingFrames > (unsigned long long)framesToRingEnd)
                       ? framesToRingEnd : (int)pendingFrames;
        if (framesToSend > maxAvailableFrames)
            framesToSend = maxAvailableFrames;
        if (framesToSend > PS2_AUDIO_CHUNK_BYTES / PS2_BYTES_PER_FRAME)
            framesToSend = PS2_AUDIO_CHUNK_BYTES / PS2_BYTES_PER_FRAME;
        if (framesToSend <= 0)
            break;

        source = (const char *)dma.buffer + ((int)sampleIndex * PS2_BYTES_PER_SAMPLE);
        sentBytes = audsrv_play_audio(source, framesToSend * PS2_BYTES_PER_FRAME);
        if (sentBytes <= 0)
        {
            Com_DPrintf("PS2 audio: audsrv_play_audio failed (%d: %s).\n",
                        sentBytes, audsrv_get_error_string());
            break;
        }

        s_submittedFrames += (unsigned int)(sentBytes / PS2_BYTES_PER_FRAME);
        pendingFrames = paintedFrames - s_submittedFrames;
        availableBytes -= sentBytes;
    }
}
