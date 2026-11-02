#include "steam_runtime.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "steam_spike_logic.h"
#include "../hook/log.h"
#include "../hook/old_names.h"

namespace steamrt {
namespace {

using namespace steamflat;

bool g_tried = false;
bool g_ok = false;
Steam g_steam = {};

template <typename T>
void Load(HMODULE dll, const char *name, T *out, char *missing, size_t size)
{
	*out = reinterpret_cast<T>(GetProcAddress(dll, name));
	if (!*out) {
		strncat_s(missing, size, " ", _TRUNCATE);
		strncat_s(missing, size, name, _TRUNCATE);
	}
}

bool LoadApi(HMODULE dll, Api *api, const char *who)
{
	char missing[1024] = "";
	const size_t n = sizeof(missing);
	Load(dll, "SteamAPI_InitFlat", &api->init, missing, n);
	Load(dll, "SteamAPI_GetHSteamPipe", &api->get_pipe, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_Init", &api->dispatch_init, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_RunFrame", &api->run_frame, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_GetNextCallback", &api->next_callback, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_FreeLastCallback", &api->free_last_callback, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_GetAPICallResult", &api->call_result, missing, n);
	Load(dll, "SteamAPI_SteamUser_v023", &api->user, missing, n);
	Load(dll, "SteamAPI_SteamFriends_v018", &api->friends, missing, n);
	Load(dll, "SteamAPI_SteamUtils_v011", &api->utils, missing, n);
	Load(dll, "SteamAPI_SteamMatchmaking_v009", &api->matchmaking, missing, n);
	Load(dll, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002", &api->messages, missing, n);
	Load(dll, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004", &api->net_utils, missing, n);
	Load(dll, "SteamAPI_ISteamUser_GetSteamID", &api->get_steam_id, missing, n);
	Load(dll, "SteamAPI_ISteamUtils_GetAppID", &api->get_app_id, missing, n);
	Load(dll, "SteamAPI_ISteamUtils_IsOverlayEnabled", &api->overlay_enabled, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetPersonaName", &api->persona_name, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendCount", &api->friend_count, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendByIndex", &api->friend_by_index, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendRelationship", &api->friend_relationship, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendPersonaName", &api->friend_name, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendGamePlayed", &api->friend_game, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_CreateLobby", &api->create_lobby, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_JoinLobby", &api->join_lobby, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_LeaveLobby", &api->leave_lobby, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_SetLobbyData", &api->set_lobby_data, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetLobbyData", &api->get_lobby_data, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers", &api->lobby_members, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetLobbyOwner", &api->lobby_owner, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex", &api->lobby_member, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetLobbyMemberData", &api->get_member_data, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_SetLobbyMemberData", &api->set_member_data, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_RequestLobbyData", &api->request_lobby_data, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_RequestLobbyList", &api->request_lobby_list, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter", &api->add_string_filter, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter", &api->add_distance_filter, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetLobbyByIndex", &api->lobby_by_index, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingMessages_SendMessageToUser", &api->send, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel", &api->receive, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser", &api->accept_session, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo", &api->session_info, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess", &api->init_relay, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus", &api->relay_status, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp", &api->local_timestamp, missing, n);
	Load(dll, "SteamAPI_SteamNetworkingMessage_t_Release", &api->release, missing, n);
	if (missing[0])
		hooklog::Write("%s: steam_api.dll lacks:%s (an older SDK? 1.65 has them all)", who, missing);
	return missing[0] == 0;
}

void SetAppIdVariable(const wchar_t *name, uint32_t app_id, const char *who)
{
	wchar_t before[32] = L"";
	const DWORD n = GetEnvironmentVariableW(name, before, 32);
	wchar_t value[16];
	_snwprintf_s(value, 16, _TRUNCATE, L"%u", app_id);
	SetEnvironmentVariableW(name, value);
	hooklog::Write("%s: %ls was %ls, now %ls", who, name, n && n < 32 ? before : L"(not set)", value);
}

bool DoStart(uint32_t app_id, const char *who)
{
	// bin\h5-together\steam_api.dll (one file for every robot instance, so not PathNextToExe;
	// bin\open-homm5\steam_api.dll, the folder's name until 0.2.7, while only that one is there)
	wchar_t dll_path[MAX_PATH];
	oldnames::PathInOurFolder(L"steam_api.dll", dll_path, MAX_PATH);
	HMODULE dll = LoadLibraryExW(dll_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (!dll) {
		hooklog::Write("%s: cannot load %ls (error %lu): is Valve's steam_api.dll there?", who, dll_path, GetLastError());
		return false;
	}
	Api &api = g_steam.api;
	if (!LoadApi(dll, &api, who))
		return false;
	wchar_t cwd[MAX_PATH] = L"";
	GetCurrentDirectoryW(MAX_PATH, cwd);
	wchar_t appid_txt[MAX_PATH];
	_snwprintf_s(appid_txt, MAX_PATH, _TRUNCATE, L"%s\\steam_appid.txt", cwd);
	if (GetFileAttributesW(appid_txt) != INVALID_FILE_ATTRIBUTES)
		hooklog::Write("%s: note: a steam_appid.txt in the game's folder overrides our app id", who);
	SetAppIdVariable(L"SteamAppId", app_id, who);
	SetAppIdVariable(L"SteamGameId", app_id, who);
	char err[kErrMsgSize] = "";
	const DWORD t0 = GetTickCount();
	const int r = api.init(err);
	g_steam.init_ms = GetTickCount() - t0;
	hooklog::Write("%s: SteamAPI_InitFlat -> %d (%s) in %lu ms%s%s", who, r, steamspike::InitResultName(r),
	               static_cast<unsigned long>(g_steam.init_ms), err[0] ? ": " : "", err);
	if (r != kInitOk)
		return false;
	api.dispatch_init();
	g_steam.pipe = api.get_pipe();
	g_steam.user = api.user();
	g_steam.friends = api.friends();
	g_steam.utils = api.utils();
	g_steam.mm = api.matchmaking();
	g_steam.msgs = api.messages();
	g_steam.netu = api.net_utils();
	if (!g_steam.user || !g_steam.friends || !g_steam.utils || !g_steam.mm || !g_steam.msgs || !g_steam.netu) {
		hooklog::Write("%s: a Steam interface is missing (user %p, friends %p, utils %p, matchmaking %p, messages %p, "
		               "networking utils %p)",
		               who, g_steam.user, g_steam.friends, g_steam.utils, g_steam.mm, g_steam.msgs, g_steam.netu);
		return false;
	}
	g_steam.me = api.get_steam_id(g_steam.user);
	g_steam.app_id = app_id;
	g_steam.steam_app_id = api.get_app_id(g_steam.utils);
	return g_steam.me != 0;
}

}  // namespace

const Steam *Start(uint32_t app_id, const char *who)
{
	if (!g_tried) {
		g_tried = true;
		g_ok = DoStart(app_id, who);
	}
	return g_ok ? &g_steam : nullptr;
}

}  // namespace steamrt
