// The add-on's old name. Until 0.2.7 it was called open-homm5, and so were its files
// next to the game exe: open-homm5.ini, open-homm5-skins.ini, open-homm5.log and the
// folder open-homm5\ (templates, the French packs in lang\fr). Now they are called
// h5-together. The installer moves them; this is for a player who only copied the new
// dinput8.dll over an old install:
//   - at start-up the settings and the hero looks' file move to the new names (when
//     the new one is not there yet), and the old log becomes the older log of the new
//     name (h5-together.old.log);
//   - a file or folder of ours (templates, lang\fr...) is looked for in h5-together\,
//     else in open-homm5\.
// Each copy the robot runs (OPEN_HOMM5_INSTANCE, see log.h) has its own file names.
#pragma once
#include <windows.h>

#include <string>
#include <vector>

namespace oldnames {

extern const wchar_t kFolder[];     // L"h5-together": our folder next to the exe
extern const wchar_t kOldFolder[];  // L"open-homm5": its name until 0.2.7

// What the start-up does with one file of the old name.
enum class Step {
	kNone,   // no old file: nothing to do
	kMove,   // move it to the new name
	kLeave,  // leave it where it is (the new name is there already, and is the one used)
};

// A file of the old name (settings, hero looks): moved when only the old one is there.
Step PlanMove(bool old_exists, bool new_exists);
// The old log: it becomes the older log of the new name, but only while the new name
// has no log at all yet (else it is older than those, and stays for the installer).
Step PlanLog(bool old_log, bool new_log, bool new_old_log);
// A file or folder of ours: the new path if it is there, else the old one if it is
// there, else the new one (where a new file goes). True: the old one.
bool UseOldFolder(bool new_exists, bool old_exists);

// Start-up, in DllMain before the log is opened (it may move the old log): moves this
// copy's files from the old names. Returns the lines to write to the log once it is open.
std::vector<std::string> MoveOldFiles();

// Path of <our folder>\<rel> next to the exe (rel: L"templates", L"lang\\fr",
// L"steam_api.dll"...): h5-together\<rel> if that is there, else open-homm5\<rel> if
// that is there, else h5-together\<rel> (where a new file goes). Returns where
// "<our folder>\<rel>" starts in out (after the exe's folder), for messages.
DWORD PathInOurFolder(const wchar_t *rel, wchar_t *out, DWORD size);

}  // namespace oldnames
