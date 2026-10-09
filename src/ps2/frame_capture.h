#pragma once
/* Exclusive full-frame phases. Disabled builds compile the hooks away. */
enum { PS2_FRAME_OTHER, PS2_FRAME_SERVER, PS2_FRAME_CLIENT,
       PS2_FRAME_RENDER, PS2_FRAME_FINISH, PS2_FRAME_PRESENT, PS2_FRAME_PHASES };
enum { PS2_RENDER_BEGIN, PS2_RENDER_VIEW, PS2_RENDER_3D, PS2_RENDER_HUD, PS2_RENDER_PARTS };
enum { PS2_SCENE_OTHER, PS2_SCENE_CAMERA, PS2_SCENE_OBJECTS, PS2_SCENE_EFFECTS, PS2_SCENE_PARTICLES, PS2_SCENE_LIGHTS, PS2_SCENE_STYLES, PS2_SCENE_SORT, PS2_SCENE_PARTS };
#if PS2_PROFILE
#ifdef __cplusplus
extern "C" {
#endif
void PS2_FramePhase(int phase);
void PS2_FrameRenderPart(int part);
void PS2_FrameScenePart(int part);
void PS2_FrameSceneCounts(int entities, int particles, int lights);
void PS2_FrameSoundIO(unsigned ticks);
#ifdef __cplusplus
}
#endif
#else
#define PS2_FramePhase(phase) ((void)0)
#define PS2_FrameRenderPart(part) ((void)0)
#define PS2_FrameScenePart(part) ((void)0)
#define PS2_FrameSceneCounts(entities, particles, lights) ((void)0)
#define PS2_FrameSoundIO(ticks) ((void)0)
#endif
