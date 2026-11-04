# Map drawing performance (study, 2026-11-04)

Status: **two fixes built**: lighter animations (`[camera] anim_saver`, on by default: the owner's choice, as it acts only when the map is already slow) and a bigger vertex pool (`[camera] vertex_pool=4`, on: an exact speed-up). Zoomed out on the owner's map: 33-36 -> 70 frames a second. The owner's question: the adventure map is slow on big maps, more so zoomed out (their game of 2026-11-03: 2560x1440, `[camera] zoom_out=250`, a huge 6-player map, 16 to 60 frames a second for long stretches). The rest of the performance picture (clicks, MMH5.5's scripts, AI turns): [performance.md](performance.md).

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

The setting picks N by itself, every half second: below 55 frames a second two half seconds in a row, one step slower (up to 1 in 4); above 80 two half seconds in a row, one step back; after a pause of over 1.5 s (loading, a save), every frame again. Only in frames that animate 300 skeletons or more (the adventure map has over 1,000, a battle a few dozen): battles and towns keep every frame. Robot `20261104-153211`: zoomed out it reaches 1 in 4 in about 2 s, **48 frames a second instead of 36**; at the normal zoom it stays at every frame. Display only (the world never reads the scene), each player chooses; on by default (the owner's choice; 0 = the game's own way), in the Options page's camera group ("Lighter animations" / "Animations allégées").

## The second fix: a bigger vertex pool (`[camera] vertex_pool=4`)

Why the models' vertices were built again every frame (the research trace `[camera] vb_trace=1`, `src/mods/vertex_pool.cpp`; robot runs `20261104-162420` to `-170705`):

- A model's builder (`NGScene::CVBCombiner`, vtable `0x00e54e58`) is a node: slot 7 `0x00c68db0` "must I build again?", slot 8 `0x00c6a360` -> `0x00c6a130` builds (its parts' vertices, 20 bytes each, into one buffer at `+0x14`). Its inputs: edge A (`+0x54`/`+0x58`, an `NGScene::CLightStateNode`), B (`+0x40`/`+0x44`, an `NGScene::CPerMaterialCombiner`: the parts), C (`+0x4c`/`+0x50`, an `NGScene::CAnimationWatch`), and the flags `+0x5c`, `+0x5d` ("build the vertices").
- The watch (vtable `0x00e8f48c`, all its work in slot 7 `0x00c69bf0`) watches the parts' `NGScene::CBind` nodes (vtable `0x00e66464`, slot 7 `0x009dc420`: its input `+0x20`/`+0x24`, which follows a skeleton). When one moved on, that part's cached vertices are freed (`+0x0c`..`+0x14`, `+0x74`/`+0x78`) and the watch reports a change; the builder's bounds (slot 9 `0x008f1da0` -> `0x00c696a0`) then set `+0x5d`.
- **But most builds had nothing to do with that.** The scene walk (`0x009ba480`) asks each builder (the normal path, its call at `0x009ba5dd`), then builds it again at once if its buffer (`+0x14`) is missing or dying (`0x009ba654`). Zoomed out, that second path made 9,300 of the 14,700 builds a second.
- The buffers die because **the shared dynamic vertex pool is too small**: the pools are made with the size `0C0000h` (`mov ecx` at `0x00c324a6`, then `0x00c3a180`, the pools of map `0x00f79ab4`). When an allocation (`0x00c3a660`) finds the next block still in use by the last `gfx_lag` frames (`0x009ae800`), the game moves its frame counter `0x00f79a50` on by 5 (`0x00c315d0`), locks the buffer with discard and calls `0x00c357a0`: everything in it dies. Zoomed out: **18.5 times a second**; at the normal zoom: 0.

| Robot (the owner's map and settings, 2560x1440, zoomed out to 250%) | Discards | Builds | Frames per second |
|---|---|---|---|
| the game's pool, every frame (`-164815`) | 18.5/s | 14,700/s, 316 ms/s | 36-37 |
| pool x8, every frame (`-165219`) | 0 | 7,100/s, 110 ms/s | 49 |
| the game's pool, lighter animations (`-165548`) | 24/s | | 48 |
| pool x2, lighter animations (`-165940`) | 0 | | 69 |
| **pool x4, lighter animations** (`-170127`) | 0 | | **70-71** |
| pool x8, lighter animations (`-165735`) | 0 | | 66-67 |

Video memory (Direct3D's estimate of what is free): x2 about 27 MB more, x4 about 77 MB, x8 about 260 MB. The default is x4: room for zooming out further (up to 400%) and bigger maps. An exact speed-up (the same picture: the game only stops throwing away its own work), so it is on; `vertex_pool=1` is the game's own size. With computer turns and the default settings (`-170705`): 21 computer turns, zoomed out 44 to 74 frames a second, no crash.

The model watch held per model (every skeleton of a model on the same frame, `vb_hold`, tried and dropped): the builds fell by 70% but the frame rate did not move, as the discards rebuilt everything anyway.

## Next

1. ~~Vertex building~~: done (the pool, above). What is left zoomed out: the scene walk, the skeletons that still move, the driver.
2. **Models nobody sees**: skip the skeletons of models under the black fog of war (and off screen), which the game animates anyway.
3. ~~DXVK on Windows~~: tried (below), a small gain only on this PC.
4. **Hitches**: a frame of 1.4 to 1.7 s at each new day in single player (likely the autosave); in multiplayer, the owner's game had a frame of 100 to 400 ms every few seconds (MMH5.5's scripts: fixed by `[coop] script_rhythm` in 0.3.2-test2; to check in the owner's next logs).

## DXVK on Windows (tried, 2026-11-04)

DXVK turns the game's Direct3D 9 into Vulkan; the Linux players already run through it (Proton). Tried on the owner's PC with its OK: DXVK 3.1.1 (the official release, `dxvk-3.1.1.tar.gz` sha256 `40565b4a...`, its 32-bit `x32/d3d9.dll`, sha256 `265888c3...`) next to the test copy's exe, AMD Radeon RX 7900 XT (the card with the screen; the PC also has an RX 7900 XTX), AMD driver 32.0.32015.2008. It runs: our overlay draws, no crash, DXVK's log clean (one unhandled render state, 26).

| Robot run | Without DXVK | With DXVK |
|---|---|---|
| Normal zoom, camera still, the robot's settings (`20261104-160347` / `-160155`) | 187 fps | 185 fps |
| Zoomed out, the owner's settings, every frame (`-151859` / `-155729`) | 35.7 | 39.9 |
| the same, 1 in 2 | 43.2 | 47.8 |
| the same, 1 in 3 | 47.8 | 51.8 |
| the same, 1 in 4 | 50.0 | 53.9 |

About +10% zoomed out, nothing at the normal zoom: the game's own CPU work (skeletons, vertex building, the scene walk) is the limit, not the driver. Not worth shipping or asking players to install for now; a PC with a slow Direct3D 9 driver might gain more.

