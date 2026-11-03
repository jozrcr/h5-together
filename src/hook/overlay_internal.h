// The overlay's parts (overlay.h), shared between its files:
//   overlay.cpp            the clients, the ImGui frame, fonts, the screen on top, keys;
//   overlay_input.cpp      modal panels: the window procedure, the ImGui feed, the DirectInput mute;
//   overlay_textures.cpp   pictures from the game's data;
//   overlay_cursor.cpp     the frame drawn just under the game's own cursor;
//   overlay_test_panel.cpp the test panel.
#pragma once
#include <windows.h>

struct IDirect3DDevice9;

namespace overlay {
namespace internal {

// overlay_textures.cpp
bool InstallTextures();  // false: no pictures (the panels use plain colours)
void ReleaseTextures();
// Is this texture (from GameTexture) one of the game's pictures of light to add
// (alpha 0 everywhere, the colours added to what is behind; a button's
// highlight)? The overlay draws those added (overlay.cpp), as the game does.
bool AddedPicture(const void *texture);

// overlay_input.cpp, from the frame (overlay.cpp), in this order:
// before the clients are asked: closes the modal panel if its screen is gone,
// Esc was pressed, or no frame was drawn for too long; puts the window
// procedure in once the game's window is known.
void ModalFrameStart();
// Is a modal panel open (after ModalFrameStart)?
bool ModalOpen();
// Just before ImGui::NewFrame: the mouse and keys for ImGui (when a modal
// panel is open; otherwise ImGui gets no mouse at all).
void FeedImGui(float back_buffer_w, float back_buffer_h);
// After the frame was drawn with the modal panel open.
void ModalDrawn();
// A client crashed: its panel closes.
void CloseModalOf(const char *owner, const char *why);
// For the test panel: where the cursor and the buttons come from, this frame.
const char *InputInfo();

// overlay_cursor.cpp
// With the frame callbacks: the game's cursor's "draw" goes through us. False
// when the game is not as expected (then the frame is drawn at Present only,
// over the cursor).
bool InstallCursorHook();
// At Present: the frame is over (the cursor's last draw in it is known now).
void CursorFrameShown();

// overlay.cpp
// Is this window the game's, and in front?
bool GameInFront();
// For overlay_cursor.cpp: is there a frame to draw (a client said so at the
// last Present) that was not drawn yet since then?
bool FrameWaiting();
// Draws it now, inside the game's scene (just before the game draws its
// cursor), with what the clients said at the last Present; they are not asked
// here (what they do when asked may act on the game, not while it draws).
void DrawUnderCursor(IDirect3DDevice9 *device);

}  // namespace internal
}  // namespace overlay
