// Mod: the opt-in "new version available" check (see update_check.h).
//
// Settings, h5-together.ini [update]:
//   check=0        1 = on (off by default: the add-on contacts nothing unless the player says so)
//   last_check=    when the last check was made (seconds since 1970; written by the add-on)
//   latest=        the newest version GitHub named then (written by the add-on): the notice
//                  shows from it until a newer check says otherwise, also on the days
//                  without a check
// The request: GET https://api.github.com/repos/jozrcr/h5-together/releases/latest with a
// User-Agent naming the add-on and its version (GitHub wants one), through WinHTTP with
// Windows' proxy settings; 5 s to connect, 10 s to answer; at most 512 KB read. Only the
// answer's "tag_name" is used. A failed check counts too (no retry before the next day).
#include "update_check.h"

#include <windows.h>
#include <winhttp.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>

#include "rmg_builder_widgets.h"
#include "update_check_logic.h"
#include "../hook/log.h"
#include "../hook/overlay.h"
#include "../hook/texts.h"
#include "../hook/version.h"
#include "../hook/third_party/imgui/imgui.h"

namespace mods {
namespace {

const char kName[] = "update notice";
const char kMainMenu[] = ".?AVCMainMenuScreen@NUI@@";
// The notice: two short lines at the bottom left of the main menu (its 1024x768 layout,
// centred and scaled with the height, as the game draws it).
const float kRefW = 1024.0f, kRefH = 768.0f;
const float kNoticeX = 14.0f, kNoticeY = 716.0f, kLineFont = 14.0f, kLineGap = 18.0f;

CRITICAL_SECTION g_lock;
bool g_on = false;
std::string g_latest;       // the newest version known (from GitHub, now or at the last check)
bool g_started = false;     // this run's check was started (main thread only)
long long g_last_check = 0;
bool g_told = false;        // the notice's first frame was logged

std::wstring IniPath()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"h5-together", L".ini", path, MAX_PATH);
	return path;
}

std::string Narrow(const wchar_t *w)
{
	std::string s;
	for (; *w; ++w)
		s += *w < 128 ? static_cast<char>(*w) : '?';
	return s;
}

// The request (on its own thread). Returns the tag, or "" and what went wrong.
std::string AskGitHub(std::string *problem)
{
	const std::wstring agent = L"H5-Together/" + texts::Wide(OPEN_HOMM5_VERSION) + L" (update check; https://github.com/jozrcr/h5-together)";
	HINTERNET session = WinHttpOpen(agent.c_str(), 4 /* WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY (Windows 8.1+) */,
	                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
		session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		*problem = "WinHTTP could not start (error " + std::to_string(GetLastError()) + ")";
		return std::string();
	}
	WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000);
	DWORD protocols = 0x00000800 /* TLS 1.2 */ | 0x00002000 /* TLS 1.3 */;
	if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
		protocols = 0x00000800;  // (Windows 7, 8: no TLS 1.3)
		WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
	}
	std::string tag;
	HINTERNET connect = WinHttpConnect(session, update::kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
	HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", update::kPath, nullptr, WINHTTP_NO_REFERER,
	                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
	                            : nullptr;
	const wchar_t headers[] = L"Accept: application/vnd.github+json\r\n";
	DWORD status = 0, size = sizeof(status);
	if (!request || !WinHttpSendRequest(request, headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
	    !WinHttpReceiveResponse(request, nullptr) ||
	    !WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
	                         &status, &size, WINHTTP_NO_HEADER_INDEX)) {
		const DWORD error = GetLastError();
		*problem = "no answer from GitHub (WinHTTP error " + std::to_string(error) +
		           (error == 12007 || error == 12029 ? ": no internet, or GitHub cannot be reached" : error == 12002 ? ": it took too long" : "") + ")";
	}
	else if (status != 200) {
		*problem = "GitHub answered " + std::to_string(status) +
		           (status == 404 ? " (no release published yet)" : status == 403 || status == 429 ? " (too many requests from this address)" : "");
	}
	else {
		std::string body;
		for (;;) {
			char buffer[8192];
			DWORD got = 0;
			if (!WinHttpReadData(request, buffer, sizeof(buffer), &got) || got == 0)
				break;
			body.append(buffer, got);
			if (body.size() > 512 * 1024)
				break;
		}
		tag = update::TagName(body);
		if (tag.empty())
			*problem = "GitHub's answer has no release tag (" + std::to_string(body.size()) + " bytes)";
	}
	if (request)
		WinHttpCloseHandle(request);
	if (connect)
		WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);
	return tag;
}

DWORD WINAPI CheckThread(void *)
{
	const DWORD start = GetTickCount();
	std::string problem;
	const std::string tag = AskGitHub(&problem);
	const long long now = static_cast<long long>(time(nullptr));
	const std::wstring ini = IniPath();
	WritePrivateProfileStringW(L"update", L"last_check", std::to_wstring(now).c_str(), ini.c_str());
	if (tag.empty()) {
		hooklog::Write("update: %s (after %lu ms); the next check is in a day", problem.c_str(), GetTickCount() - start);
		return 0;
	}
	update::Version v;
	if (!update::ParseVersion(tag, &v)) {
		hooklog::Write("update: GitHub's latest release is \"%s\", not a version number: nothing to say", tag.c_str());
		return 0;
	}
	std::string version = std::to_string(v.part[0]) + "." + std::to_string(v.part[1]) + "." + std::to_string(v.part[2]) + v.suffix;
	WritePrivateProfileStringW(L"update", L"latest", texts::Wide(version).c_str(), ini.c_str());
	const bool newer = update::IsNewer(version, OPEN_HOMM5_VERSION);
	{
		EnterCriticalSection(&g_lock);
		g_latest = version;
		LeaveCriticalSection(&g_lock);
	}
	if (newer)
		hooklog::Write("update: H5 Together %s is out (this is %s): https://%s (after %lu ms; nothing is downloaded)",
		               version.c_str(), OPEN_HOMM5_VERSION, update::kReleasesPage, GetTickCount() - start);
	else
		hooklog::Write("update: this is the newest version (%s; GitHub's latest release: %s; after %lu ms)", OPEN_HOMM5_VERSION,
		               tag.c_str(), GetTickCount() - start);
	return 0;
}

std::string Notice()
{
	EnterCriticalSection(&g_lock);
	const std::string latest = g_latest;
	LeaveCriticalSection(&g_lock);
	return update::IsNewer(latest, OPEN_HOMM5_VERSION) ? latest : std::string();
}

// Once per run: the check, on its own thread, if the last one was a day ago or more.
void StartCheck()
{
	if (g_started)
		return;
	g_started = true;
	const long long now = static_cast<long long>(time(nullptr));
	if (!update::Due(now, g_last_check)) {
		hooklog::Write("update: checked less than a day ago: no request this time");
		return;
	}
	HANDLE t = CreateThread(nullptr, 0, &CheckThread, nullptr, 0, nullptr);
	if (t) {
		CloseHandle(t);
		hooklog::Write("update: asking GitHub for the latest release (https://%ls%ls)", update::kHost, update::kPath);
	}
	else
		hooklog::Write("update: cannot start the check's thread (error %lu)", GetLastError());
}

bool Wanted()
{
	char top[96] = "";
	if (!overlay::TopScreen(top, sizeof(top)) || strcmp(top, kMainMenu) != 0)
		return false;
	StartCheck();  // (at the main menu: the game has started, and the player sees the answer there)
	return !Notice().empty();
}

void Prepare(const overlay::Frame &frame)
{
	overlay::WantFont(floorf(kLineFont * frame.height / kRefH + 0.5f));
}

void Draw(const overlay::Frame &frame)
{
	const std::string latest = Notice();
	if (latest.empty())
		return;
	const float k = frame.height / kRefH;
	const float px = floorf(kLineFont * k + 0.5f);
	const float x0 = floorf((frame.width - kRefW * k) / 2 + 0.5f);
	ImFont *font = overlay::Font(px);
	ImDrawList *draw = ImGui::GetForegroundDrawList();
	const std::string line1 = texts::Format("update.available", {{"new", latest}, {"ours", OPEN_HOMM5_VERSION}});
	const std::string line2 = texts::Format("update.where", {{"page", update::kReleasesPage}});
	builder_ui::ShadowText(draw, font, px, ImVec2(x0 + kNoticeX * k, kNoticeY * k), builder_ui::kGold, line1.c_str());
	builder_ui::ShadowText(draw, font, px, ImVec2(x0 + kNoticeX * k, (kNoticeY + kLineGap) * k), builder_ui::kParchment, line2.c_str());
	if (!g_told) {
		g_told = true;
		hooklog::Write("update: the main menu shows \"%s\" / \"%s\"", line1.c_str(), line2.c_str());
	}
}

bool CheckOn()
{
	return GetPrivateProfileIntW(L"update", L"check", 0, IniPath().c_str()) != 0;
}

}  // namespace

void InstallUpdateCheckEarly()
{
	if (CheckOn())
		overlay::InstallEarly();
}

void ApplyUpdateCheckFromSettings()
{
	InitializeCriticalSection(&g_lock);
	g_on = CheckOn();
	if (!g_on) {
		hooklog::Write("update: the check for new versions is off ([update] check=0, the default): nothing is asked");
		return;
	}
	const std::wstring ini = IniPath();
	wchar_t text[64] = L"";
	GetPrivateProfileStringW(L"update", L"last_check", L"", text, 64, ini.c_str());
	g_last_check = _wtoi64(text);
	GetPrivateProfileStringW(L"update", L"latest", L"", text, 64, ini.c_str());
	g_latest = Narrow(text);
	const overlay::Client client = {kName, &Wanted, &Prepare, &Draw};
	const bool shown = overlay::AddClient(client);
	if (!shown)
		StartCheck();  // no overlay: no main menu notice, the log only
	hooklog::Write("update: on ([update] check=1): at most once a day, at the main menu, GitHub is asked for the latest "
	               "release; %s%s",
	               update::IsNewer(g_latest, OPEN_HOMM5_VERSION) ? "the last check found " : "",
	               update::IsNewer(g_latest, OPEN_HOMM5_VERSION) ? (g_latest + " (newer than this one)").c_str()
	               : shown                                        ? "nothing newer known yet"
	                                                              : "no overlay: the log only");
}

}  // namespace mods
