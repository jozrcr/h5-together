// Changing h5-together.ini in place, the installer's way (install.ps1's
// Merge-Settings): a setting's value goes into its own line of its section,
// so the file stays tidy and the next install keeps the value.
//
//   - A changed setting fills its commented line ";key=..." in its section
//     ("key=value", the ";" gone), or its "key=..." line if it already has one.
//     If the section has neither, "key=value" goes after the section's last
//     line; a section the file lacks goes at the end.
//   - Back to the default (the menu's "Default", or the default value chosen):
//     the ";" goes back (";key=<default>"). If the section already has a
//     ";key=" line, the "key=..." line goes instead, so there is one line per
//     setting, as in the shipped file.
//   - Every other line, comment and blank line stays byte for byte, with its
//     own line ending; new lines take the file's (CRLF unless the file has only LF).
//   - Like Windows' GetPrivateProfileString, only the first section of a name
//     counts, and in it the first "key=" line; names ignore upper/lower case.
//     Later "key=" lines of the same key in that section would take over once the
//     first is commented out, so going back to the default removes them too.
//
// No Windows calls in Edit: test/settings_test.cpp checks it with the same
// cases as tools/package/test_scripts.py.
#pragma once

#include <string>
#include <vector>

namespace mods {
namespace settings {

struct Change {
	std::string section, key;
	std::string value;       // the new value (ignored when to_default)
	std::string def;         // the default, for the ";key=<default>" line
	bool to_default = false;
};

// The file's bytes with the changes made. False (and *out untouched) when the
// file is not plain text: it holds a zero byte (saved as "Unicode" by Notepad).
bool Edit(const std::string &file, const std::vector<Change> &changes, std::string *out);

// The value the game would read (the first section of that name, its first
// "key=" line), and whether there is one; *value untouched when not.
bool Read(const std::string &file, const char *section, const char *key, std::string *value);

// The file next to the game's exe (hooklog::PathNextToExe: h5-together.ini, or
// h5-together-<instance>.ini in robot runs) with the changes made, written to a
// new file first and then put in place. False and *error ("the file is
// read-only", Windows' error number) when it cannot be written: the file is then
// as it was. (Windows only; not in the test exe.)
bool EditFile(const wchar_t *path, const std::vector<Change> &changes, std::string *error);

}  // namespace settings
}  // namespace mods
