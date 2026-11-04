# Map drawing performance (study, 2026-11-04)

Status: **study**, with a first fix built (`[camera] anim_saver`, off by default). The owner's question: the adventure map is slow on big maps, more so zoomed out (their game of 2026-11-03: 2560x1440, `[camera] zoom_out=250`, a huge 6-player map, 16 to 60 frames a second for long stretches). The rest of the performance picture (clicks, MMH5.5's scripts, AI turns): [performance.md](performance.md).

## How it was measured

Single-player robot runs on the test copy (`C:\Games\H5_test_k`), the owner's 6-player epic map (`/Maps/RMG/65012FB6-2895-4CC6-8D8B-A2EFA4F80401/map.xdb`, the file `Maps/6P-epic-3v1v1v1.h5m`), 2560x1440 windowed, `--camera-setting zoom_out=250`, the camera zoomed out with Page Down (`--step "40|dik|d1 5000"`), no AI turns during the measure (`--end-turn-every 400`), `--profile-all` (frame times every 10 s and the main thread sampled every ~2 ms). The owner's graphics settings (their `user_a2.cfg`): `gfx_quality 1`, `gfx_fsaa 4`, `gfx_anisotropic_filter 8`, `gfx_depth_tex_resolution 1024`. Separate runs differ by up to ±15% (the camera does not stop at exactly the same place): compare within one run when possible (`anim_every=0` below).

## What costs what

| Zoom | Frames per second |
|---|---|
| the game's farthest (normal) | 120 to 165 |
| 250% (`zoom_out=250`), nothing moving | 33 to 37 |

**The CPU is the limit, not the graphics card.** Zoomed out, 65% of the main thread's samples are in the game's own code, about 19% in the graphics driver and 8% in Windows. The game's own graphics switches barely change it (one run each, zoomed out): particles off 37, shadows off 36, `adventure_anim_blend 0` 36, `gfx_lod_switch_distance 50` 36, `gfx_quality 0` 35, anti-aliasing off 37, the robot's low settings 37, the owner's 33.

Where a zoomed-out frame (28 ms) goes, from the samples' call stacks (the parts overlap a little):

| Part | ms per frame | Code |
|---|---|---|
| The scene walk (visibility, then what is drawn) | ~12 | `0x009ba480` and below |
| Building the models' vertices on the CPU | 8.6 | `NGScene::CVBCombiner` (vtable `0x00e54e58`, slot 8 `0x00c6a360`) -> `0x00c6a130` -> `0x00c6ba50` -> `0x00c6b7f0` -> `0x00c6b4a0` -> `0x00c6afa0` / `0x00c6b0b0`: hand-written MMX code that unpacks and turns each vertex's normal (`0x00c68210`, a square-root table at `0x010ce990`) and blends vertices by their bones (`0x00cf9be0` -> `0x00cfa830` -> `0x00cf92c0`, `0x00cf8b40`) |
| The graphics driver and Windows | 10.8 | |
| Skeletons (the pose of each animated model) | 4.7 | `NAnimation::CSkeletonAnimator` (vtable `0x00e4e7dc`): slot 7 `0x00893600` "must it be computed again?", slot 8 `0x00893720` computes it (Granny, `granny2.dll`) |
| Particles | 2.5 | `NGScene::CStandardParticleEffect` `0x00bf4e80`, `CParticleAnimator` `0x00b10030` |

**About 1,270 skeletons ask to be animated every frame**, zoomed in or out alike (about 1,040 at the normal zoom), and on day 2 most of the zoomed-out view is still black (unexplored): the game animates models nobody can see.

### The scene's update graph

The scene is a graph of nodes. Each frame a node is checked by the helper `0x00578a90` (and the same few lines inlined elsewhere): if its last check (`+0x0c`) is not the scene's update stamp (`0x00f482ac`, which moves on **twice per frame**), it asks the node "must you be computed again?" (vtable slot 7, `+0x1c`); if yes, it computes it (slot 8, `+0x20`) and raises its version (`+0x10`). What depends on a node keeps the version it last saw and is computed again when it changed.

## The first fix: lighter animations (`[camera] anim_saver=1`)

`src/mods/anim_throttle.cpp`: the skeleton's slot 7 says yes on one frame in N only (frames = stamp / 2; the skeletons spread over the N frames by a hash of their address; never before a skeleton's second computation, or Granny reads a pose that does not exist yet: robot `20261104-150631`, a crash at the menu's scene). Its model is then not rebuilt either, and less goes to the graphics card.

One run, the same camera, N changed every 20 s (`anim_every=0`, `20261104-151859`):

| Each model animated on | fps | Skeletons | Driver and Windows |
|---|---|---|---|
| every frame (the game) | 35.7 | 4.7 ms | 10.8 ms |
| 1 frame in 2 | 43.2 (+21%) | 2.7 ms | 7.5 ms |
| 1 frame in 3 | 47.8 (+34%) | 1.7 ms | 6.2 ms |
| 1 frame in 4 | 50.0 (+40%) | 1.4 ms | 5.9 ms |

The setting picks N by itself, every half second: below 55 frames a second two half seconds in a row, one step slower (up to 1 in 4); above 80 two half seconds in a row, one step back; after a pause of over 1.5 s (loading, a save), every frame again. Only in frames that animate 300 skeletons or more (the adventure map has over 1,000, a battle a few dozen): battles and towns keep every frame. Robot `20261104-153211`: zoomed out it reaches 1 in 4 in about 2 s, **48 frames a second instead of 36**; at the normal zoom it stays at every frame. Display only (the world never reads the scene), each player chooses; off by default (the game's own way), in the Options page's camera group ("Lighter animations" / "Animations allégées").

## Next

1. **Vertex building** (~7 ms a frame left with the fix): `CVBCombiner` builds most models' vertices again each frame even when their skeleton did not change. Keeping each model's last result while its skeleton's version is the same would save most of it. Needs a closer look at the combiner's records (`0x00c6b4a0` and around).
2. **Models nobody sees**: skip the skeletons of models under the black fog of war (and off screen), which the game animates anyway.
3. **The driver** (~6 to 11 ms): DXVK (Direct3D 9 on Vulkan) usually costs less CPU for old Direct3D 9 games; the Linux players already run through it (Proton). A test on Windows needs a download (the owner's OK) and a file next to the test copy's exe.
4. **Hitches**: a frame of 1.4 to 1.7 s at each new day in single player (likely the autosave); in multiplayer, the owner's game had a frame of 100 to 400 ms every few seconds (MMH5.5's scripts: fixed by `[coop] script_rhythm` in 0.3.2-test2; to check in the owner's next logs).
