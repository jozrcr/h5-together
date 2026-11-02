// Test of the update check's pure work (src/mods/update_check_logic.cpp): version numbers
// and their order, the tag in GitHub's answer, and when a check is due. No network.
// (Not called update_check_test: Windows takes an exe with "update" in its name for an
// installer and asks for administrator rights before it runs.)
#include <stdio.h>

#include <string>

#include "../../mods/update_check_logic.h"
#include "../version.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

using mods::update::Version;

bool Parses(const char *text, int a, int b, int c, const char *suffix = "")
{
	Version v;
	return mods::update::ParseVersion(text, &v) && v.part[0] == a && v.part[1] == b && v.part[2] == c && v.suffix == suffix;
}

bool Refused(const char *text)
{
	Version v;
	return !mods::update::ParseVersion(text, &v);
}

}  // namespace

int main()
{
	using mods::update::IsNewer;
	Check(Parses("0.3.0", 0, 3, 0) && Parses("v0.3.1", 0, 3, 1) && Parses("V1.2", 1, 2, 0) && Parses("v10.20.30", 10, 20, 30),
	      "versions: 2 or 3 numbers, with or without a v");
	Check(Parses("0.3.1-beta2", 0, 3, 1, "-beta2") && Parses("v0.4.0+robot", 0, 4, 0, "+robot"), "versions: a suffix after - or +");
	Check(Refused("") && Refused("latest") && Refused("v") && Refused("0") && Refused("0.x") && Refused("1.2.3.4") && Refused("0.3.0 ") &&
	          Refused("0..3") && Refused("0.3.") && Refused("0.3-") && Refused("-0.3") && Refused("999999.1"),
	      "versions: anything else is refused");
	Check(!Refused(OPEN_HOMM5_VERSION) && !IsNewer(OPEN_HOMM5_VERSION, OPEN_HOMM5_VERSION),
	      "versions: our own version (version.h) reads, and is not newer than itself");
	Check(IsNewer("v0.3.1", "0.3.0") && IsNewer("0.4.0", "0.3.9") && IsNewer("1.0.0", "0.99.99") && IsNewer("v0.3.10", "0.3.9"),
	      "newer: by the numbers (10 after 9, not as text)");
	Check(!IsNewer("v0.3.0", "0.3.0") && !IsNewer("0.2.7", "0.3.0") && !IsNewer("v0.3", "0.3.0"), "newer: the same or older is not");
	Check(IsNewer("0.3.1", "0.3.1-beta") && !IsNewer("0.3.1-beta", "0.3.1") && IsNewer("0.3.1-beta", "0.3.0") &&
	          IsNewer("0.3.1-rc2", "0.3.1-rc1"),
	      "newer: a test build (with a suffix) comes before its release, after the version before");
	Check(!IsNewer("latest", "0.3.0") && !IsNewer("v0.4.0", "") && !IsNewer("", "0.3.0"), "newer: no when either cannot be read");

	using mods::update::TagName;
	const std::string answer =
		"{\n  \"url\": \"https://api.github.com/repos/jozrcr/h5-together/releases/1\",\n  \"html_url\": \"https://github.com/x\",\n"
		"  \"id\": 1,\n  \"tag_name\": \"v0.3.1\",\n  \"name\": \"H5 Together 0.3.1\",\n  \"draft\": false\n}";
	Check(TagName(answer) == "v0.3.1", "GitHub's answer: its tag_name");
	Check(TagName("{\"tag_name\" :\t\"v0.2.7\"}") == "v0.2.7", "GitHub's answer: spaces around the colon");
	Check(TagName("{\"message\":\"Not Found\"}").empty() && TagName("{\"tag_name\": 5}").empty() && TagName("{\"tag_name\": \"v0.3").empty() &&
	          TagName("{\"tag_name\": \"v0\\\"3\"}").empty() && TagName("").empty(),
	      "GitHub's answer: no tag, a number, a cut answer, an escape: none");

	using mods::update::Due;
	const long long day = 24 * 60 * 60, now = 1793000000;
	Check(Due(now, 0) && Due(now, now - day) && Due(now, now - 3 * day), "due: never checked, or a day ago or more");
	Check(!Due(now, now - day + 1) && !Due(now, now) && !Due(now, now - 60), "due: not within a day (at most one request a day)");
	Check(Due(now, now + 3600), "due: when the clock went back (the last check is in the future)");

	printf(g_failures ? "%d FAILED\n" : "new release test: all passed\n", g_failures);
	return g_failures ? 1 : 0;
}
