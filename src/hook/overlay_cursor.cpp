// The overlay's frame, drawn just under the game's own cursor (see overlay.cpp,
// "How it draws"), so that the cursor stays above our panels.
//
// The game draws its own cursor (ui_use_hwcursor = 0, the default) as a picture
// of its frame: NUI::CCursor (vftable 0x00e49494; the game's one cursor object
// is at 0x00fe6da8), slot 2 "draw", 0x00836440 __thiscall(CCursor *,
// NUI::CUIVisitor *), ret 4. A screen's frame (0x0066cfe0, 0x006b1310) calls
// it once the screen's windows are drawn, and every screen on the game's stack
// of screens does so: the top screen draws the screens under it first, so the
// last cursor drawn, the top screen's, is the one that shows (see CursorDrawHook).
// Our panels used to be drawn after the whole frame, just before Present, so
// they covered the cursor. Now that vftable slot points to us: at the frame's
// last cursor draw we draw the overlay's frame first, then call the game's
// draw, and the cursor lands on top. (With Windows' cursor, ui_use_hwcursor =
// 1, that draw only places Windows' cursor; our frame is drawn there all the same.)
//
// The game's 2D pictures go through NGScene::C2DScene (vftable 0x00e89b90): it
// collects the quads of one texture in its current NGfx quads renderer (the
// pointer at scene+0x204: scene+0xc or scene+0x108, or none) and draws them
// as one batch when the texture or the renderer changes (0x00a46bc0
// __thiscall(renderer): "draw the waiting batch", what the scene calls itself
// when it changes renderer). When the cursor comes, the last windows' batch may
// still be waiting: we have it drawn first, with that same function, so that
// it lands under our panels and not over them.
//
// The scene, from the visitor the cursor gets (made by 0x00838290): visitor+4
// is the 2D view (NGScene::C2DGameView, vftable 0x00e89b3c; the visitor draws
// through its I2DGameView part at +0xc, vftable 0x00e89b2c, see 0x008380e5),
// and the view's +0x10 is the scene (read through that part at 0x00c1b3ef).
//
// The device: NGfx's own (the global at 0x010029a8, read at 0x00c36393), when
// it is the one whose frames we see shown, and when what it draws on now is
// its back buffer (not a picture the game draws in).
//
// Every link is checked: the code bytes once, the vftables at every frame.
// When one is not as expected, that frame is drawn at Present as before (the
// cursor under our panels), and the log says why, once per reason.
#include <d3d9.h>
#include <string.h>

#include "d3d9_hook.h"
#include "log.h"
#include "overlay_internal.h"
#include "patch.h"

namespace overlay {
namespace {

const DWORD kCursorDrawSlot = 0x00e4949c;  // NUI::CCursor vftable 0x00e49494, slot 2
const DWORD kCursorDraw = 0x00836440;
const DWORD kFlushBatch = 0x00a46bc0;
const DWORD kGameDevice = 0x010029a8;  // IDirect3DDevice9 *
const DWORD kVisitorVftable = 0x00e49980;   // NUI::CUIVisitor
const DWORD kViewVftable = 0x00e89b3c;      // NGScene::C2DGameView
const DWORD kViewPartVftable = 0x00e89b2c;  // its I2DGameView part, at +0xc
const DWORD kSceneVftable = 0x00e89b90;     // NGScene::C2DScene

// The code we rely on, as we know it.
struct Code {
	DWORD at;
	BYTE bytes[16];
	int size;
	const char *what;
};
const Code kCode[] = {
	// sub esp,50h; push esi; mov esi,ecx; cmp dword ptr [esi+48h],4 (the cursor's kind: 4 = none)
	{kCursorDraw, {0x83, 0xec, 0x50, 0x56, 0x8b, 0xf1, 0x83, 0x7e, 0x48, 0x04}, 10, "the cursor's draw"},
	// sub esp,30h; push ebp; push esi; mov esi,ecx; xor ebp,ebp; cmp [esi+0E8h],ebp (no batch: nothing to do)
	{kFlushBatch, {0x83, 0xec, 0x30, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xed, 0x39, 0xae, 0xe8, 0x00, 0x00, 0x00}, 15,
	 "the 2D batch's draw"},
	// the visitor's picture: mov ecx,[eax+4] (the view) ... add ecx,0Ch (its I2DGameView part)
	{0x008380e5, {0x8b, 0x48, 0x04, 0x83, 0xc0, 0x08}, 6, "the visitor's picture"},
	{0x008380fa, {0x83, 0xc1, 0x0c}, 3, "the visitor's picture"},
	// the view's picture: mov ecx,[ecx+4] (the scene)
	{0x00c1b3ef, {0x8b, 0x49, 0x04}, 3, "the view's picture"},
	// the scene's quad: mov ecx,[esi+204h] (the current renderer); lea edi,[esi+0Ch]; cmp ecx,edi
	{0x00c1c203, {0x8b, 0x8e, 0x04, 0x02, 0x00, 0x00, 0x57, 0x8d, 0x7e, 0x0c, 0x3b, 0xcf}, 12, "the scene's quad"},
	// NGfx's draw: mov ecx,[010029a8h] (the device)
	{0x00c36393, {0x8b, 0x0d, 0xa8, 0x29, 0x00, 0x01}, 6, "NGfx's device"},
};

typedef void(__fastcall *CursorDrawFn)(void *cursor, void *edx, void *visitor);
typedef void(__fastcall *FlushFn)(void *renderer, void *edx);
CursorDrawFn g_draw_next = nullptr;
bool g_off = false;     // turned off after a crash (caught): every frame at Present
bool g_worked = false;  // logged once

DWORD Read(const void *base, DWORD offset)
{
	return *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(base) + offset);
}

// "Not under the cursor this frame, because...": in the log once per reason.
void NotUnder(const char *why)
{
	static const char *told[8];
	static int count = 0;
	for (int i = 0; i < count; ++i)
		if (told[i] == why)
			return;
	if (count < 8)
		told[count++] = why;
	hooklog::Write("overlay: a frame drawn at Present, over the game's cursor: %s (said once)", why);
}

// The 2D scene the cursor will be drawn in, or nullptr (*why says what was not as expected).
void *SceneOf(void *visitor, const char **why)
{
	__try {
		if (!visitor || Read(visitor, 0) != kVisitorVftable) {
			*why = "the cursor's visitor is not the one we know";
			return nullptr;
		}
		const void *view = reinterpret_cast<const void *>(Read(visitor, 4));
		if (!view || Read(view, 0) != kViewVftable || Read(view, 0xc) != kViewPartVftable) {
			*why = "the cursor's 2D view is not the one we know";
			return nullptr;
		}
		void *scene = reinterpret_cast<void *>(Read(view, 0x10));
		if (!scene || Read(scene, 0) != kSceneVftable) {
			*why = "the cursor's 2D scene is not the one we know";
			return nullptr;
		}
		return scene;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		*why = "the way to the cursor's 2D scene crashed (caught)";
		return nullptr;
	}
}

// The device the game draws with now, if its frames are the ones we see shown
// and it draws on its back buffer; else nullptr (and *why).
IDirect3DDevice9 *DrawingDevice(const char **why)
{
	IDirect3DDevice9 *device = *reinterpret_cast<IDirect3DDevice9 *const *>(kGameDevice);
	if (!device || device != d3dhook::PresentedDevice()) {
		*why = "the game's device is not the one whose frames we see";
		return nullptr;
	}
	IDirect3DSurface9 *target = nullptr, *back = nullptr;
	const bool screen = SUCCEEDED(device->GetRenderTarget(0, &target)) &&
	                    SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) && target && target == back;
	if (target)
		target->Release();
	if (back)
		back->Release();
	if (!screen) {
		*why = "the game draws its cursor on another picture than the screen";
		return nullptr;
	}
	return device;
}

// The batch of pictures still waiting in the scene, drawn now. False: it crashed (caught).
bool DrawWaitingBatch(void *scene)
{
	__try {
		if (void *renderer = reinterpret_cast<void *>(Read(scene, 0x204)))
			reinterpret_cast<FlushFn>(kFlushBatch)(renderer, nullptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
	return true;
}

bool DrawOverlay(IDirect3DDevice9 *device)
{
	__try {
		internal::DrawUnderCursor(device);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
	return true;
}

void BeforeCursor(void *visitor)
{
	if (!d3dhook::PresentedDevice())
		return;  // no frame shown yet: this one is drawn at Present
	const char *why = nullptr;
	void *scene = SceneOf(visitor, &why);
	IDirect3DDevice9 *device = scene ? DrawingDevice(&why) : nullptr;
	if (!device) {
		NotUnder(why);
		return;
	}
	if (!DrawWaitingBatch(scene)) {
		g_off = true;
		hooklog::Write("overlay: drawing the game's waiting 2D pictures crashed (caught): from now on our panels are "
		               "drawn at Present, over the game's cursor");
		return;
	}
	if (!DrawOverlay(device)) {
		g_off = true;
		hooklog::Write("overlay: drawing our panels under the game's cursor crashed (caught): from now on they are "
		               "drawn at Present, over it");
		return;
	}
	if (!g_worked) {
		g_worked = true;
		hooklog::Write("overlay: our panels are drawn just under the game's cursor (first frame)");
	}
}

// The cursor's draw of a frame we draw under: the last one. The cursor is drawn
// once per screen on the game's stack of screens (the main menu, the lobby,
// the waiting room...): the top screen's draw (e.g. 0x00722310) first draws
// the screens under it, then its own windows and the cursor (0x0066cfe0), so
// only the last cursor shows, over every screen. Which call is the last one is
// known only at Present: we take the one that was last in the frame before,
// by its place among the frame's calls and its visitor (a local of the top
// screen's 0x0066cfe0, so at the same stack address while the screens stay
// the same). Just after a change of screens, a frame may have no such call:
// it is drawn at Present, over the cursor.
struct Call {
	int index;
	const void *visitor;
};
Call g_last_before = {-1, nullptr};  // the last cursor draw of the frame before
Call g_last_now = {-1, nullptr};     // the last one so far in this frame
int g_calls = 0;                     // cursor draws so far in this frame

void __fastcall CursorDrawHook(void *cursor, void *edx, void *visitor)
{
	const Call call = {g_calls++, visitor};
	g_last_now = call;
	if (!g_off && call.index == g_last_before.index && call.visitor == g_last_before.visitor && internal::FrameWaiting())
		BeforeCursor(visitor);
	g_draw_next(cursor, edx, visitor);
}

}  // namespace

namespace internal {

void CursorFrameShown()
{
	g_last_before = g_last_now;
	g_last_now.index = -1;
	g_last_now.visitor = nullptr;
	g_calls = 0;
}

bool InstallCursorHook()
{
	if (g_draw_next)
		return true;
	for (const Code &c : kCode) {
		if (memcmp(reinterpret_cast<const void *>(c.at), c.bytes, c.size) != 0) {
			hooklog::Write("overlay: %s (0x%08lx) is not as expected: our panels are drawn over the game's cursor",
			               c.what, c.at);
			return false;
		}
	}
	g_draw_next = reinterpret_cast<CursorDrawFn>(hookpatch::PatchPointer(kCursorDrawSlot, kCursorDraw, &CursorDrawHook));
	if (!g_draw_next) {
		hooklog::Write("overlay: the game's cursor (vftable slot 0x%08lx) is not as expected: our panels are drawn over it",
		               kCursorDrawSlot);
		return false;
	}
	hooklog::Write("overlay: the game's cursor draw goes through us: our panels go just under the cursor");
	return true;
}

}  // namespace internal
}  // namespace overlay
