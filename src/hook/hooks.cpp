#include "hooks.h"

#include <windows.h>

#include "crash.h"
#include "game_build.h"
#include "log.h"
#include "overlay.h"
#include "watchdog.h"
#include "../mods/ai_pacing.h"
#include "../mods/audio_volume.h"
#include "../mods/autosave.h"
#include "../mods/camera.h"
#include "../mods/coop_guard.h"
#include "../mods/edge_sky.h"
#include "../mods/fast_paths.h"
#include "../mods/hero_skins.h"
#include "../mods/hero_skins_internal.h"
#include "../mods/host_rules.h"
#include "../mods/lan_discovery.h"
#include "../mods/languages.h"
#include "../mods/lockstep.h"
#include "../mods/net_tuning.h"
#include "../mods/notify.h"
#include "../mods/original_music.h"
#include "../mods/player_list.h"
#include "../mods/rmg_builder.h"
#include "../mods/rmg_factions.h"
#include "../mods/rmg_teams.h"
#include "../mods/rmg_templates.h"
#include "../mods/settings_menu.h"
#include "../mods/steam_spike.h"
#include "../mods/update_check.h"
#include "../mods/wheel.h"

namespace hooks {

void InstallAiTrace();

namespace {

// Escape hatch for testing: an empty file "h5-together.nohooks" next to the
// exe keeps the DLL in forwarding-only mode ("open-homm5.nohooks" too: the
// add-on's old name, until 0.2.7).
bool NoHooks()
{
	wchar_t path[MAX_PATH];
	DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
	while (n > 0 && path[n - 1] != L'\\')
		--n;
	const wchar_t *const names[] = {L"h5-together.nohooks", L"open-homm5.nohooks"};
	for (const wchar_t *name : names) {
		lstrcpynW(path + n, name, MAX_PATH - n);
		if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
			return true;
	}
	return false;
}

}  // namespace

void InstallEarly()
{
	// Only data-table hooks that must be in place before the game reads its
	// data, and only in a build we know (an unwrapped exe: the Steam-wrapped
	// one is still encrypted now, and gets them with InstallAll()).
	if (NoHooks() || !IsHookable(DetectGameBuild()))
		return;
	crash::Install("from the DLL's load, before the game's own crash filter");  // first: before the game's code runs
	mods::InstallLanguagesEarly();  // [lang] texts=fr, before the game reads its first text
	mods::InstallOriginalMusicEarly();
	mods::InstallPlayerListEarly();  // the Direct3D import, before the game makes its device
	mods::InstallRmgBuilderEarly();
	mods::InstallHeroSkinsPanelEarly();  // the overlay's Direct3D import, for the looks' panel
	mods::InstallHostRulesEarly();  // its panel: the Direct3D import
	mods::InstallUpdateCheckEarly();  // [update] check=1: its notice in the main menu
	mods::InstallSettingsMenuEarly();  // our page in the Options screen: before the game reads its list
	overlay::InstallTestPanelEarly();
}

void InstallAll()
{
	if (NoHooks()) {
		hooklog::Write("h5-together.nohooks (or open-homm5.nohooks) found: hooks disabled");
		return;
	}
	crash::Install("at the game's first DirectInput call");  // (done at the DLL's load in an unwrapped exe)
	watchdog::Install();  // first: the others register their state for its reports
	mods::ApplySteamSpikeFromSettings();  // [online] spike (test only, off by default): early, before the game's Direct3D device
	InstallAiTrace();
	mods::ApplyNetTuningFromSettings();
	mods::ApplyFastPathsFromSettings();
	mods::ApplyLanDiscoveryFromSettings();
	mods::ApplyCoopGuardFromSettings();
	mods::ApplyLockstepFromSettings();
	mods::ApplyAiPacingFromSettings();
	mods::ApplyAutosaveFromSettings();
	mods::ApplyNotifyFromSettings();
	mods::ApplyRmgTeamsFromSettings();
	mods::ApplyRmgTemplatesFromSettings();
	mods::ApplyLanguagesFromSettings();
	mods::ApplyRmgFactionsFromSettings();
	mods::ApplyOriginalMusicFromSettings();
	mods::ApplyPlayerListFromSettings();  // overlay clients: drawn in this order
	mods::ApplyRmgBuilderFromSettings();
	overlay::ApplyTestPanelFromSettings();
	mods::ApplyCameraFromSettings();
	mods::ApplyWheelFromSettings();  // its DirectInput filter goes first (order -10)
	mods::ApplyEdgeSkyFromSettings();  // [camera] edge_sky: before the first map reads its light
	mods::ApplyHostRulesFromSettings();  // [rules]: the host's rules, served as H55-Settings.lua
	mods::ApplyAudioVolumeFromSettings();
	mods::ApplyHeroSkinsFromSettings();  // after notify and the co-op guard: it stacks on their hooks
	mods::ApplyUpdateCheckFromSettings();  // [update] check (off by default)
	mods::ApplySettingsMenuFromSettings();  // Options, H5 Together: Accept writes h5-together.ini; F3; the first-start question
}

}  // namespace hooks
