// Check of our own Steamworks declarations (src/mods/steam_api_flat.h) against
// Valve's SDK headers: every size, offset, callback number and constant the
// Steam spike and the virtual LAN's Steam transport rely on, and the shape of
// every flat function we call (its return value and each argument: same size,
// same kind, as the 32-bit __cdecl call passes them). The SDK is never in the
// repo (ref\ is gitignored), so build.ps1 -Test builds this only when
// ref\steamworks_sdk_* is there, and skips it otherwise (as on GitHub's build
// machines).
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <type_traits>

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
// added for the virtual LAN's Steam transport
SAME_AT(Message, time_received_us, SteamNetworkingMessage_t, m_usecTimeReceived);
SAME_AT(LobbyChatUpdate, user_changed, LobbyChatUpdate_t, m_ulSteamIDUserChanged);
SAME_AT(LobbyDataUpdate, member, LobbyDataUpdate_t, m_ulSteamIDMember);
SAME_AT(LobbyEnter, lobby, LobbyEnter_t, m_ulSteamIDLobby);
SAME_VALUE(kLobbyPrivate, k_ELobbyTypePrivate);
SAME_VALUE(kMemberEntered, k_EChatMemberStateChangeEntered);
SAME_VALUE(kMemberLeft, k_EChatMemberStateChangeLeft);
SAME_VALUE(kMemberDisconnected, k_EChatMemberStateChangeDisconnected);
SAME_VALUE(kMemberKicked, k_EChatMemberStateChangeKicked);
SAME_VALUE(kMemberBanned, k_EChatMemberStateChangeBanned);
SAME_VALUE(kEnterSuccess, k_EChatRoomEnterResponseSuccess);
static_assert(sizeof(f::Microseconds) == sizeof(SteamNetworkingMicroseconds), "Microseconds size");
// added for step 4, the online UI
SAME_ID(kCallGameLobbyJoinRequested, GameLobbyJoinRequested_t);
SAME_SIZE(GameLobbyJoinRequested, GameLobbyJoinRequested_t);
SAME_AT(GameLobbyJoinRequested, lobby, GameLobbyJoinRequested_t, m_steamIDLobby);
SAME_AT(GameLobbyJoinRequested, friend_id, GameLobbyJoinRequested_t, m_steamIDFriend);
SAME_AT(GameOverlayActivated, active, GameOverlayActivated_t, m_bActive);
SAME_AT(GameOverlayActivated, app_id, GameOverlayActivated_t, m_nAppID);
SAME_VALUE(kPersonaOffline, k_EPersonaStateOffline);
SAME_VALUE(kPersonaOnline, k_EPersonaStateOnline);
SAME_VALUE(kPersonaBusy, k_EPersonaStateBusy);
SAME_VALUE(kPersonaAway, k_EPersonaStateAway);
SAME_VALUE(kPersonaSnooze, k_EPersonaStateSnooze);

// The shape of a function: what a 32-bit __cdecl call puts on the stack and gets back.
template <typename A, typename B>
constexpr bool SameSlot()
{
	return sizeof(A) == sizeof(B) && std::is_reference<A>::value == std::is_reference<B>::value &&
	       std::is_floating_point<A>::value == std::is_floating_point<B>::value;
}
template <typename A, typename B>
constexpr bool SameReturn()
{
	if constexpr (std::is_void<A>::value || std::is_void<B>::value)
		return std::is_void<A>::value == std::is_void<B>::value;
	else
		return SameSlot<A, B>();
}
template <typename R1, typename... A1, typename R2, typename... A2>
constexpr bool SameShape(R1(__cdecl *)(A1...), R2(__cdecl *)(A2...))
{
	if constexpr (sizeof...(A1) != sizeof...(A2))
		return false;
	else
		return SameReturn<R1, R2>() && (true && ... && SameSlot<A1, A2>());
}
#define SAME_FN(ours, sdk) \
	static_assert(SameShape(static_cast<f::ours>(nullptr), static_cast<decltype(&sdk)>(nullptr)), #ours " vs " #sdk)

SAME_FN(InitFlatFn, SteamAPI_InitFlat);
SAME_FN(GetPipeFn, SteamAPI_GetHSteamPipe);
SAME_FN(VoidFn, SteamAPI_ManualDispatch_Init);
SAME_FN(RunFrameFn, SteamAPI_ManualDispatch_RunFrame);
SAME_FN(GetNextCallbackFn, SteamAPI_ManualDispatch_GetNextCallback);
SAME_FN(RunFrameFn, SteamAPI_ManualDispatch_FreeLastCallback);
SAME_FN(GetApiCallResultFn, SteamAPI_ManualDispatch_GetAPICallResult);
SAME_FN(AccessorFn, SteamAPI_SteamUser_v023);
SAME_FN(AccessorFn, SteamAPI_SteamFriends_v018);
SAME_FN(AccessorFn, SteamAPI_SteamUtils_v011);
SAME_FN(AccessorFn, SteamAPI_SteamMatchmaking_v009);
SAME_FN(AccessorFn, SteamAPI_SteamNetworkingMessages_SteamAPI_v002);
SAME_FN(AccessorFn, SteamAPI_SteamNetworkingUtils_SteamAPI_v004);
SAME_FN(GetSteamIdFn, SteamAPI_ISteamUser_GetSteamID);
SAME_FN(GetAppIdFn, SteamAPI_ISteamUtils_GetAppID);
SAME_FN(IfaceBoolFn, SteamAPI_ISteamUtils_IsOverlayEnabled);
SAME_FN(GetPersonaNameFn, SteamAPI_ISteamFriends_GetPersonaName);
SAME_FN(GetFriendCountFn, SteamAPI_ISteamFriends_GetFriendCount);
SAME_FN(GetFriendByIndexFn, SteamAPI_ISteamFriends_GetFriendByIndex);
SAME_FN(GetFriendRelationshipFn, SteamAPI_ISteamFriends_GetFriendRelationship);
SAME_FN(GetFriendPersonaNameFn, SteamAPI_ISteamFriends_GetFriendPersonaName);
SAME_FN(GetFriendGamePlayedFn, SteamAPI_ISteamFriends_GetFriendGamePlayed);
SAME_FN(CreateLobbyFn, SteamAPI_ISteamMatchmaking_CreateLobby);
SAME_FN(JoinLobbyFn, SteamAPI_ISteamMatchmaking_JoinLobby);
SAME_FN(LeaveLobbyFn, SteamAPI_ISteamMatchmaking_LeaveLobby);
SAME_FN(SetLobbyDataFn, SteamAPI_ISteamMatchmaking_SetLobbyData);
SAME_FN(GetLobbyDataFn, SteamAPI_ISteamMatchmaking_GetLobbyData);
SAME_FN(GetNumLobbyMembersFn, SteamAPI_ISteamMatchmaking_GetNumLobbyMembers);
SAME_FN(GetLobbyOwnerFn, SteamAPI_ISteamMatchmaking_GetLobbyOwner);
SAME_FN(GetLobbyMemberByIndexFn, SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex);
SAME_FN(GetLobbyMemberDataFn, SteamAPI_ISteamMatchmaking_GetLobbyMemberData);
SAME_FN(SetLobbyMemberDataFn, SteamAPI_ISteamMatchmaking_SetLobbyMemberData);
SAME_FN(RequestLobbyDataFn, SteamAPI_ISteamMatchmaking_RequestLobbyData);
SAME_FN(RequestLobbyListFn, SteamAPI_ISteamMatchmaking_RequestLobbyList);
SAME_FN(AddStringFilterFn, SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter);
SAME_FN(AddDistanceFilterFn, SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter);
SAME_FN(GetLobbyByIndexFn, SteamAPI_ISteamMatchmaking_GetLobbyByIndex);
SAME_FN(SendMessageToUserFn, SteamAPI_ISteamNetworkingMessages_SendMessageToUser);
SAME_FN(ReceiveMessagesFn, SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel);
SAME_FN(SessionFn, SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser);
SAME_FN(GetSessionInfoFn, SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo);
SAME_FN(InitRelayFn, SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess);
SAME_FN(GetRelayStatusFn, SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus);
SAME_FN(GetLocalTimestampFn, SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp);
SAME_FN(ReleaseMessageFn, SteamAPI_SteamNetworkingMessage_t_Release);
SAME_FN(SetRichPresenceFn, SteamAPI_ISteamFriends_SetRichPresence);
SAME_FN(GetFriendRichPresenceFn, SteamAPI_ISteamFriends_GetFriendRichPresence);
SAME_FN(RequestFriendRichPresenceFn, SteamAPI_ISteamFriends_RequestFriendRichPresence);
SAME_FN(GetFriendPersonaStateFn, SteamAPI_ISteamFriends_GetFriendPersonaState);
SAME_FN(ActivateInviteDialogFn, SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog);
SAME_FN(InviteUserToLobbyFn, SteamAPI_ISteamMatchmaking_InviteUserToLobby);

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
