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
- Current version: `0.1.0-alpha.62`
- Current code commit before this handoff: `f3afb66`
  (`Optimize repeated VU1 batch state`)
- Current published release:
  `https://github.com/alekszverr-jpg/quake2-ps2/releases/tag/v0.1.0-alpha.62`
- Alpha.62 PROFILE ELF SHA-256:
  `7EAED5142189F9FEB341C3CC1195102EE4365FCE637EA5E6C6746865CA42CEFD`

The next code release should normally be `0.1.0-alpha.63`. This handoff-only
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

## Latest PROFILE result: Alpha.62

Alpha.62 seeds invariant GIF/GS batch state into both VU1 XTOP halves. Later
chunks in the same draw refresh only the header and changing draw tag. It also
adds PROFILE `VIFqw`, `VUvert` and `VUstate` counters.

The supplied PCSX2 screenshots rendered correctly. Representative values:

| Scene | FPS | VIFchain | VUchunk | VIFqw | VUvert | VUstate |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base1 light | 26 | 70 | 175 | 1911 | 10938 | 93 |
| Base1 outdoor | 20 | 74 | 249 | 2500 | 17493 | 112 |
| Base1 heavy | 15 | 89 | 324 | 3107 | 23079 | 128 |

The VIF source-chain payload fell roughly 25-34%, but FPS did not materially
change. This confirms state payload was worth removing while the dominant cost
is still EE world/entity preparation and many small submissions.

## Exact next task: P1 larger VIF submissions

Continue the first unchecked P1 item: replace the synchronous wait at the end
of every compatible `vu1::DrawTriangles` call with a deferred packet containing
multiple 3D draws. The immediate goal is fewer `VIFchain` submissions, not an
aggressive asynchronous MFIFO redesign.

The relevant files are:

- `src/ps2/renderer/vu1.cpp`
- `src/ps2/renderer/vu1.h`
- `src/ps2/renderer/render_view.cpp`
- `src/ps2/renderer/gs.cpp`
- `src/ps2/renderer/vif_packet.*`

Current behaviour:

1. `vu1::DrawTriangles` flushes pending 2D.
2. It calls `gs::EnsureTextureResident`.
3. It references caller-owned vertex scratch memory in one VIF chain.
4. It appends `FLUSH`, submits and waits before returning.

A conservative implementation design already identified during inspection:

1. Add a modest, 128-byte-aligned internal VU vertex staging area. Copy each
   submitted chunk into it so deferred packets never reference world/MD2
   scratch memory that the caller immediately overwrites.
2. Put frame constants inline in the chain, or otherwise keep each referenced
   constants block stable until completion. The current singleton
   `s_constants` cannot safely be referenced by multiple deferred draws.
3. Accumulate compatible chunks in `s_drawPacket` and add a public
   `vu1::Flush()` that submits/waits only when the packet/staging area fills or
   at an ordering boundary.
4. If constants/MVP change inside a chain, insert the required VIF barrier
   before overwriting fixed VU data-memory addresses.
5. Flush deferred PATH1 geometry *before* any texture upload, dirty-texture
   rewrite or VRAM eviction/reuse. A queued `XGKICK` must never sample a page
   after PATH3 has replaced it.
6. Keep `gs::EnsureTextureResident` synchronous for this first iteration.
   Texture pre-upload scheduling belongs to P2.
7. Flush all deferred 3D at the end of `view::RenderFrame`, before later HUD/2D
   commands can change PATH ordering.
8. Preserve Alpha.62's rule that each XTOP half receives a full state block
   before later chunks use the compact header-only form.
9. Assert that no deferred chain leaks across `BeginFrameStats`/frame boundaries.

Do not merely remove `Wait()`: caller vertex buffers and constants are reused
immediately, textures can be evicted between calls, and PATH1 has priority over
PATH3. Violating any of those constraints previously produced severe geometry
and texture corruption on both PCSX2 and real hardware.

Expected Alpha.63 PROFILE result:

- `VIFchain` should become lower than `Batches` in the three Base1 positions.
- `VUvert` and rendered triangle counts should remain comparable to Alpha.62.
- `VUstate` may remain similar; the first goal is chain merging.
- No texture, transparency, sky, particle, weapon or moving-brush regressions.
- FPS may improve only slightly because the captures are primarily EE-bound.

If safe multi-draw staging becomes too invasive, stop with an instrumented
checkpoint rather than weakening PATH/VRAM synchronization.

## Build and release procedure

The GitHub Actions `build` workflow is the reproducible toolchain. It builds
both variants and uploads one `quake2-ps2-build` artifact.

Local commands when PS2DEV/PS2SDK are configured:

```sh
make BUILD=release
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
> then ROADMAP.md and the top of CHANGELOG. Check git status and recent commits.
> Continue the documented P1 larger-VIF-submission task conservatively, keeping
> PATH1/PATH3 texture ordering and caller-buffer lifetimes correct. Build and
> publish only the numbered PROFILE prerelease for testing, copy the successful
> quake2-profile.elf to the project root, and do not touch untracked game data.

