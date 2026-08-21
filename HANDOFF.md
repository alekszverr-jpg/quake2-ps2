# Development handoff

This file is the durable context for continuing development in a new Codex
task. Read it together with [ROADMAP.md](ROADMAP.md) and [CHANGELOG](CHANGELOG)
before changing renderer, audio or memory-management code.

## Repository checkpoint

- Workspace: `C:\Users\user\Documents\quake2-ps2`
- Active Git branch: `main`
- Fork used for pushes and releases:
  `https://github.com/alekszverr-jpg/quake2-ps2.git`
- Read-only upstream reference:
  `https://github.com/glampert/quake2-ps2.git`
- Current version: `0.1.0-alpha.65`
- Current code commit before this handoff: `fb20b62`
  (`Sort opaque entities by texture`)
- Current published release:
  `https://github.com/alekszverr-jpg/quake2-ps2/releases/tag/v0.1.0-alpha.65`
- Alpha.65 PROFILE ELF SHA-256:
  `110F3491F9B5C200EEC88F130B0B88CAB158FA1AE784856A59338D917CF4014E`

The next code release should normally be `0.1.0-alpha.66`. This handoff-only
checkpoint does not advance `VERSION`.

## Workspace safety

The worktree intentionally contains many untracked files, including the user's
game/source data, downloaded CI builds and local tools:

- `Quake2Game/`
- `Quake2Source/`
- `build/ci-*/`
- `tools/`
- local archives and diagnostic images under `build/`

These files belong to the user. Do not delete, clean, move or commit them. Stage
only the exact source/documentation files changed for a release. In particular,
never use `git clean`, `git reset --hard` or broad recursive deletion here.

`*.elf` is ignored. After a successful PROFILE CI build, copy the downloaded
`quake2-profile.elf` to the project root for the user's convenient testing even
though that root copy is not committed.

## User workflow and test constraints

- The user currently tests only `quake2-profile.elf`.
- PCSX2 is the usual short-cycle target; important renderer and stability fixes
  are periodically checked on a retail PS2.
- The user cannot type into an in-game console. Every test switch, diagnostic,
  map selector or cheat needed for validation must be reachable by gamepad/menu.
- Preserve the gamepad Give All test shortcut and the map-selection menu.
- Lead with a ready-to-test ELF/release and concise test positions. The user can
  provide screenshots and observed FPS/counters.
- Keep `ROADMAP.md`, `CHANGELOG`, `VERSION` and the README version badge in sync
  with code releases. Publish numbered GitHub prereleases.

## Validated project state

The port boots from host files in PCSX2 and from FAT32 USB through uLaunchELF on
real hardware. The following major paths have been implemented and validated:

- Quake II BSP levels, MD2 models, weapons, enemies and gameplay interaction
- Dual-stick controls: left stick movement, right stick view
- Doors, buttons, lifts, map transitions and a gamepad map selector
- Textures and mipmaps without the former wall strips or camera-motion shimmer
- Coloured/static/animated lighting, player/entity lighting and muzzle flashes
- Sky, water, glass transparency, particles and transparent surface ordering
- Conservative BSP, face, brush-entity and MD2 frustum culling
- AI player detection and close-range attacks
- PROFILE/release build separation and gamepad-controlled diagnostics

Renderer changes must be checked for regressions in all of the following:

- black/foreign texture strips caused by stale or reused GS pages
- sky pixels leaking through BSP cracks
- dark stripes caused by crack-seal depth placement
- triangle-shaped particles over water or sky
- glass losing transparency or showing unrelated textures
- stretched/corrupted weapon geometry
- missing doors, moving brush models or entities at screen edges
- differences between PCSX2 and retail-PS2 ordering

## Known unfinished areas

- Heavy scenes remain EE-bound and often run around 15-20 FPS in the PROFILE
  build; the light Base1 position is around 26-31 FPS with diagnostics enabled.
- Map transitions can still exhaust EE memory. This is not proven to be only a
  delayed unload: retained level/model/texture allocations and fragmentation
  must be measured under the P8 plan.
- Low-rate game audio works in stereo but can crackle. The HIGH audio mode is
  silent or can hang and remains deferred until frame pacing is healthier.
- Music is not implemented. The roadmap records a separate user-supplied,
  offline-converted PS2 ADPCM streaming design using IOP/SPU2 double buffers;
  copyrighted CD audio must not be bundled.
- Campaign-wide rendering, cinematics, long-session memory stability and all
  special effects are not yet fully validated.

## Latest PROFILE result: Alpha.65 awaiting validation

Alpha.65 begins P2 by sorting safe opaque MD2/sprite runs by the actual resolved
GS texture rather than only Quake II's existing model/raw-skin pointer order.
This accounts for `skinnum` and animated sprite frames. Brush models and
`RF_DEPTHHACK` weapons remain ordering barriers, and translucent entities keep
their original order. PROFILE `EntTex0`/`EntTex` expose eligible texture runs
before and after sorting.

Alpha.64's supplied PCSX2 results validated two-buffer overlap:

| Scene | FPS | Batches | VIFchain | VUWait us | VUchunk | VIFqw | VUvert | VUstate |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base1 light | 20 | 73 | 22 | 101 | 191 | 2520 | 12078 | 98 |
| Base1 outdoor | 20 | 63 | 13 | 148 | 227 | 2639 | 16047 | 97 |
| Base1 heavy | 15 | 86 | 33 | 265 | 329 | 3665 | 23340 | 125 |

The screenshots showed correct geometry, textures, sky and weapons. The closest
outdoor comparison retained 13 chains and reduced `VUWait` from 232 to 148 us.
Alpha.65 CI passed for `fb20b62`, but entity texture sorting still needs PCSX2
and retail-PS2 validation.

## Exact next task: validate opaque entity texture sorting

Test the published Alpha.65 PROFILE ELF in the same Base1 positions. Record
`EntTex0`, `EntTex`, `TexUp`, `TexDMA`, `VRAMwait`, `VRAMsync`, `Ent us`, FPS
and the established VIF/geometry counters. Inspect weapons, enemies, sprites,
brush alpha and all renderer regression paths listed above.

The relevant files are:

- `src/ps2/renderer/render_view.cpp`
- `src/ps2/renderer/render_view.h`
- `src/ps2/renderer/ref.cpp`
- `src/ps2/renderer/gs.cpp`
- `src/ps2/renderer/vram.cpp`

Alpha.65 behaviour:

1. Collect eligible opaque MD2/sprite entity draws between ordering barriers.
2. Resolve the actual texture used by `AliasSkin` or the sprite's current frame.
3. Count original texture runs as `EntTex0`, sort by `Texture*`, then count
   submitted runs as `EntTex`.
4. Flush each sorted run before a brush model or depth-hack view weapon.
5. Draw translucent entities afterward in their unchanged input order.

Expected Alpha.65 validation result:

- `EntTex` should be no greater than `EntTex0`; entity-heavy scenes should show
  whether actual grouping reduces `TexUp`, DMA or VRAM reuse waits.
- Entity/triangle/VIF counts should remain comparable to Alpha.64.
- No missing entities, changed weapon depth, sprite errors, brush-alpha changes
  or texture/transparency/sky/particle regressions.

If `EntTex0 == EntTex` in representative entity-heavy scenes, Quake II's existing
model sort is already sufficient and this extra sort should be reconsidered.
If it reduces runs but upload churn remains high, continue P2 with a bounded
visible-frame texture working set/pre-upload plan rather than more VIF changes.

## Build and release procedure

The GitHub Actions `build` workflow is the reproducible toolchain. During this
optimization cycle it builds only `quake2-profile.elf` and uploads it in the
`quake2-ps2-build` artifact.

Local commands when PS2DEV/PS2SDK are configured:

```sh
make BUILD=profile
```

For every numbered code release:

1. Update source plus `VERSION`, README badge, `CHANGELOG` and `ROADMAP.md`.
2. Stage only those exact files and commit on `main`.
3. Push to remote `fork`.
4. Wait for the GitHub Actions `build` run to pass.
5. Download its artifact into a new `build/ci-<short-commit>/` directory.
6. Compute and record the PROFILE ELF SHA-256.
7. Copy `quake2-profile.elf` to the repository root.
8. Publish `v<version>` as a GitHub prerelease with only the PROFILE ELF asset
   unless the user requests another package.

Never publish copyrighted `baseq2` data, soundtrack files or the user's local
game directories.

## Suggested prompt for the new task

> Continue the Quake II PS2 port in this workspace. Read HANDOFF.md completely,
> then ROADMAP.md and CHANGELOG. Check git status and recent commits. Review the
> Alpha.65 Base1 PROFILE results and renderer screenshots. Evaluate
> EntTex0/EntTex and the upload/VRAM counters, then continue or revert the P2
> sort according to measured benefit without weakening PATH1/PATH3 ordering.
> Build and publish only the numbered PROFILE prerelease, copy the successful
> quake2-profile.elf to the project root, and do not touch untracked game data.
