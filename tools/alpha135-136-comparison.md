# Alpha135 / Alpha136 matched benchmark

Prepared 2026-10-26. This comparison measures the per-draw MD2 colour cache.
It does not change source version, game configuration or savegames.

## Prepared state

- Source remains Alpha136, implementation f1cfcb0.
- Both projects' `quake2-profile.elf` now contain Alpha136 after the paired results.
- Documents `quake2-alpha135-baseline.elf` is the original baseline.
- Both projects' `quake2-alpha136-candidate.elf` preserve the verified candidate.
- Alpha135 SHA256: `6F71212AE937D38F58833157AF9BEBDB1BD7E3D16D1F5934EC9A77989354E5D4`.
- Alpha136 SHA256: `CDE0116AB3ADD7110C9A79C835D9EC932E8ACD34C1379D2D77795C1125109415`.
- Local pre-A configuration snapshot: `build/alpha135-136-comparison/config-before-a.cfg`.

## Procedure

1. Restart the game using the installed Alpha135 ELF. Go to
   **Game -> benchmark (3 runs)**. This action automatically plays demo1 three
   times. Do not choose model/world profiling or a six-run comparison.
2. Do not press controls during the demo: any button cancels it. Photograph
   the completed FPS results, including all three rows, demo frame ranges,
   Combined FPS/ms, and Far light / Flash detail labels.
3. After phase A is received, verify its complete frame ranges and settings,
   then install the preserved Alpha136 candidate as `quake2-profile.elf` in both
   projects, checking the hash. Do not rebuild or change any settings in between.
4. Restart and run the same **benchmark (3 runs)** once. Capture the same page.
   Compare combined FPS/ms and per-run variation; renderer details can help
   explain where any difference occurred. A small result inside run-to-run
   variation is inconclusive and may need a reversed-order repeat.

The normal benchmark automatically enables timedemo, disables diagnostic
overlays/log notifications and detailed world/model/light timers, and restores
its saved settings on completion/cancellation. It excludes loading but has no
extra warm-up pass. Do not add a warm-up on only one version.

The saved config currently has world lights ON, far light detail reduced,
flash detail reduced, mipmaps ON, automatic mip level, video mode2, FOV90,
viewsize100. Preserve these settings between versions; the exact same demo
and assets must be used. Source: src/client/cl_benchmark.c and menu.c.

## Results

| Version | Run1 FPS | Run2 FPS | Run3 FPS | Combined FPS | Combined ms |
| --- | --- | --- | --- | --- | --- |
| Alpha135 |43.36|43.35|43.37|43.36|23.06|
| Alpha136 |43.54|43.55|43.55|43.55|22.96|

Both screenshots show688frames per run, demo frames195..882, demo1 uncapped,
VSync OFF, far light reduced and flash detail reduced. Approximate per-run
seconds are15.87 on135 and15.80 on136. All136runs exceed all135runs;
within-group range is0.02FPS on135 and0.01FPS on136. Combined improvement is
0.19FPS (~0.44%) /0.10ms less per frame. These are rounded displayed results.

Retain Alpha136 as a small positive result on this demo; both root ELFs restored
from the hash-verified136candidate. Baseline135remains available. This does not
prove a universal gain or resolve the live100ms frame peak; build order/session
variation is not eliminated by this single A/B pair. Visual acceptance remains
pending; no visual claim inferred from benchmark screenshots.

The earlier live firing captures changed workload: Alpha13546.07ms vs
Alpha13641.71ms average, but Entities11.51ms vs11.65ms while World12.89ms vs
10.40ms. They do not establish a benefit from the model colour cache.
