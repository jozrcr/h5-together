// The update check's pure work (update_check.cpp; tested by src/hook/test/update_check_test.cpp):
// version numbers, GitHub's answer, and "is a check due".
#pragma once

#include <string>

namespace mods {
namespace update {

// GitHub's public "latest release" address for our repo (the only request ever made).
const wchar_t kHost[] = L"api.github.com";
const wchar_t kPath[] = L"/repos/jozrcr/h5-together/releases/latest";
// Where a player gets it (shown in the notice; never opened or downloaded by the DLL).
const char kReleasesPage[] = "github.com/jozrcr/h5-together/releases";
// At most one request per this many seconds.
const long long kPeriod = 24 * 60 * 60;

struct Version {
	int part[3] = {0, 0, 0};  // major, minor, patch
	std::string suffix;       // "-beta" in "0.3.1-beta" ("" for a release)
};

// "0.3.1", "v0.3.1", "V0.3", "0.3.1-beta2" (2 or 3 numbers; a "v" before them; a suffix
// after a '-' or '+'). False for anything else ("", "latest", "0.x", "1.2.3.4").
bool ParseVersion(const std::string &text, Version *out);
// <0, 0, >0. A version with a suffix (a test build, "0.3.1-beta") comes before the same
// numbers without one; two suffixes compare as text.
int Compare(const Version &a, const Version &b);
// Is `latest` (a tag, "v0.3.1") newer than `ours` ("0.3.0")? False when either cannot be read.
bool IsNewer(const std::string &latest, const std::string &ours);
// The "tag_name" of GitHub's answer (JSON), or "" when there is none.
std::string TagName(const std::string &json);
// Is a check due at `now` (seconds since 1970), after one at `last` (0 = never)? Also
// when the clock went back (last is in the future).
bool Due(long long now, long long last, long long period = kPeriod);

}  // namespace update
}  // namespace mods
