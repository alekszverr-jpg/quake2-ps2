#pragma once
/* Exclusive full-frame phases. Disabled builds compile the hooks away. */
enum { PS2_FRAME_OTHER, PS2_FRAME_SERVER, PS2_FRAME_CLIENT,
       PS2_FRAME_RENDER, PS2_FRAME_FINISH, PS2_FRAME_PRESENT, PS2_FRAME_PHASES };
#if PS2_PROFILE
#ifdef __cplusplus
extern "C" {
#endif
void PS2_FramePhase(int phase);
void PS2_FrameSoundIO(unsigned ticks);
#ifdef __cplusplus
}
#endif
#else
#define PS2_FramePhase(phase) ((void)0)
#define PS2_FrameSoundIO(ticks) ((void)0)
#endif
