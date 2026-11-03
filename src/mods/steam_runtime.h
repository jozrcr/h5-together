// Starting Steam inside the game's process (docs/plans/online-steam.md), for the
// Steam spike (steam_spike.cpp, a test) and the virtual LAN's Steam transport
// (virtual_lan_steam.cpp, [online] mode=steam).
//
// - steam_api.dll (32-bit, from Valve's Steamworks SDK) is loaded from
//   bin\h5-together\ at run time. Without it the add-on runs as usual.
// - SteamAppId and SteamGameId are set in this process (nothing on disk) before
//   SteamAPI_InitFlat, so Steam knows the app even when Steam did not start the
//   game (GameRanger, a shortcut, the robot). SteamAPI_RestartAppIfNecessary is
//   never called: it would restart the game through Steam, which runs Steam's
//   own H5_Game.exe, not MMH5.5's.
// - Callbacks use Steam's manual dispatch: they come only when a thread of ours
//   asks for them, never on one of the game's threads.
// Steam starts once per process: one user at a time (the spike and mode=steam
// never run together; steam_spike.cpp checks).
#pragma once

#include <stdint.h>

#include "steam_api_flat.h"

namespace steamrt {

// The flat functions we call (steam_api_flat.h), found with GetProcAddress.
struct Api {
	steamflat::InitFlatFn init;
	steamflat::GetPipeFn get_pipe;
	steamflat::VoidFn dispatch_init;
	steamflat::RunFrameFn run_frame;
	steamflat::GetNextCallbackFn next_callback;
	steamflat::RunFrameFn free_last_callback;
	steamflat::GetApiCallResultFn call_result;
	steamflat::AccessorFn user, friends, utils, matchmaking, messages, net_utils;
	steamflat::GetSteamIdFn get_steam_id;
	steamflat::GetAppIdFn get_app_id;
	steamflat::IfaceBoolFn overlay_enabled;
	steamflat::GetPersonaNameFn persona_name;
	steamflat::GetFriendCountFn friend_count;
	steamflat::GetFriendByIndexFn friend_by_index;
	steamflat::GetFriendRelationshipFn friend_relationship;
	steamflat::GetFriendPersonaNameFn friend_name;
	steamflat::GetFriendGamePlayedFn friend_game;
	steamflat::CreateLobbyFn create_lobby;
	steamflat::JoinLobbyFn join_lobby;
	steamflat::LeaveLobbyFn leave_lobby;
	steamflat::SetLobbyDataFn set_lobby_data;
	steamflat::GetLobbyDataFn get_lobby_data;
	steamflat::GetNumLobbyMembersFn lobby_members;
	steamflat::GetLobbyOwnerFn lobby_owner;
	steamflat::GetLobbyMemberByIndexFn lobby_member;
	steamflat::GetLobbyMemberDataFn get_member_data;
	steamflat::SetLobbyMemberDataFn set_member_data;
	steamflat::RequestLobbyDataFn request_lobby_data;
	steamflat::RequestLobbyListFn request_lobby_list;
	steamflat::AddStringFilterFn add_string_filter;
	steamflat::AddDistanceFilterFn add_distance_filter;
	steamflat::GetLobbyByIndexFn lobby_by_index;
	steamflat::SendMessageToUserFn send;
	steamflat::ReceiveMessagesFn receive;
	steamflat::SessionFn accept_session;
	steamflat::GetSessionInfoFn session_info;
	steamflat::InitRelayFn init_relay;
	steamflat::GetRelayStatusFn relay_status;
	steamflat::GetLocalTimestampFn local_timestamp;
	steamflat::ReleaseMessageFn release;
	// step 4, the online UI
	steamflat::SetRichPresenceFn set_rich_presence;
	steamflat::GetFriendRichPresenceFn friend_rich_presence;
	steamflat::RequestFriendRichPresenceFn request_rich_presence;
	steamflat::GetFriendPersonaStateFn friend_state;
	steamflat::ActivateInviteDialogFn invite_dialog;
	steamflat::InviteUserToLobbyFn invite_user;
};

struct Steam {
	Api api;
	steamflat::Pipe pipe;
	steamflat::Iface user, friends, utils, mm, msgs, netu;
	steamflat::SteamId me;   // our SteamID
	uint32_t app_id;         // the app id we asked for ([online] app_id, 15370 by default)
	uint32_t steam_app_id;   // the one Steam says we run as
	uint32_t init_ms;        // how long SteamAPI_InitFlat took
	bool launched_by_steam;  // SteamAppId was set before ours: Steam started the game
};

// Loads steam_api.dll and starts Steam as `app_id`, on the calling thread (the
// game's, at its first DirectInput call). `who` begins every log line ("steam",
// "vlan steam"). Null if Steam did not start: the log says why (no
// steam_api.dll, Steam not running, ...). Only the first call does the work;
// later ones get its answer.
const Steam *Start(uint32_t app_id, const char *who);

// Why Steam did not start (for the online UI's "Steam is not available" line).
enum class Failure {
	kNone,         // it started (or was never tried)
	kNoDll,        // no steam_api.dll in bin\h5-together\ (a GOG copy, or not installed)
	kOldDll,       // a steam_api.dll without the functions we call (an older SDK)
	kNoSteam,      // SteamAPI_InitFlat: the Steam client is not running (or not reachable from Proton)
	kSteamTooOld,  // SteamAPI_InitFlat: the Steam client must be updated
	kRefused,      // SteamAPI_InitFlat failed otherwise (not logged in? the game not owned on Steam?)
	kNoAnswer,     // started, but an interface or our SteamID is missing
};
Failure LastFailure();

}  // namespace steamrt
