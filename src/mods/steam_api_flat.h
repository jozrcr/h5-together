// Our own declarations of the few Steamworks functions and structures the
// Steam spike and the virtual LAN's Steam transport use
// (docs/plans/online-steam.md). The SDK itself is never in
// the repo: its licence lets us use it to build our software, not republish
// its headers. These are the flat C entry points of steam_api.dll (32-bit,
// __cdecl), loaded at run time with GetProcAddress, so the DLL still works
// with no steam_api.dll at all.
//
// The layouts follow the SDK 1.65 headers on 32-bit Windows (callback
// structures 8-byte packed, network identities 1-byte packed).
// src/hook/test/steam_abi_check.cpp checks every size, offset and number here
// against a local copy of the SDK (ref\steamworks_sdk_*), when there is one.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace steamflat {

typedef uint64_t SteamId;   // CSteamID as 64 bits
typedef uint64_t ApiCall;   // SteamAPICall_t
typedef int32_t Pipe;       // HSteamPipe
typedef int32_t UserHandle; // HSteamUser

// ESteamAPIInitResult
enum InitResult { kInitOk = 0, kInitFailedGeneric = 1, kInitNoSteamClient = 2, kInitVersionMismatch = 3 };
const int kErrMsgSize = 1024;  // SteamErrMsg

// EResult values we name (the rest are logged as numbers)
const int kResultOk = 1;

// EChatMemberStateChange bits (LobbyChatUpdate_t)
const uint32_t kMemberEntered = 1;
const uint32_t kMemberLeft = 2;
const uint32_t kMemberDisconnected = 4;
const uint32_t kMemberKicked = 8;
const uint32_t kMemberBanned = 16;

// EChatRoomEnterResponse: success
const uint32_t kEnterSuccess = 1;

typedef int64_t Microseconds;  // SteamNetworkingMicroseconds

// ELobbyType, ELobbyComparison, ELobbyDistanceFilter
enum LobbyType { kLobbyPrivate = 0, kLobbyFriendsOnly = 1, kLobbyPublic = 2, kLobbyInvisible = 3 };
const int kLobbyCompareEqual = 0;
const int kLobbyDistanceWorldwide = 3;

// EFriendFlags / EFriendRelationship
const int kFriendFlagImmediate = 4;  // regular friends
const int kFriendRelationshipFriend = 3;

// EPersonaState (the ones we name)
const int kPersonaOffline = 0;
const int kPersonaOnline = 1;
const int kPersonaBusy = 2;
const int kPersonaAway = 3;
const int kPersonaSnooze = 4;

// ESteamNetworkingAvailability
enum Availability {
	kAvailCannotTry = -102,
	kAvailFailed = -101,
	kAvailPreviously = -100,
	kAvailRetrying = -10,
	kAvailUnknown = 0,
	kAvailNeverTried = 1,
	kAvailWaiting = 2,
	kAvailAttempting = 3,
	kAvailCurrent = 100,
};

// ESteamNetworkingConnectionState (the ones we name)
enum ConnectionState {
	kConnNone = 0,
	kConnConnecting = 1,
	kConnFindingRoute = 2,
	kConnConnected = 3,
	kConnClosedByPeer = 4,
	kConnProblemDetectedLocally = 5,
};

// k_nSteamNetworkingSend_*
const int kSendUnreliableNoNagle = 1;
const int kSendReliable = 8;
const int kSendAutoRestartBrokenSession = 32;

// k_nSteamNetworkConnectionInfoFlags_Relayed
const int kConnectionFlagRelayed = 16;

// ESteamNetworkingIdentityType: SteamID
const int kIdentitySteamId = 16;

// SteamNetworkingIdentity (136 bytes, 1-byte packed in the SDK; every field
// here falls on its natural place, so no packing is needed to match it)
struct Identity {
	int32_t type;       // m_eType
	int32_t size;       // m_cbSize
	uint8_t data[128];  // union: m_steamID64 is the first 8 bytes
};

// SteamNetworkingMessage_t
struct Message {
	void *data;          // m_pData
	int32_t size;        // m_cbSize
	uint32_t conn;       // m_conn
	Identity peer;       // m_identityPeer
	int64_t conn_user_data;
	int64_t time_received_us;
	int64_t message_number;
	void(__cdecl *free_data)(Message *);
	void(__cdecl *release)(Message *);
	int32_t channel;     // m_nChannel
	int32_t flags;
	int64_t user_data;
	uint16_t lane;
	uint16_t pad;
};

// SteamNetworkingIPAddr (18 bytes, 1-byte packed in the SDK)
struct IpAddr {
	uint8_t bytes[18];
};

// SteamNetConnectionInfo_t
struct ConnectionInfo {
	Identity remote;            // m_identityRemote: 136 bytes at 0
	int64_t user_data;          // 136
	uint32_t listen_socket;     // 144
	IpAddr remote_address;      // 148
	uint16_t pad1;              // 166
	uint32_t pop_remote;        // m_idPOPRemote, 168
	uint32_t pop_relay;         // m_idPOPRelay, 172
	int32_t state;              // m_eState, 176
	int32_t end_reason;         // m_eEndReason, 180
	char end_debug[128];        // m_szEndDebug, 184
	char description[128];      // m_szConnectionDescription, 312
	int32_t flags;              // m_nFlags, 440
	uint32_t reserved[63];
};

// SteamNetConnectionRealTimeStatus_t
struct RealTimeStatus {
	int32_t state;
	int32_t ping_ms;
	float quality_local;
	float quality_remote;
	float out_packets_per_sec;
	float out_bytes_per_sec;
	float in_packets_per_sec;
	float in_bytes_per_sec;
	int32_t send_rate_bytes_per_sec;
	int32_t pending_unreliable;
	int32_t pending_reliable;
	int32_t sent_unacked_reliable;
	int64_t queue_time_us;
	uint32_t reserved[16];
};

// FriendGameInfo_t
struct FriendGameInfo {
	uint64_t game_id;      // CGameID; the app id is its low 24 bits
	uint32_t game_ip;
	uint16_t game_port;
	uint16_t query_port;
	uint8_t lobby[8];      // CSteamID m_steamIDLobby (1-byte aligned in the SDK: at offset 16)
};

// CallbackMsg_t
struct CallbackMsg {
	UserHandle user;
	int32_t id;       // m_iCallback
	uint8_t *param;   // m_pubParam
	int32_t size;     // m_cubParam
};

// Callback numbers (k_iCallback) and their structures.
const int kCallApiCallCompleted = 703;            // SteamAPICallCompleted_t
struct ApiCallCompleted {
	ApiCall call;
	int32_t id;
	uint32_t size;
};
const int kCallLobbyEnter = 504;                  // LobbyEnter_t
struct LobbyEnter {
	SteamId lobby;
	uint32_t chat_permissions;
	bool locked;
	uint32_t response;  // EChatRoomEnterResponse: 1 = success
};
const int kCallLobbyDataUpdate = 505;             // LobbyDataUpdate_t
struct LobbyDataUpdate {
	SteamId lobby;
	SteamId member;
	uint8_t success;
};
const int kCallLobbyChatUpdate = 506;             // LobbyChatUpdate_t
struct LobbyChatUpdate {
	SteamId lobby;
	SteamId user_changed;
	SteamId making_change;
	uint32_t state_change;  // EChatMemberStateChange bits: 1 entered, 2 left, 4 disconnected
};
const int kCallLobbyMatchList = 510;              // LobbyMatchList_t
struct LobbyMatchList {
	uint32_t count;
};
const int kCallLobbyCreated = 513;                // LobbyCreated_t
struct LobbyCreated {
	int32_t result;  // EResult
	SteamId lobby;
};
const int kCallGameOverlayActivated = 331;        // GameOverlayActivated_t
struct GameOverlayActivated {
	uint8_t active;
	bool user_initiated;
	uint32_t app_id;
	uint32_t overlay_pid;
};
const int kCallGameLobbyJoinRequested = 333;      // GameLobbyJoinRequested_t: an invite accepted, or "Join game"
struct GameLobbyJoinRequested {
	SteamId lobby;
	SteamId friend_id;  // the friend it came through (0 if not directly through a friend)
};
const int kCallRelayNetworkStatus = 1281;         // SteamRelayNetworkStatus_t
struct RelayNetworkStatus {
	int32_t avail;
	int32_t ping_measurement_in_progress;
	int32_t avail_network_config;
	int32_t avail_any_relay;
	char debug_msg[256];
};
const int kCallSessionRequest = 1251;             // SteamNetworkingMessagesSessionRequest_t
struct SessionRequest {
	Identity remote;
};
const int kCallSessionFailed = 1252;              // SteamNetworkingMessagesSessionFailed_t
struct SessionFailed {
	ConnectionInfo info;
};

// The flat functions (all __cdecl). Interfaces are opaque pointers.
typedef void *Iface;
typedef int(__cdecl *InitFlatFn)(char *err_msg);
typedef void(__cdecl *VoidFn)();
typedef Pipe(__cdecl *GetPipeFn)();
typedef void(__cdecl *RunFrameFn)(Pipe);
typedef bool(__cdecl *GetNextCallbackFn)(Pipe, CallbackMsg *);
typedef bool(__cdecl *GetApiCallResultFn)(Pipe, ApiCall, void *, int, int, bool *);
typedef Iface(__cdecl *AccessorFn)();
typedef SteamId(__cdecl *GetSteamIdFn)(Iface);
typedef uint32_t(__cdecl *GetAppIdFn)(Iface);
typedef bool(__cdecl *IfaceBoolFn)(Iface);
typedef const char *(__cdecl *GetPersonaNameFn)(Iface);
typedef int(__cdecl *GetFriendCountFn)(Iface, int flags);
typedef SteamId(__cdecl *GetFriendByIndexFn)(Iface, int index, int flags);
typedef int(__cdecl *GetFriendRelationshipFn)(Iface, SteamId);
typedef const char *(__cdecl *GetFriendPersonaNameFn)(Iface, SteamId);
typedef bool(__cdecl *GetFriendGamePlayedFn)(Iface, SteamId, FriendGameInfo *);
typedef ApiCall(__cdecl *CreateLobbyFn)(Iface, int type, int max_members);
typedef ApiCall(__cdecl *JoinLobbyFn)(Iface, SteamId);
typedef void(__cdecl *LeaveLobbyFn)(Iface, SteamId);
typedef bool(__cdecl *SetLobbyDataFn)(Iface, SteamId, const char *key, const char *value);
typedef const char *(__cdecl *GetLobbyDataFn)(Iface, SteamId, const char *key);
typedef int(__cdecl *GetNumLobbyMembersFn)(Iface, SteamId);
typedef SteamId(__cdecl *GetLobbyOwnerFn)(Iface, SteamId);
typedef ApiCall(__cdecl *RequestLobbyListFn)(Iface);
typedef void(__cdecl *AddStringFilterFn)(Iface, const char *key, const char *value, int comparison);
typedef void(__cdecl *AddDistanceFilterFn)(Iface, int distance);
typedef SteamId(__cdecl *GetLobbyByIndexFn)(Iface, int index);
typedef int(__cdecl *SendMessageToUserFn)(Iface, const Identity &, const void *, uint32_t, int flags, int channel);
typedef int(__cdecl *ReceiveMessagesFn)(Iface, int channel, Message **out, int max);
typedef bool(__cdecl *SessionFn)(Iface, const Identity &);
typedef int(__cdecl *GetSessionInfoFn)(Iface, const Identity &, ConnectionInfo *, RealTimeStatus *);
typedef void(__cdecl *InitRelayFn)(Iface);
typedef int(__cdecl *GetRelayStatusFn)(Iface, RelayNetworkStatus *);
typedef void(__cdecl *ReleaseMessageFn)(Message *);
// added for the virtual LAN's Steam transport (virtual_lan_steam.cpp)
typedef SteamId(__cdecl *GetLobbyMemberByIndexFn)(Iface, SteamId lobby, int index);
typedef const char *(__cdecl *GetLobbyMemberDataFn)(Iface, SteamId lobby, SteamId user, const char *key);
typedef void(__cdecl *SetLobbyMemberDataFn)(Iface, SteamId lobby, const char *key, const char *value);
typedef bool(__cdecl *RequestLobbyDataFn)(Iface, SteamId lobby);
typedef Microseconds(__cdecl *GetLocalTimestampFn)(Iface);
// added for step 4, the online UI (online_ui.cpp): invites, friends' rich presence
typedef bool(__cdecl *SetRichPresenceFn)(Iface, const char *key, const char *value);
typedef const char *(__cdecl *GetFriendRichPresenceFn)(Iface, SteamId, const char *key);
typedef void(__cdecl *RequestFriendRichPresenceFn)(Iface, SteamId);
typedef int(__cdecl *GetFriendPersonaStateFn)(Iface, SteamId);
typedef void(__cdecl *ActivateInviteDialogFn)(Iface, SteamId lobby);
typedef bool(__cdecl *InviteUserToLobbyFn)(Iface, SteamId lobby, SteamId invitee);

// An identity for a SteamID, filled the way SetSteamID64 does.
inline Identity IdentityOf(SteamId id)
{
	Identity identity = {};
	identity.type = kIdentitySteamId;
	identity.size = 8;
	for (int i = 0; i < 8; ++i)
		identity.data[i] = static_cast<uint8_t>(id >> (8 * i));
	return identity;
}

inline SteamId SteamIdOf(const Identity &identity)
{
	SteamId id = 0;
	for (int i = 0; i < 8; ++i)
		id |= static_cast<SteamId>(identity.data[i]) << (8 * i);
	return identity.type == kIdentitySteamId ? id : 0;
}

inline SteamId LobbyOf(const FriendGameInfo &info)
{
	SteamId id = 0;
	for (int i = 0; i < 8; ++i)
		id |= static_cast<SteamId>(info.lobby[i]) << (8 * i);
	return id;
}

}  // namespace steamflat
