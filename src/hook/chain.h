// Chain-loading another dinput8.dll mod (docs/install.md, "Another dinput8.dll mod").
//
// Other mods also come as a dinput8.dll next to the game's exe (ASI loaders, which load
// widescreen fixes and the like). Only one file can have that name. A player renames the
// other mod's file to dinput8_next.dll (or names it in h5-together.ini: [chain] dll=), and
// our DLL loads it when the game loads ours, and passes DirectInput on to it instead of to
// Windows' own dinput8.dll; that mod then passes it on to Windows' (dllmain.cpp's Real()).
// If it cannot be loaded, is not a dinput8.dll, or is a copy of H5 Together itself, the
// log says why and Windows' own dinput8.dll is used, as without it.
//
// When that mod asks for "dinput8.dll" by name it gets ours (a DLL of that name is already
// loaded): dllmain.cpp then passes its calls to Windows' DLL, never back to it.
#pragma once
#include <windows.h>

#include <string>

namespace chain {

// From DllMain, once the log is open: loads the other mod's DLL, if there is one.
void Load(HMODULE self);
// That DLL, or nullptr (none, or it could not be used).
HMODULE Module();

// --- the pure parts (tested by test/proxy_test.cpp) ----------------------------------------
// The file to load for the [chain] dll= value ("" = the default, dinput8_next.dll), next to
// the exe unless the value is a full path; "" when chaining is off ("none" or "0").
std::wstring PathFor(const std::wstring &value, const std::wstring &exe_folder);
// Does this file's text say it is H5 Together (or open-homm5, its name until 0.2.7)?
bool IsOurs(const std::string &bytes);

}  // namespace chain
