// Check of our own Steamworks declarations (src/mods/steam_api_flat.h) against
// Valve's SDK headers: every size, offset, callback number and constant the
// Steam spike relies on. The SDK is never in the repo (ref\ is gitignored), so
// build.ps1 -Test builds this only when ref\steamworks_sdk_* is there, and
// skips it otherwise (as on GitHub's build machines).
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "steam/steam_api.h"
#include "steam/steam_api_flat.h"
#include "steam/isteamnetworkingmessages.h"
#include "steam/isteamnetworkingutils.h"

#include "../../mods/steam_api_flat.h"

namespace f = steamflat;

#define SAME_SIZE(ours, sdk) static_assert(sizeof(f::ours) == sizeof(sdk), #ours " size")
#define SAME_AT(ours, ofield, sdk, sfield) \
	static_assert(offsetof(f::ours, ofield) == offsetof(sdk, sfield), #ours "." #ofield " offset")
#define SAME_ID(value, sdk) static_assert(f::value == sdk::k_iCallback, #value " callback number")
#define SAME_VALUE(value, sdk) static_assert(static_cast<long long>(f::value) == static_cast<long long>(sdk), #value)

SAME_SIZE(Identity, SteamNetworkingIdentity);
SAME_AT(Identity, data, SteamNetworkingIdentity, m_steamID64);
SAME_SIZE(Message, SteamNetworkingMessage_t);
SAME_AT(Message, data, SteamNetworkingMessage_t, m_pData);
SAME_AT(Message, size, SteamNetworkingMessage_t, m_cbSize);
SAME_AT(Message, peer, SteamNetworkingMessage_t, m_identityPeer);
SAME_AT(Message, release, SteamNetworkingMessage_t, m_pfnRelease);
SAME_AT(Message, channel, SteamNetworkingMessage_t, m_nChannel);
SAME_AT(Message, lane, SteamNetworkingMessage_t, m_idxLane);
SAME_SIZE(ConnectionInfo, SteamNetConnectionInfo_t);
SAME_AT(ConnectionInfo, remote_address, SteamNetConnectionInfo_t, m_addrRemote);
SAME_AT(ConnectionInfo, pop_relay, SteamNetConnectionInfo_t, m_idPOPRelay);
SAME_AT(ConnectionInfo, state, SteamNetConnectionInfo_t, m_eState);
SAME_AT(ConnectionInfo, end_reason, SteamNetConnectionInfo_t, m_eEndReason);
SAME_AT(ConnectionInfo, end_debug, SteamNetConnectionInfo_t, m_szEndDebug);
SAME_AT(ConnectionInfo, description, SteamNetConnectionInfo_t, m_szConnectionDescription);
SAME_AT(ConnectionInfo, flags, SteamNetConnectionInfo_t, m_nFlags);
SAME_SIZE(RealTimeStatus, SteamNetConnectionRealTimeStatus_t);
SAME_AT(RealTimeStatus, ping_ms, SteamNetConnectionRealTimeStatus_t, m_nPing);
SAME_AT(RealTimeStatus, queue_time_us, SteamNetConnectionRealTimeStatus_t, m_usecQueueTime);
SAME_SIZE(FriendGameInfo, FriendGameInfo_t);
SAME_AT(FriendGameInfo, lobby, FriendGameInfo_t, m_steamIDLobby);
SAME_SIZE(CallbackMsg, CallbackMsg_t);
SAME_AT(CallbackMsg, id, CallbackMsg_t, m_iCallback);
SAME_AT(CallbackMsg, param, CallbackMsg_t, m_pubParam);
SAME_AT(CallbackMsg, size, CallbackMsg_t, m_cubParam);

SAME_ID(kCallApiCallCompleted, SteamAPICallCompleted_t);
SAME_SIZE(ApiCallCompleted, SteamAPICallCompleted_t);
SAME_AT(ApiCallCompleted, size, SteamAPICallCompleted_t, m_cubParam);
SAME_ID(kCallLobbyEnter, LobbyEnter_t);
SAME_SIZE(LobbyEnter, LobbyEnter_t);
SAME_AT(LobbyEnter, response, LobbyEnter_t, m_EChatRoomEnterResponse);
SAME_ID(kCallLobbyDataUpdate, LobbyDataUpdate_t);
SAME_SIZE(LobbyDataUpdate, LobbyDataUpdate_t);
SAME_AT(LobbyDataUpdate, success, LobbyDataUpdate_t, m_bSuccess);
SAME_ID(kCallLobbyChatUpdate, LobbyChatUpdate_t);
SAME_SIZE(LobbyChatUpdate, LobbyChatUpdate_t);
SAME_AT(LobbyChatUpdate, state_change, LobbyChatUpdate_t, m_rgfChatMemberStateChange);
SAME_ID(kCallLobbyMatchList, LobbyMatchList_t);
SAME_SIZE(LobbyMatchList, LobbyMatchList_t);
SAME_ID(kCallLobbyCreated, LobbyCreated_t);
SAME_SIZE(LobbyCreated, LobbyCreated_t);
SAME_AT(LobbyCreated, lobby, LobbyCreated_t, m_ulSteamIDLobby);
SAME_ID(kCallGameOverlayActivated, GameOverlayActivated_t);
SAME_SIZE(GameOverlayActivated, GameOverlayActivated_t);
SAME_ID(kCallRelayNetworkStatus, SteamRelayNetworkStatus_t);
SAME_SIZE(RelayNetworkStatus, SteamRelayNetworkStatus_t);
SAME_AT(RelayNetworkStatus, avail_any_relay, SteamRelayNetworkStatus_t, m_eAvailAnyRelay);
SAME_AT(RelayNetworkStatus, debug_msg, SteamRelayNetworkStatus_t, m_debugMsg);
SAME_ID(kCallSessionRequest, SteamNetworkingMessagesSessionRequest_t);
SAME_SIZE(SessionRequest, SteamNetworkingMessagesSessionRequest_t);
SAME_ID(kCallSessionFailed, SteamNetworkingMessagesSessionFailed_t);
SAME_SIZE(SessionFailed, SteamNetworkingMessagesSessionFailed_t);

SAME_VALUE(kInitOk, k_ESteamAPIInitResult_OK);
SAME_VALUE(kInitNoSteamClient, k_ESteamAPIInitResult_NoSteamClient);
SAME_VALUE(kInitVersionMismatch, k_ESteamAPIInitResult_VersionMismatch);
SAME_VALUE(kErrMsgSize, k_cchMaxSteamErrMsg);
SAME_VALUE(kResultOk, k_EResultOK);
SAME_VALUE(kLobbyFriendsOnly, k_ELobbyTypeFriendsOnly);
SAME_VALUE(kLobbyPublic, k_ELobbyTypePublic);
SAME_VALUE(kLobbyInvisible, k_ELobbyTypeInvisible);
SAME_VALUE(kLobbyCompareEqual, k_ELobbyComparisonEqual);
SAME_VALUE(kLobbyDistanceWorldwide, k_ELobbyDistanceFilterWorldwide);
SAME_VALUE(kFriendFlagImmediate, k_EFriendFlagImmediate);
SAME_VALUE(kFriendRelationshipFriend, k_EFriendRelationshipFriend);
SAME_VALUE(kAvailCannotTry, k_ESteamNetworkingAvailability_CannotTry);
SAME_VALUE(kAvailFailed, k_ESteamNetworkingAvailability_Failed);
SAME_VALUE(kAvailWaiting, k_ESteamNetworkingAvailability_Waiting);
SAME_VALUE(kAvailAttempting, k_ESteamNetworkingAvailability_Attempting);
SAME_VALUE(kAvailCurrent, k_ESteamNetworkingAvailability_Current);
SAME_VALUE(kConnConnecting, k_ESteamNetworkingConnectionState_Connecting);
SAME_VALUE(kConnFindingRoute, k_ESteamNetworkingConnectionState_FindingRoute);
SAME_VALUE(kConnConnected, k_ESteamNetworkingConnectionState_Connected);
SAME_VALUE(kConnClosedByPeer, k_ESteamNetworkingConnectionState_ClosedByPeer);
SAME_VALUE(kConnProblemDetectedLocally, k_ESteamNetworkingConnectionState_ProblemDetectedLocally);
SAME_VALUE(kSendUnreliableNoNagle, k_nSteamNetworkingSend_UnreliableNoNagle);
SAME_VALUE(kSendReliable, k_nSteamNetworkingSend_Reliable);
SAME_VALUE(kSendAutoRestartBrokenSession, k_nSteamNetworkingSend_AutoRestartBrokenSession);
SAME_VALUE(kConnectionFlagRelayed, k_nSteamNetworkConnectionInfoFlags_Relayed);
SAME_VALUE(kIdentitySteamId, k_ESteamNetworkingIdentityType_SteamID);

int main()
{
	// Our identity bytes are the SDK's own SetSteamID64 (an inline of its header).
	const unsigned long long id = 76561197960287930ull;
	SteamNetworkingIdentity sdk;
	sdk.Clear();
	sdk.SetSteamID64(id);
	const f::Identity ours = f::IdentityOf(id);
	const bool same = memcmp(&sdk, &ours, sizeof(ours)) == 0;
	printf("%s: our SteamID identity has the SDK's bytes\n", same ? "ok  " : "FAIL");
	printf("%s\n", same ? "Steam declarations match the SDK" : "SOME CHECKS FAILED");
	return same ? 0 : 1;
}
