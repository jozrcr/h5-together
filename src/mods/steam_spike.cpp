// Mod (test only): the Steam spike of docs/plans/online-steam.md ("The first spike").
//
// The question: inside the game's own process, under the game's own Steam app
// id (15370, a game that never used Steamworks), does Steam start, accept a
// lobby and carry relayed messages? Everything it learns goes to open-homm5.log
// (lines starting with "steam:").
//
//   [online]
//   spike=0            0 = off (the default); 1 = solo test on one PC;
//                      host / join = the two-PC test (one PC each)
//   app_id=15370       480 = Valve's test app "Spacewar", only to tell an
//                      app-id problem from a problem of ours
//   spike_seconds=     how long the test runs (solo 60, host 900, join 300)
//   spike_lobby=friends  the test lobby: friends | public | invisible
//   spike_pings=100    join: messages sent to the host and echoed back
//   spike_ping_bytes=300
//
// How:
// - steam_api.dll (32-bit, from Valve's Steamworks SDK) is loaded from
//   bin\open-homm5\ at run time. Without it the add-on runs as usual.
// - SteamAppId and SteamGameId are set in this process (nothing on disk) before
//   SteamAPI_InitFlat, so Steam knows the app even when Steam did not start the
//   game (GameRanger, a shortcut, the robot). SteamAPI_RestartAppIfNecessary is
//   never called: it would restart the game through Steam, which runs Steam's
//   own H5_Game.exe, not MMH5.5's.
// - Init runs on the game's thread at the first DirectInput8Create, early, so
//   that Steam's overlay can still hook the Direct3D device. Everything else
//   runs on a thread of our own, every 20 ms: Steam's callbacks (manual
//   dispatch, so they run on that thread only, never on the game's threads) and
//   the test's steps. Not in the per-frame hook: the game shows no frames while
//   a map loads, and a session request left waiting that long stalls a join.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "steam_spike.h"

#include "steam_api_flat.h"
#include "steam_spike_logic.h"
#include "../hook/log.h"
#include "../hook/version.h"

namespace mods {
namespace {

using namespace steamflat;
using steamspike::Mode;

const int kChannel = 7;  // the test messages' channel
const int kTickMs = 20;

struct Api {
	InitFlatFn init;
	GetPipeFn get_pipe;
	VoidFn dispatch_init;
	RunFrameFn run_frame;
	GetNextCallbackFn next_callback;
	RunFrameFn free_last_callback;
	GetApiCallResultFn call_result;
	AccessorFn user, friends, utils, matchmaking, messages, net_utils;
	GetSteamIdFn get_steam_id;
	GetAppIdFn get_app_id;
	IfaceBoolFn overlay_enabled;
	GetPersonaNameFn persona_name;
	GetFriendCountFn friend_count;
	GetFriendByIndexFn friend_by_index;
	GetFriendRelationshipFn friend_relationship;
	GetFriendPersonaNameFn friend_name;
	GetFriendGamePlayedFn friend_game;
	CreateLobbyFn create_lobby;
	JoinLobbyFn join_lobby;
	LeaveLobbyFn leave_lobby;
	SetLobbyDataFn set_lobby_data;
	GetLobbyDataFn get_lobby_data;
	GetNumLobbyMembersFn lobby_members;
	GetLobbyOwnerFn lobby_owner;
	RequestLobbyListFn request_lobby_list;
	AddStringFilterFn add_string_filter;
	AddDistanceFilterFn add_distance_filter;
	GetLobbyByIndexFn lobby_by_index;
	SendMessageToUserFn send;
	ReceiveMessagesFn receive;
	SessionFn accept_session;
	GetSessionInfoFn session_info;
	InitRelayFn init_relay;
	GetRelayStatusFn relay_status;
	ReleaseMessageFn release;
};

struct Settings {
	Mode mode;
	uint32_t app_id;
	int seconds;
	int lobby_type;
	const char *lobby_type_name;
	int pings;
	int ping_bytes;
};

Api g_api = {};
Settings g_set = {};
Pipe g_pipe = 0;
Iface g_user = nullptr, g_friends = nullptr, g_utils = nullptr, g_mm = nullptr, g_msgs = nullptr, g_netu = nullptr;
SteamId g_me = 0;
DWORD g_start = 0;

// the test's state (only our thread touches it)
ApiCall g_create_call = 0, g_list_call = 0, g_join_call = 0;
SteamId g_lobby = 0;
int g_lobbies_listed = -1;
bool g_lobby_in_list = false;
RelayNetworkStatus g_relay = {};
bool g_relay_logged = false;
DWORD g_relay_ok_ms = 0;
DWORD g_next_relay_check = 0;
int g_self_sent = 0, g_self_send_failed = 0, g_self_received = 0;
int g_unknown_callbacks = 0;
DWORD g_next_session_log = 20000;
bool g_overlay_logged_late = false;
SteamId g_target = 0;  // join: the host
int g_pings_sent = 0, g_ping_send_failed = 0;
DWORD g_next_ping = 0, g_pings_done_at = 0, g_next_search = 0;
steamspike::RttStats g_rtt;
bool g_done = false;
const int kMaxPeers = 8;
struct Peer {
	SteamId id;
	int pings;
};
Peer g_peers[kMaxPeers] = {};
DWORD g_next_peer_log = 30000;

DWORD Now()
{
	return GetTickCount() - g_start;
}

template <typename T>
void Load(HMODULE dll, const char *name, T *out, char *missing, size_t size)
{
	*out = reinterpret_cast<T>(GetProcAddress(dll, name));
	if (!*out) {
		strncat_s(missing, size, " ", _TRUNCATE);
		strncat_s(missing, size, name, _TRUNCATE);
	}
}

bool LoadApi(HMODULE dll)
{
	char missing[1024] = "";
	const size_t n = sizeof(missing);
	Load(dll, "SteamAPI_InitFlat", &g_api.init, missing, n);
	Load(dll, "SteamAPI_GetHSteamPipe", &g_api.get_pipe, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_Init", &g_api.dispatch_init, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_RunFrame", &g_api.run_frame, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_GetNextCallback", &g_api.next_callback, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_FreeLastCallback", &g_api.free_last_callback, missing, n);
	Load(dll, "SteamAPI_ManualDispatch_GetAPICallResult", &g_api.call_result, missing, n);
	Load(dll, "SteamAPI_SteamUser_v023", &g_api.user, missing, n);
	Load(dll, "SteamAPI_SteamFriends_v018", &g_api.friends, missing, n);
	Load(dll, "SteamAPI_SteamUtils_v011", &g_api.utils, missing, n);
	Load(dll, "SteamAPI_SteamMatchmaking_v009", &g_api.matchmaking, missing, n);
	Load(dll, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002", &g_api.messages, missing, n);
	Load(dll, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004", &g_api.net_utils, missing, n);
	Load(dll, "SteamAPI_ISteamUser_GetSteamID", &g_api.get_steam_id, missing, n);
	Load(dll, "SteamAPI_ISteamUtils_GetAppID", &g_api.get_app_id, missing, n);
	Load(dll, "SteamAPI_ISteamUtils_IsOverlayEnabled", &g_api.overlay_enabled, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetPersonaName", &g_api.persona_name, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendCount", &g_api.friend_count, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendByIndex", &g_api.friend_by_index, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendRelationship", &g_api.friend_relationship, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendPersonaName", &g_api.friend_name, missing, n);
	Load(dll, "SteamAPI_ISteamFriends_GetFriendGamePlayed", &g_api.friend_game, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_CreateLobby", &g_api.create_lobby, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_JoinLobby", &g_api.join_lobby, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_LeaveLobby", &g_api.leave_lobby, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_SetLobbyData", &g_api.set_lobby_data, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetLobbyData", &g_api.get_lobby_data, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers", &g_api.lobby_members, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetLobbyOwner", &g_api.lobby_owner, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_RequestLobbyList", &g_api.request_lobby_list, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter", &g_api.add_string_filter, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter", &g_api.add_distance_filter, missing, n);
	Load(dll, "SteamAPI_ISteamMatchmaking_GetLobbyByIndex", &g_api.lobby_by_index, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingMessages_SendMessageToUser", &g_api.send, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel", &g_api.receive, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser", &g_api.accept_session, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo", &g_api.session_info, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess", &g_api.init_relay, missing, n);
	Load(dll, "SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus", &g_api.relay_status, missing, n);
	Load(dll, "SteamAPI_SteamNetworkingMessage_t_Release", &g_api.release, missing, n);
	if (missing[0])
		hooklog::Write("steam: steam_api.dll lacks:%s (an older SDK?): spike off", missing);
	return missing[0] == 0;
}

const char *FriendName(SteamId id)
{
	if (id == g_me)
		return "ourselves";
	const char *name = g_api.friend_name(g_friends, id);
	return name && name[0] ? name : "?";
}

bool IsFriend(SteamId id)
{
	return g_api.friend_relationship(g_friends, id) == kFriendRelationshipFriend;
}

// A POP id is a 3- or 4-letter data centre code packed in 32 bits.
void PopName(uint32_t pop, char *out, size_t size)
{
	char code[5] = {};
	int n = 0;
	for (int shift = 24; shift >= 0; shift -= 8) {
		const char c = static_cast<char>((pop >> shift) & 0xff);
		if (c)
			code[n++] = c;
	}
	_snprintf_s(out, size, _TRUNCATE, "%s", n ? code : "none");
}

void LogSession(SteamId peer, const char *why)
{
	ConnectionInfo info = {};
	RealTimeStatus status = {};
	const int state = g_api.session_info(g_msgs, IdentityOf(peer), &info, &status);
	char relay[8];
	PopName(info.pop_relay, relay, sizeof(relay));
	hooklog::Write("steam: session with %s (%s): %s (%d), ping %d ms, %s (flags %d, relay POP %s), end reason %d \"%s\", \"%s\"",
	               FriendName(peer), why, steamspike::ConnectionStateName(state), state, status.ping_ms,
	               (info.flags & kConnectionFlagRelayed) ? "RELAYED" : "not relayed", info.flags, relay, info.end_reason,
	               info.end_debug, info.description);
}

void LogOverlay(const char *when)
{
	const bool enabled = g_api.overlay_enabled(g_utils);
	const bool loaded = GetModuleHandleW(L"GameOverlayRenderer.dll") != nullptr;
	hooklog::Write("steam: overlay %s: IsOverlayEnabled %s, GameOverlayRenderer.dll %s", when, enabled ? "yes" : "no",
	               loaded ? "loaded" : "not loaded");
}

void CheckRelay()
{
	RelayNetworkStatus status = {};
	const int avail = g_api.relay_status(g_netu, &status);
	status.avail = avail;
	const bool changed = !g_relay_logged || status.avail != g_relay.avail ||
	                     status.avail_network_config != g_relay.avail_network_config ||
	                     status.avail_any_relay != g_relay.avail_any_relay;
	if (changed) {
		hooklog::Write("steam: relay network at %lu ms: %s (network config %s, any relay %s, measuring pings %d) \"%s\"", Now(),
		               steamspike::AvailabilityName(status.avail), steamspike::AvailabilityName(status.avail_network_config),
		               steamspike::AvailabilityName(status.avail_any_relay), status.ping_measurement_in_progress,
		               status.debug_msg);
		g_relay = status;
		g_relay_logged = true;
	}
	if (status.avail == kAvailCurrent && !g_relay_ok_ms) {
		g_relay_ok_ms = Now();
		hooklog::Write("steam: relay network OK after %lu ms", g_relay_ok_ms);
	}
}

void RequestList(const char *value)
{
	g_api.add_string_filter(g_mm, "oh5_spike", value, kLobbyCompareEqual);
	g_api.add_distance_filter(g_mm, kLobbyDistanceWorldwide);
	g_list_call = g_api.request_lobby_list(g_mm);
	hooklog::Write("steam: lobby list asked (oh5_spike=%s, worldwide): call %llu", value,
	               static_cast<unsigned long long>(g_list_call));
}

void StartJoin(SteamId lobby, const char *how)
{
	if (g_join_call || g_lobby)
		return;
	g_join_call = g_api.join_lobby(g_mm, lobby);
	hooklog::Write("steam: joining lobby %llu (found %s): call %llu", static_cast<unsigned long long>(lobby), how,
	               static_cast<unsigned long long>(g_join_call));
}

void OnLobbyCreated(const LobbyCreated &r, bool failed)
{
	hooklog::Write("steam: CreateLobby(%s) -> %s (EResult %d)%s, lobby %llu", g_set.lobby_type_name,
	               steamspike::ResultName(r.result), r.result, failed ? ", call FAILED" : "",
	               static_cast<unsigned long long>(r.lobby));
	if (failed || r.result != kResultOk || !r.lobby)
		return;
	g_lobby = r.lobby;
	const char *value = g_set.mode == Mode::kHost ? "host" : "solo";
	const bool set1 = g_api.set_lobby_data(g_mm, g_lobby, "oh5_spike", value);
	const bool set2 = g_api.set_lobby_data(g_mm, g_lobby, "oh5_version", OPEN_HOMM5_VERSION);
	const char *back1 = g_api.get_lobby_data(g_mm, g_lobby, "oh5_spike");
	const char *back2 = g_api.get_lobby_data(g_mm, g_lobby, "oh5_version");
	const SteamId owner = g_api.lobby_owner(g_mm, g_lobby);
	hooklog::Write("steam: lobby data set %s/%s, read back oh5_spike=\"%s\" oh5_version=\"%s\"; members %d, owner %s",
	               set1 ? "yes" : "NO", set2 ? "yes" : "NO", back1 ? back1 : "(null)", back2 ? back2 : "(null)",
	               g_api.lobby_members(g_mm, g_lobby), owner == g_me ? "us" : "someone else");
	RequestList(value);
}

void OnLobbyList(const LobbyMatchList &r, bool failed)
{
	g_lobbies_listed = static_cast<int>(r.count);
	hooklog::Write("steam: lobby list -> %u lobbies%s", r.count, failed ? " (call FAILED)" : "");
	SteamId first = 0;
	for (int i = 0; i < static_cast<int>(r.count) && i < 20; ++i) {
		const SteamId lobby = g_api.lobby_by_index(g_mm, i);
		const char *value = g_api.get_lobby_data(g_mm, lobby, "oh5_spike");
		const char *version = g_api.get_lobby_data(g_mm, lobby, "oh5_version");
		hooklog::Write("steam:   lobby %llu%s, oh5_spike=\"%s\", oh5_version=\"%s\"", static_cast<unsigned long long>(lobby),
		               lobby == g_lobby ? " (ours)" : "", value ? value : "", version ? version : "");
		if (lobby == g_lobby)
			g_lobby_in_list = true;
		if (!first && lobby != g_lobby)
			first = lobby;
	}
	if (g_set.mode == Mode::kSolo && g_lobby && !g_lobby_in_list)
		hooklog::Write("steam: our lobby is not in the list (expected for a friends-only lobby: those never show in searches)");
	if (g_set.mode == Mode::kJoin && first)
		StartJoin(first, "in the lobby list");
}

void OnLobbyEnter(const LobbyEnter &r, const char *how)
{
	hooklog::Write("steam: lobby entered (%s): lobby %llu, response %u%s, locked %d", how,
	               static_cast<unsigned long long>(r.lobby), r.response, r.response == 1 ? " (success)" : " (refused)",
	               r.locked ? 1 : 0);
	if (g_set.mode != Mode::kJoin || r.response != 1 || g_target)
		return;
	g_lobby = r.lobby;
	g_target = g_api.lobby_owner(g_mm, g_lobby);
	const char *value = g_api.get_lobby_data(g_mm, g_lobby, "oh5_spike");
	const char *version = g_api.get_lobby_data(g_mm, g_lobby, "oh5_version");
	hooklog::Write("steam: the host is %s (%llu); lobby oh5_spike=\"%s\", oh5_version=\"%s\" (ours %s); members %d",
	               FriendName(g_target), static_cast<unsigned long long>(g_target), value ? value : "",
	               version ? version : "", OPEN_HOMM5_VERSION, g_api.lobby_members(g_mm, g_lobby));
	g_next_ping = Now() + 1000;
}

void OnCallCompleted(const ApiCallCompleted &c)
{
	alignas(8) uint8_t buf[512] = {};
	bool failed = false;
	if (c.call == g_create_call) {
		g_create_call = 0;
		if (g_api.call_result(g_pipe, c.call, buf, sizeof(LobbyCreated), kCallLobbyCreated, &failed))
			OnLobbyCreated(*reinterpret_cast<const LobbyCreated *>(buf), failed);
		else
			hooklog::Write("steam: CreateLobby: no result (call failed)");
	} else if (c.call == g_list_call) {
		g_list_call = 0;
		if (g_api.call_result(g_pipe, c.call, buf, sizeof(LobbyMatchList), kCallLobbyMatchList, &failed))
			OnLobbyList(*reinterpret_cast<const LobbyMatchList *>(buf), failed);
		else
			hooklog::Write("steam: RequestLobbyList: no result (call failed)");
	} else if (c.call == g_join_call) {
		g_join_call = 0;
		if (g_api.call_result(g_pipe, c.call, buf, sizeof(LobbyEnter), kCallLobbyEnter, &failed))
			OnLobbyEnter(*reinterpret_cast<const LobbyEnter *>(buf), failed ? "join call FAILED" : "join call");
		else
			hooklog::Write("steam: JoinLobby: no result (call failed)");
	}
}

Peer *PeerOf(SteamId id)
{
	for (Peer &p : g_peers) {
		if (p.id == id)
			return &p;
		if (!p.id) {
			p.id = id;
			return &p;
		}
	}
	return nullptr;
}

void OnSessionRequest(const SessionRequest &r)
{
	const SteamId from = SteamIdOf(r.remote);
	const bool allowed = from && (from == g_me || from == g_target || IsFriend(from));
	if (allowed) {
		const bool ok = g_api.accept_session(g_msgs, r.remote);
		hooklog::Write("steam: session request from %s (%llu): accepted%s", FriendName(from),
		               static_cast<unsigned long long>(from), ok ? "" : " (AcceptSessionWithUser said no)");
	} else {
		hooklog::Write("steam: session request from %llu (identity type %d): ignored, not a friend",
		               static_cast<unsigned long long>(from), r.remote.type);
	}
}

void OnCallback(const CallbackMsg &msg)
{
	const void *p = msg.param;
	switch (msg.id) {
	case kCallApiCallCompleted:
		if (msg.size >= static_cast<int>(sizeof(ApiCallCompleted)))
			OnCallCompleted(*static_cast<const ApiCallCompleted *>(p));
		return;
	case kCallRelayNetworkStatus:
		CheckRelay();
		return;
	case kCallLobbyEnter:
		// also posted for our own lobby; the join's own result comes as a call result
		if (msg.size >= static_cast<int>(sizeof(LobbyEnter)))
			hooklog::Write("steam: (callback) lobby entered: %llu, response %u",
			               static_cast<unsigned long long>(static_cast<const LobbyEnter *>(p)->lobby),
			               static_cast<const LobbyEnter *>(p)->response);
		return;
	case kCallLobbyDataUpdate:
		if (msg.size >= static_cast<int>(sizeof(LobbyDataUpdate))) {
			const LobbyDataUpdate *u = static_cast<const LobbyDataUpdate *>(p);
			hooklog::Write("steam: lobby data update: lobby %llu, %s, success %u", static_cast<unsigned long long>(u->lobby),
			               u->member == u->lobby ? "the lobby's data" : FriendName(u->member), u->success);
		}
		return;
	case kCallLobbyChatUpdate:
		if (msg.size >= static_cast<int>(sizeof(LobbyChatUpdate))) {
			const LobbyChatUpdate *u = static_cast<const LobbyChatUpdate *>(p);
			hooklog::Write("steam: lobby members: %s %s (state bits %u)", FriendName(u->user_changed),
			               (u->state_change & 1) ? "entered" : "left", u->state_change);
		}
		return;
	case kCallSessionRequest:
		if (msg.size >= static_cast<int>(sizeof(SessionRequest)))
			OnSessionRequest(*static_cast<const SessionRequest *>(p));
		return;
	case kCallSessionFailed:
		if (msg.size >= static_cast<int>(sizeof(SessionFailed))) {
			const ConnectionInfo &info = static_cast<const SessionFailed *>(p)->info;
			hooklog::Write("steam: session FAILED with %s: state %s, end reason %d \"%s\"", FriendName(SteamIdOf(info.remote)),
			               steamspike::ConnectionStateName(info.state), info.end_reason, info.end_debug);
		}
		return;
	case kCallGameOverlayActivated:
		if (msg.size >= static_cast<int>(sizeof(GameOverlayActivated)))
			hooklog::Write("steam: overlay %s", static_cast<const GameOverlayActivated *>(p)->active ? "opened" : "closed");
		return;
	default:
		if (++g_unknown_callbacks <= 20)
			hooklog::Write("steam: (callback %d, %d bytes)", msg.id, msg.size);
		return;
	}
}

void PumpCallbacks()
{
	g_api.run_frame(g_pipe);
	CallbackMsg msg = {};
	while (g_api.next_callback(g_pipe, &msg)) {
		OnCallback(msg);
		g_api.free_last_callback(g_pipe);
	}
}

bool SendPing(SteamId to, steamspike::Kind kind, uint32_t sequence, uint32_t sent_ms, int flags, int size)
{
	uint8_t buf[2048];
	const steamspike::Ping ping = {kind, sequence, sent_ms};
	const size_t n = steamspike::EncodePing(ping, buf, sizeof(buf), static_cast<size_t>(size));
	const int r = g_api.send(g_msgs, IdentityOf(to), buf, static_cast<uint32_t>(n), flags, kChannel);
	if (r != kResultOk || g_set.mode == Mode::kSolo)
		hooklog::Write("steam: message #%u (%s, %d bytes, %s) to %s -> %s (EResult %d)", sequence,
		               kind == steamspike::Kind::kPing ? "ping" : "echo", size,
		               (flags & kSendReliable) ? "reliable" : "unreliable", FriendName(to), steamspike::ResultName(r), r);
	return r == kResultOk;
}

void ReceiveMessages()
{
	Message *msgs[32];
	for (;;) {
		const int n = g_api.receive(g_msgs, kChannel, msgs, 32);
		if (n <= 0)
			return;
		for (int i = 0; i < n; ++i) {
			const Message *m = msgs[i];
			const SteamId from = SteamIdOf(m->peer);
			steamspike::Ping ping = {};
			const bool ours = steamspike::DecodePing(static_cast<const uint8_t *>(m->data), static_cast<size_t>(m->size), &ping);
			if (!ours) {
				hooklog::Write("steam: %d-byte message from %s that is not ours", m->size, FriendName(from));
			} else if (g_set.mode == Mode::kSolo) {
				++g_self_received;
				hooklog::Write("steam: message #%u came back from %s after %lu ms (%d bytes)", ping.sequence, FriendName(from),
				               Now() - ping.sent_ms, m->size);
			} else if (g_set.mode == Mode::kHost && ping.kind == steamspike::Kind::kPing) {
				const bool allowed = from == g_me || IsFriend(from);
				Peer *peer = PeerOf(from);
				if (peer && ++peer->pings == 1)
					hooklog::Write("steam: first ping from %s (%llu)%s", FriendName(from), static_cast<unsigned long long>(from),
					               allowed ? "" : ": not a friend, no echo");
				if (allowed)
					SendPing(from, steamspike::Kind::kEcho, ping.sequence, ping.sent_ms, kSendUnreliableNoNagle,
					         m->size);
			} else if (g_set.mode == Mode::kJoin && ping.kind == steamspike::Kind::kEcho && from == g_target) {
				g_rtt.Add(Now() - ping.sent_ms);
			}
			g_api.release(msgs[i]);
		}
	}
}

void Finish(const char *summary)
{
	if (g_lobby) {
		g_api.leave_lobby(g_mm, g_lobby);
		hooklog::Write("steam: left lobby %llu", static_cast<unsigned long long>(g_lobby));
	}
	hooklog::Write("steam: spike done (%s, app id %u): %s", steamspike::ModeName(g_set.mode), g_set.app_id, summary);
	g_done = true;
}

void StepSolo(DWORD now)
{
	// messages to ourselves, from 5 s on, one a second; number 3 is reliable
	if (now >= 5000 && g_self_sent < 5 && now >= 5000 + 1000u * static_cast<DWORD>(g_self_sent)) {
		++g_self_sent;
		const int flags = (g_self_sent == 3 ? kSendReliable : kSendUnreliableNoNagle) | kSendAutoRestartBrokenSession;
		if (!SendPing(g_me, steamspike::Kind::kPing, static_cast<uint32_t>(g_self_sent), now, flags, g_set.ping_bytes))
			++g_self_send_failed;
	}
	if (now >= g_next_session_log && g_self_sent) {
		g_next_session_log += 20000;
		LogSession(g_me, "messages to ourselves");
	}
	if (now >= 45000 && !g_overlay_logged_late) {
		g_overlay_logged_late = true;
		LogOverlay("at 45 s");
	}
	if (now >= static_cast<DWORD>(g_set.seconds) * 1000) {
		char summary[512];
		_snprintf_s(summary, sizeof(summary), _TRUNCATE,
		            "relay %s (%s), lobby %s, lobby list %d, messages to ourselves sent %d (failed %d), came back %d",
		            g_relay_ok_ms ? "OK" : "NOT OK", steamspike::AvailabilityName(g_relay.avail),
		            g_lobby ? "created" : "NOT created", g_lobbies_listed, g_self_sent, g_self_send_failed, g_self_received);
		Finish(summary);
	}
}

void StepHost(DWORD now)
{
	if (now >= g_next_peer_log) {
		g_next_peer_log += 30000;
		for (const Peer &p : g_peers)
			if (p.id)
				LogSession(p.id, "the joiner");
	}
	if (now >= static_cast<DWORD>(g_set.seconds) * 1000) {
		char summary[256];
		int peers = 0, pings = 0;
		for (const Peer &p : g_peers) {
			peers += p.id ? 1 : 0;
			pings += p.pings;
		}
		_snprintf_s(summary, sizeof(summary), _TRUNCATE, "relay %s, lobby %s, %d joiner(s) sent %d pings",
		            g_relay_ok_ms ? "OK" : "NOT OK", g_lobby ? "created" : "NOT created", peers, pings);
		Finish(summary);
	}
}

void SearchHost(DWORD now)
{
	if (now < g_next_search)
		return;
	g_next_search = now + 5000;
	const int count = g_api.friend_count(g_friends, kFriendFlagImmediate);
	int playing = 0;
	for (int i = 0; i < count; ++i) {
		const SteamId id = g_api.friend_by_index(g_friends, i, kFriendFlagImmediate);
		FriendGameInfo info = {};
		if (!g_api.friend_game(g_friends, id, &info) || steamspike::AppIdOfGameId(info.game_id) != g_set.app_id)
			continue;
		++playing;
		const SteamId lobby = LobbyOf(info);
		hooklog::Write("steam: friend %s plays app %u, lobby %llu", FriendName(id), g_set.app_id,
		               static_cast<unsigned long long>(lobby));
		if (lobby)
			StartJoin(lobby, "through the friends list");
	}
	if (!playing)
		hooklog::Write("steam: none of %d friends plays app %u yet", count, g_set.app_id);
	if (!g_list_call && !g_join_call && !g_lobby)
		RequestList("host");
}

void StepJoin(DWORD now)
{
	if (!g_target) {
		SearchHost(now);
		if (now >= static_cast<DWORD>(g_set.seconds) * 1000)
			Finish("NO host lobby found (is the host's game running with spike=host? try spike_lobby=public on the host)");
		return;
	}
	if (g_pings_sent < g_set.pings && now >= g_next_ping) {
		g_next_ping = now + 50;
		++g_pings_sent;
		if (!SendPing(g_target, steamspike::Kind::kPing, static_cast<uint32_t>(g_pings_sent), now,
		              kSendUnreliableNoNagle | kSendAutoRestartBrokenSession, g_set.ping_bytes))
			++g_ping_send_failed;
		if (g_pings_sent == 1)
			LogSession(g_target, "first ping sent");
		if (g_pings_sent == g_set.pings)
			g_pings_done_at = now;
	}
	if (g_pings_done_at && now >= g_pings_done_at + 5000) {
		LogSession(g_target, "after the pings");
		char summary[256];
		_snprintf_s(summary, sizeof(summary), _TRUNCATE,
		            "%d pings sent (%d refused), %d echoes back; round trip min %u, median %u, max %u ms", g_pings_sent,
		            g_ping_send_failed, g_rtt.Count(), g_rtt.Min(), g_rtt.Median(), g_rtt.Max());
		Finish(summary);
	}
}

DWORD WINAPI SpikeThread(LPVOID)
{
	g_api.init_relay(g_netu);
	hooklog::Write("steam: InitRelayNetworkAccess called");
	CheckRelay();
	if (g_set.mode != Mode::kJoin) {
		g_create_call = g_api.create_lobby(g_mm, g_set.lobby_type, g_set.mode == Mode::kHost ? 8 : 4);
		hooklog::Write("steam: CreateLobby(%s) asked: call %llu", g_set.lobby_type_name,
		               static_cast<unsigned long long>(g_create_call));
	}
	while (!g_done) {
		PumpCallbacks();
		ReceiveMessages();
		const DWORD now = Now();
		if (now >= g_next_relay_check) {
			g_next_relay_check = now + 1000;
			CheckRelay();
		}
		if (g_set.mode == Mode::kSolo)
			StepSolo(now);
		else if (g_set.mode == Mode::kHost)
			StepHost(now);
		else
			StepJoin(now);
		Sleep(kTickMs);
	}
	for (;;) {  // keep Steam's queue drained, slowly
		PumpCallbacks();
		Sleep(500);
	}
}

void ReadSettings()
{
	wchar_t path[MAX_PATH];
	hooklog::PathNextToExe(L"open-homm5", L".ini", path, MAX_PATH);
	wchar_t text[64];
	GetPrivateProfileStringW(L"online", L"spike", L"0", text, 64, path);
	g_set.mode = steamspike::ParseMode(text);
	g_set.app_id = GetPrivateProfileIntW(L"online", L"app_id", 15370, path);
	const int default_seconds = g_set.mode == Mode::kHost ? 900 : g_set.mode == Mode::kJoin ? 300 : 60;
	g_set.seconds = GetPrivateProfileIntW(L"online", L"spike_seconds", default_seconds, path);
	GetPrivateProfileStringW(L"online", L"spike_lobby", L"friends", text, 64, path);
	g_set.lobby_type = kLobbyFriendsOnly;
	g_set.lobby_type_name = "friends only";
	if (_wcsicmp(text, L"public") == 0) {
		g_set.lobby_type = kLobbyPublic;
		g_set.lobby_type_name = "public";
	} else if (_wcsicmp(text, L"invisible") == 0) {
		g_set.lobby_type = kLobbyInvisible;
		g_set.lobby_type_name = "invisible";
	}
	g_set.pings = GetPrivateProfileIntW(L"online", L"spike_pings", 100, path);
	g_set.ping_bytes = GetPrivateProfileIntW(L"online", L"spike_ping_bytes", 300, path);
	if (g_set.ping_bytes < static_cast<int>(steamspike::kHeaderSize) || g_set.ping_bytes > 1200)
		g_set.ping_bytes = 300;
	if (g_set.pings < 1 || g_set.pings > 1000)
		g_set.pings = 100;
}

void SetAppIdVariable(const wchar_t *name)
{
	wchar_t before[32] = L"";
	const DWORD n = GetEnvironmentVariableW(name, before, 32);
	wchar_t value[16];
	_snwprintf_s(value, 16, _TRUNCATE, L"%u", g_set.app_id);
	SetEnvironmentVariableW(name, value);
	hooklog::Write("steam: %ls was %ls, now %ls", name, n && n < 32 ? before : L"(not set)", value);
}

}  // namespace

void ApplySteamSpikeFromSettings()
{
	ReadSettings();
	if (g_set.mode == Mode::kOff)
		return;
	hooklog::Write("steam: spike %s, app id %u, %d s, lobby %s", steamspike::ModeName(g_set.mode), g_set.app_id,
	               g_set.seconds, g_set.lobby_type_name);
	// bin\open-homm5\steam_api.dll (one file for every robot instance, so not PathNextToExe)
	wchar_t dll_path[MAX_PATH];
	DWORD n = GetModuleFileNameW(nullptr, dll_path, MAX_PATH);
	while (n > 0 && dll_path[n - 1] != L'\\')
		--n;
	lstrcpynW(dll_path + n, L"open-homm5\\steam_api.dll", MAX_PATH - n);
	HMODULE dll = LoadLibraryExW(dll_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (!dll) {
		hooklog::Write("steam: cannot load %ls (error %lu): spike off", dll_path, GetLastError());
		return;
	}
	if (!LoadApi(dll))
		return;
	wchar_t cwd[MAX_PATH] = L"";
	GetCurrentDirectoryW(MAX_PATH, cwd);
	wchar_t appid_txt[MAX_PATH];
	_snwprintf_s(appid_txt, MAX_PATH, _TRUNCATE, L"%s\\steam_appid.txt", cwd);
	if (GetFileAttributesW(appid_txt) != INVALID_FILE_ATTRIBUTES)
		hooklog::Write("steam: note: %ls exists and overrides our app id", appid_txt);
	SetAppIdVariable(L"SteamAppId");
	SetAppIdVariable(L"SteamGameId");
	char err[kErrMsgSize] = "";
	const DWORD t0 = GetTickCount();
	const int r = g_api.init(err);
	hooklog::Write("steam: SteamAPI_InitFlat -> %d (%s) in %lu ms%s%s", r, steamspike::InitResultName(r),
	               GetTickCount() - t0, err[0] ? ": " : "", err);
	if (r != kInitOk)
		return;
	g_api.dispatch_init();
	g_pipe = g_api.get_pipe();
	g_user = g_api.user();
	g_friends = g_api.friends();
	g_utils = g_api.utils();
	g_mm = g_api.matchmaking();
	g_msgs = g_api.messages();
	g_netu = g_api.net_utils();
	if (!g_user || !g_friends || !g_utils || !g_mm || !g_msgs || !g_netu) {
		hooklog::Write("steam: an interface is missing (user %p, friends %p, utils %p, matchmaking %p, messages %p, "
		               "networking utils %p): spike off",
		               g_user, g_friends, g_utils, g_mm, g_msgs, g_netu);
		return;
	}
	g_me = g_api.get_steam_id(g_user);
	hooklog::Write("steam: logged in as \"%s\" (SteamID %llu); Steam says our app id is %u", g_api.persona_name(g_friends),
	               static_cast<unsigned long long>(g_me), g_api.get_app_id(g_utils));
	LogOverlay("at init");
	g_start = GetTickCount();
	HANDLE t = CreateThread(nullptr, 0, &SpikeThread, nullptr, 0, nullptr);
	if (t)
		CloseHandle(t);
	else
		hooklog::Write("steam: cannot start the spike thread (error %lu)", GetLastError());
}

}  // namespace mods
