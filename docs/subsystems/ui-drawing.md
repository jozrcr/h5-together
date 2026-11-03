# UI drawing: the 2D layer and the game's cursor

Status: **static reading** (2026-11-03: Ghidra on the vanilla 3.1 exe), used by `src/hook/overlay_cursor.cpp` and checked there in a running game (see the end). Addresses are in the vanilla 3.1 exe; they are the same in `MMH55_Utility(_64).exe` (MMH5.5's runtime patches, `tools/inventory/mmh55_patch_sites.py`, touch none of them).

Why we looked: our overlay (Dear ImGui, `src/hook/overlay*.cpp`) was drawn just before `IDirect3DDevice9::Present`, after the whole frame, so it covered the game's own mouse cursor. To put our panels **under** the cursor we needed to know where the cursor is drawn.

## The cursor

The game draws its own cursor by default (console variable `ui_use_hwcursor = 0`, at `0x00fe6d78`; 1 = Windows' cursor). It is one object, `NUI::CCursor`, a static at `0x00fe6da8` (`0x00836750` returns it; it is called from 94 places, mostly to change the cursor's kind).

| `NUI::CCursor` vftable `0x00e49494` | What |
|---|---|
| slot 2, `0x00836440` | **draw** `__thiscall(CCursor *, NUI::CUIVisitor *)`, ret 4. Nothing if the kind (`+0x48`) is 4 (`CURSOR_INVISIBLE`, the 5th name of the `CURSOR_*` list at `0x00516900`) or the cursor is off the screen. With Windows' cursor it only places that one (`0x00c3b130`, `0x00c3b110`). Else it builds a quad from the cursor's picture (`+0x3c`, its size at `+0x5c`/`+0x60`) and the rectangle of slot 4, and gives it to the visitor (visitor slot 6). |
| slot 4, `0x008363a0` | the cursor's rectangle: its place minus the hotspot (`+0x34`, `+0x38`) |
| slot 7, `0x00835ce0` | update: `GetCursorPos` + `ScreenToClient` into `0x00fe6da0`/`0x00fe6da4` (floats, client pixels), or the other way (`SetCursorPos`) when the game moves the cursor itself from DirectInput |
| slot 8, `0x00835e50` | the edge scrolling direction (the cursor at a border of the window) |
| slot 9, `0x00835b90` | `ui_use_hwcursor` |

The draw is called through the vftable only (no direct call), from the screens' frame functions: `0x0066cfe0` (a screen's render, address taken in two vftables, 67 callers) and `0x006b1310`. Both make a `NUI::CUIVisitor` on the stack (`0x00838290`), draw what they draw, then the cursor, near the end of the frame.

**The cursor is drawn once per screen, every frame.** The game keeps a stack of screens (the list at `[0x00fe719c]`). Each screen's draw (a vftable slot; e.g. `0x00722310` for the waiting room, `0x006f6c70` for the multiplayer background, `0x00694540` for the main menu) first draws the screen under it (`0x008384d0` gives it, then its slot `+0x2c`), then calls `0x0066cfe0` for itself, which draws its windows and the cursor. So in the waiting room the cursor is drawn three times per frame (main menu, background, waiting room), the lower screens first; only the last one, the top screen's, shows. Robot log (2026-11-03): three calls per frame, return addresses `0x006945d3`, `0x006f6cd8`, `0x00722380`, each with its own visitor on the stack, at the same addresses every frame while the screens stay the same. Right after the cursor, `0x0066cfe0` checks whether its screen is the top one (`0x008385a0() == this + 0x10 + [[this+0x10]+8]`).

## The 2D layer

| Object | vftable | Notes |
|---|---|---|
| `NUI::CUIVisitor` | `0x00e49980` | made by `0x00838290` (the screen size, the 2D view from the service `0x1109bc00`, `0x00a4aa50`). `+4`: the 2D view (ref-counted). Slot 6 `0x008380e0`: a picture, through the view's `I2DGameView` part (`[+4] + 0xc`, its slot 2). |
| `NGScene::C2DGameView` | `0x00e89b3c` | its `I2DGameView` part at `+0xc` (vftable `0x00e89b2c`); `+0x10` is the scene (read at `0x00c1b3ef`, through that part's `+4`) |
| `NGScene::C2DScene` | `0x00e89b90` | slot 7 `0x00c1c200` and slot 8 `0x00c1c100`: add a quad. Two quad renderers inside, `+0xc` and `+0x108`; `+0x204` is the current one (or 0). Changing renderer first draws the old one's waiting batch. Slot 11 `0x00c1c0a0`: draw the batch of the `+0xc` renderer. |
| NGfx quads renderer | (inside the scene) | `+0xe8`: the waiting batch (0 = none). `0x00a46bc0` `__thiscall(renderer)`: sets its shaders and states and draws the batch now (`0x009ae590` → `0x00c31400` → `0x00c362e0`, which calls the Direct3D device), then empties it. |

So 2D pictures reach the device in batches of one texture, soon after they are given: a batch is drawn when the texture or the renderer changes, or when asked. At any moment, one batch may still be waiting. Below that, NGfx also gathers indices in a dynamic buffer (`0x00f79acc`, `0x00c362e0`) and calls `DrawIndexedPrimitive` in `0x00c357a0`; `0x00a46bc0` ends with `0x009ad050` → `0x00c311f0` → `0x00c357a0`, so after it the batch really is on the device.

NGfx's Direct3D device is the global `0x010029a8` (`IDirect3DDevice9 *`; read at `0x00c36393`, released and zeroed by `0x00a40270`; `0x010029a4` is the `IDirect3D9 *`).

## Pictures of light (a button's hover)

Some interface pictures are made to be **added** to what is behind, not blended by alpha: their `.(Texture).xdb` says `<ConversionType>CONVERT_TRANSPARENT_ADD</ConversionType>`, and the `.dds` has alpha 0 in every pixel, the light in its colours. Examples: `UI/Common/DlgButtons/Button_122_34_Highlight.(Texture).dds` (a dialog button's hover: a violet-blue glow, average added colour about R 22, G 0, B 56) and `UI/Common/ComboBox_201_33/ComboBtnHighlight.(Texture).dds`. Drawn with ordinary alpha blending they are invisible. `C2DScene` has a second quad renderer (`+0x108`) that is chosen by a value of the quad (`0x00c1c100`); probably the one for these pictures (not checked).

## Cursor shapes

The cursor's kind (`+0x48`) is set by slot 5 (`0x00836380` → `0x00836220`): it takes the kind's entry of `/UI/RefTables/Cursors.xdb` (`0x00518d90`): the picture (`+0x3c`), the hotspot (`+0x34`, `+0x38`) and, for Windows' cursor, a file loaded with `LoadCursorFromFileA` (so changing the kind is not cheap). On the adventure map the game sets the kind from what is under the cursor on the map, also while one of our panels covers it.

## What the overlay does with it

`src/hook/overlay_cursor.cpp`: the cursor's draw slot (`0x00e4949c`) points to us. At the frame's last cursor draw (the one that was last in the frame before: same place among the calls, same visitor address), before calling the game's draw: find the scene from the visitor (each vftable checked), have its waiting batch drawn (`0x00a46bc0` on `[scene+0x204]`), check that NGfx's device is the one whose frames we see and that it draws on its back buffer, then draw the overlay's ImGui frame inside the game's scene. The cursor then lands on top. Frames without a cursor draw, or where a check fails, are drawn at `Present` as before. The pictures of light are drawn added (`overlay.cpp`, `AddLightBlending`).

## Checked in a running game

Robot runs, 2026-11-03 (`MMH55_Utility_64.exe`, windowed 1008×729 back buffer, the robot's `mouse` step, `tools/robot/README.md`):

- Drawing under the **first** cursor draw of the frame hid our panels completely in the waiting room (the screens above covered them): that is how the "once per screen" was found. Under the last one, the "Rules..." button, the rules panel and the looks panel show, with the cursor above them (waiting room, adventure map).
- With the earlier build (drawn at `Present`), the cursor vanished under the rules panel.
- The game's own buttons do not light up in robot runs (probably because their hover comes from window messages, which the robot's background window does not get), so the game's own hover look was not compared; the highlight's colours above are from the file.
