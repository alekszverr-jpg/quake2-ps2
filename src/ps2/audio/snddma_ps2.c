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
#define PS2_AUDIO_CHUNK_BYTES 4096
#define PS2_AUDIO_FIRST_CHUNK_BYTES 2048
#define PS2_AUDIO_MIN_MIXAHEAD 0.35F

static unsigned long long s_submittedFrames;
static int s_audsrvStarted;
static int s_audioStatus;
static int s_lastError;
static int s_lastQueuedBytes;
static int s_outputRate;
static unsigned int s_playCalls;
static int s_minQueuedBytes;
static char s_silenceChunk[PS2_AUDIO_CHUNK_BYTES] __attribute__((aligned(64)));

enum
{
    PS2_AUDIO_NOT_ATTEMPTED = 0,
    PS2_AUDIO_IOP_FAILED,
    PS2_AUDIO_AUDSRV_FAILED,
    PS2_AUDIO_FORMAT_FAILED,
    PS2_AUDIO_ACTIVE,
    PS2_AUDIO_STREAM_FAILED
};

/* Read-only diagnostics used by GAME -> TEST MAP. They intentionally avoid
 * RPC calls: the real-time path records the latest queue state for the menu. */
int PS2_SNDDMA_GetStatus(void)      { return s_audioStatus; }
int PS2_SNDDMA_GetLastError(void)   { return s_lastError; }
int PS2_SNDDMA_GetQueuedBytes(void) { return s_lastQueuedBytes; }
int PS2_SNDDMA_GetRate(void)        { return s_outputRate; }
unsigned int PS2_SNDDMA_GetPlayCalls(void) { return s_playCalls; }
int PS2_SNDDMA_GetMinQueuedBytes(void) { return s_minQueuedBytes; }

qboolean SNDDMA_Init(void)
{
    audsrv_fmt_t format;
    int result;
    int rate;
    int availableBytes;

    memset(&dma, 0, sizeof(dma));
    s_submittedFrames = 0;
    s_audsrvStarted = 0;
    s_audioStatus = PS2_AUDIO_NOT_ATTEMPTED;
    s_lastError = 0;
    s_lastQueuedBytes = 0;
    s_outputRate = 0;
    s_playCalls = 0;
    s_minQueuedBytes = -1;

    if (!PS2_InitAudioIop())
    {
        s_audioStatus = PS2_AUDIO_IOP_FAILED;
        Com_Printf("WARNING: PS2 audio modules unavailable; continuing without sound.\n");
        return false;
    }

    result = audsrv_init();
    if (result != AUDSRV_ERR_NOERROR)
    {
        s_audioStatus = PS2_AUDIO_AUDSRV_FAILED;
        s_lastError = result;
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
        s_audioStatus = PS2_AUDIO_FORMAT_FAILED;
        s_lastError = result;
        Com_Printf("WARNING: audsrv_set_format failed (%d: %s).\n",
                   result, audsrv_get_error_string());
        SNDDMA_Shutdown();
        return false;
    }

    /*
     * After set_format, audsrv exposes only the free half of its IOP ring and
     * starts consuming the other half. A sound restart can take longer than
     * that reserve at 22 kHz; if it reaches empty, audsrv may confuse empty
     * with full and replay stale data forever. Fill every currently writable
     * byte with silence before doing any remaining EE-side setup. Real mixed
     * PCM will replace the primer progressively as playback creates space.
     */
    availableBytes = audsrv_available();
    while (availableBytes >= PS2_BYTES_PER_FRAME)
    {
        int bytesToSend;
        int sentBytes;

        bytesToSend = availableBytes;
        if (bytesToSend > PS2_AUDIO_CHUNK_BYTES)
            bytesToSend = PS2_AUDIO_CHUNK_BYTES;
        bytesToSend &= ~(PS2_BYTES_PER_FRAME - 1);
        if (bytesToSend <= 0)
            break;

        sentBytes = audsrv_play_audio(s_silenceChunk, bytesToSend);
        if (sentBytes <= 0)
            break;

        s_playCalls++;
        availableBytes -= sentBytes;
    }

    audsrv_set_volume(MAX_VOLUME);

    dma.channels = PS2_DMA_CHANNELS;
    dma.samples = PS2_DMA_SAMPLES;
    dma.submission_chunk = 256; /* stereo frames; power of two for snd_dma.c */
    dma.samplepos = 0;
    dma.samplebits = PS2_DMA_SAMPLE_BITS;
    dma.speed = rate;
    dma.buffer = Z_Malloc(dma.samples * PS2_BYTES_PER_SAMPLE);
    memset(dma.buffer, 0, dma.samples * PS2_BYTES_PER_SAMPLE);
    s_outputRate = rate;
    s_audioStatus = PS2_AUDIO_ACTIVE;

    /*
     * audsrv can repeat stale ring-buffer data after an underrun. Keep a
     * larger reserve than desktop Quake II so low-frame-rate PS2 scenes can
     * replenish the IOP stream before it reaches that state.
     */
    if (s_mixahead != NULL && s_mixahead->value < PS2_AUDIO_MIN_MIXAHEAD)
        Cvar_SetValue("s_mixahead", PS2_AUDIO_MIN_MIXAHEAD);

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
    s_lastQueuedBytes = queued;
    if (s_submittedFrames > 0 &&
        (s_minQueuedBytes < 0 || queued < s_minQueuedBytes))
        s_minQueuedBytes = queued;

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
    int waitResult;

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

    /*
     * The startup silence intentionally fills the IOP ring. Wait once for a
     * small writable window so the first real mixed block is guaranteed to
     * enter the queue before the primer drains. All later submissions remain
     * non-blocking, including normal low-frame-rate gameplay.
     */
    if (pendingFrames > 0 && s_submittedFrames == 0 &&
        availableBytes < PS2_AUDIO_FIRST_CHUNK_BYTES)
    {
        waitResult = audsrv_wait_audio(PS2_AUDIO_FIRST_CHUNK_BYTES);
        if (waitResult == AUDSRV_ERR_NOERROR)
            availableBytes = audsrv_available();
        else
        {
            s_audioStatus = PS2_AUDIO_STREAM_FAILED;
            s_lastError = waitResult;
            return;
        }
    }

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
            s_audioStatus = PS2_AUDIO_STREAM_FAILED;
            s_lastError = audsrv_get_error();
            Com_DPrintf("PS2 audio: audsrv_play_audio failed (%d: %s).\n",
                        sentBytes, audsrv_get_error_string());
            break;
        }

        s_playCalls++;
        s_submittedFrames += (unsigned int)(sentBytes / PS2_BYTES_PER_FRAME);
        pendingFrames = paintedFrames - s_submittedFrames;
        availableBytes -= sentBytes;
    }
}
