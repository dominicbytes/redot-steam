// Offline regression tests for the production admission and receive methods.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define MAX_MESSAGE_COUNT 255
#define unlikely(x) (x)
#define WARN_PRINT(...) ((void)0)
#define ERR_PRINT(...) ((void)0)
#define ERR_FAIL_COND_MSG(condition, ...) do { if (condition) { return; } } while (false)

using uint64 = uint64_t;
constexpr int k_ESteamNetworkingConnectionState_None = 0;
constexpr int k_ESteamNetworkingConnectionState_Connecting = 1;
constexpr int k_ESteamNetworkingConnectionState_FindingRoute = 2;
constexpr int k_ESteamNetworkingConnectionState_Connected = 3;
constexpr int k_ESteamNetworkingConnectionState_ClosedByPeer = 4;
constexpr int k_ESteamNetworkingConnectionState_ProblemDetectedLocally = 5;
constexpr int k_ESteamNetConnectionEnd_Remote_BadCert = 1;
constexpr int k_HSteamListenSocket_Invalid = 0;
constexpr int k_EResultOK = 0;
struct SteamNetConnectionStatusChangedCallback_t {
	uint32_t m_hConn;
	int m_eOldState;
	struct {
		int m_eState;
		int64_t m_nUserData;
		uint64_t m_identityRemote;
		int m_hListenSocket;
		int m_eEndReason;
	} m_info;
};

enum Error { OK, ERR_INVALID_DATA };
using HSteamNetConnection = uint32_t;
template <typename T> using Ref = std::shared_ptr<T>;
template <typename K, typename V> struct KeyValue { K key; V value; };
template <typename K, typename V> struct HashMap {
	std::vector<KeyValue<K, V>> entries;
	bool has(K key) const {
		return std::any_of(entries.begin(), entries.end(), [key](auto &e) { return e.key == key; });
	}
	V &operator[](K key) {
		for (auto &e : entries) { if (e.key == key) { return e.value; } }
		entries.push_back({key, {}});
		return entries.back().value;
	}
	void erase(K key) {
		entries.erase(std::remove_if(entries.begin(), entries.end(), [key](auto &e) { return e.key == key; }), entries.end());
	}
};
struct SteamNetworkingMessage_t {
	HSteamNetConnection m_conn;
	uint32_t claimed_id;
	int releases = 0;
};
struct MessageList {
	std::vector<SteamNetworkingMessage_t *> entries;
	void push_back(SteamNetworkingMessage_t *message) { entries.push_back(message); }
	bool is_empty() const { return entries.empty(); }
	int size() const { return static_cast<int>(entries.size()); }
};
static HashMap<HSteamNetConnection, int64_t> user_data;
static std::vector<SteamNetworkingMessage_t *> receive_batch;
static int requested_messages = 0;
static void *SteamAPI_SteamNetworkingSockets_SteamAPI() { return nullptr; }
static uint64_t SteamAPI_SteamNetworkingIdentity_GetSteamID64(uint64_t *id) { return *id; }
static void SteamAPI_ISteamNetworkingSockets_CloseConnection(void *, HSteamNetConnection, int, const char *, bool) {}
static int SteamAPI_ISteamNetworkingSockets_AcceptConnection(void *, HSteamNetConnection) { return k_EResultOK; }
static bool SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup(void *, HSteamNetConnection, int) { return true; }
static int SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup(void *, int, SteamNetworkingMessage_t **messages, int count) {
	requested_messages = count;
	int received = std::min(count, static_cast<int>(receive_batch.size()));
	std::copy_n(receive_batch.begin(), received, messages);
	receive_batch.erase(receive_batch.begin(), receive_batch.begin() + received);
	return received;
}
static void SteamAPI_SteamNetworkingMessage_t_Release(SteamNetworkingMessage_t *message) { assert(++message->releases == 1); }

struct SteamPacketPeer {
	enum PeerState {
		STATE_NONE = k_ESteamNetworkingConnectionState_None,
		STATE_CONNECTING = k_ESteamNetworkingConnectionState_Connecting,
		STATE_CONNECTED = k_ESteamNetworkingConnectionState_Connected
	};
	uint32_t peer_id = 0;
	uint64_t steam_id = 0;
	HSteamNetConnection connection_handle = 0;
	PeerState state = STATE_CONNECTED;
	bool closed = false;
	void set_peer_id(uint32_t id) { peer_id = id; user_data[connection_handle] = id; }
	uint32_t get_peer_id() const { return peer_id; }
	uint64_t get_steam_id() const { return steam_id; }
	HSteamNetConnection get_connection_handle() const { return connection_handle; }
	PeerState get_state() const { return state; }
	void set_state(PeerState value) { state = value; }
	Error ping(uint32_t) { return OK; }
	Error process_ping(SteamNetworkingMessage_t *message) { set_peer_id(message->claimed_id); return OK; }
	void disconnect_peer(bool force) { assert(force); closed = true; state = STATE_NONE; }
};
struct SteamMultiplayerPeer {
	enum { CONNECTION_DISCONNECTED, CONNECTION_CONNECTING, CONNECTION_CONNECTED, DEBUG_LEVEL_NONE };
	HashMap<uint32_t, Ref<SteamPacketPeer>> peers;
	HashMap<HSteamNetConnection, Ref<SteamPacketPeer>> steam_connections;
	MessageList incoming_packets;
	int poll_group = 1;
	int debug_level = DEBUG_LEVEL_NONE;
	int listen_socket = k_HSteamListenSocket_Invalid;
	int connection_retries = 0;
	int unique_id = 2;
	bool server = false;
	uint64_t expected_host_steam_id = 100;
	int connection_status = CONNECTION_CONNECTING;
	std::vector<uint32_t> connected;
	void emit_signal(const char *name, uint32_t id) { assert(std::string(name) == "peer_connected"); connected.push_back(id); }
	std::vector<uint32_t> disconnected;
	void _disconnect_peer(uint32_t id, bool) { disconnected.push_back(id); peers.erase(id); }
	bool is_refusing_new_connections() const { return false; }
	void add_peer(uint64_t) {}
	void _add_pending_peer(uint64_t, HSteamNetConnection, SteamPacketPeer::PeerState) {}
	int _get_unique_id() const { return unique_id; }
	void network_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *);
	void _poll();
	void _upgrade_peer(HSteamNetConnection);
};

// PRODUCTION_METHODS

static Ref<SteamPacketPeer> pending(SteamMultiplayerPeer &network, HSteamNetConnection conn, uint64_t steam_id = 200) {
	auto peer = std::make_shared<SteamPacketPeer>();
	peer->connection_handle = conn;
	peer->steam_id = steam_id;
	network.steam_connections[conn] = peer;
	user_data[conn] = 0;
	return peer;
}
static void rejected_claim(uint32_t id, bool server = false, uint64_t steam_id = 200) {
	SteamMultiplayerPeer network;
	network.server = server;
	network.unique_id = server ? 1 : 2;
	auto peer = pending(network, 20, steam_id);
	SteamNetworkingMessage_t handshake{20, id};
	SteamNetworkingMessage_t payload{20, 99};
	receive_batch = {&handshake, &payload};
	network._poll();
	assert(peer->closed);
	assert(!network.steam_connections.has(20));
	assert(network.connected.empty());
	assert(network.incoming_packets.is_empty());
	assert(handshake.releases == 1 && payload.releases == 1);
	assert(network.connection_status == SteamMultiplayerPeer::CONNECTION_CONNECTING);
}
int main() {
	for (uint32_t id : {0u, 0x80000000u, 0xffffffffu, 2u, 1u}) { rejected_claim(id); }
	rejected_claim(1, true, 100);
	// A duplicate claim cannot replace the existing peer or deliver later data.
	SteamMultiplayerPeer duplicate;
	auto owner = pending(duplicate, 10);
	owner->set_peer_id(3);
	duplicate._upgrade_peer(10);
	auto attacker = pending(duplicate, 20, 300);
	SteamNetworkingMessage_t claim{20, 3}, payload{20, 99};
	receive_batch = {&claim, &payload};
	duplicate._poll();
	assert(duplicate.peers[3] == owner && attacker->closed);
	assert(duplicate.connected.size() == 1 && duplicate.incoming_packets.is_empty());
	assert(claim.releases == 1 && payload.releases == 1);
	// Steam may deliver a stale close callback with the attacker's claimed ID.
	SteamNetConnectionStatusChangedCallback_t closed{20, k_ESteamNetworkingConnectionState_Connected,
		{k_ESteamNetworkingConnectionState_ClosedByPeer, 3, 300, 0, 0}};
	duplicate.network_connection_status_changed(&closed);
	assert(duplicate.peers[3] == owner && duplicate.disconnected.empty());
	closed.m_hConn = 10;
	duplicate.network_connection_status_changed(&closed);
	assert(!duplicate.peers.has(3) && duplicate.disconnected.size() == 1);
	// Only the host selected by create_client may claim authority; mesh stays open.
	for (uint32_t id : {1u, 3u, 0x7fffffffu}) {
		SteamMultiplayerPeer network;
		auto peer = pending(network, 30, id == 1 ? 100 : 200);
		SteamNetworkingMessage_t handshake{30, id}, data{30, 55};
		receive_batch = {&handshake, &data};
		network._poll();
		assert(!peer->closed && network.peers[id] == peer);
		assert(network.connected.size() == 1 && network.connected[0] == id);
		assert(network.connection_status == SteamMultiplayerPeer::CONNECTION_CONNECTED);
		assert(network.incoming_packets.size() == 1 && data.releases == 0 && handshake.releases == 1);
		SteamAPI_SteamNetworkingMessage_t_Release(&data);
	}
	// A known ID alone is not admission while waiting for the connected callback.
	SteamMultiplayerPeer early;
	auto peer = pending(early, 40, 100);
	peer->state = SteamPacketPeer::STATE_CONNECTING;
	SteamNetworkingMessage_t handshake{40, 1}, data{40, 88};
	receive_batch = {&handshake, &data};
	early._poll();
	assert(early.peers.entries.empty() && early.connected.empty() && early.incoming_packets.is_empty());
	peer->state = SteamPacketPeer::STATE_CONNECTED;
	early._upgrade_peer(40);
	assert(early.peers[1] == peer && early.connected.size() == 1);
	assert(handshake.releases == 1 && data.releases == 1);
	std::cout << "NATIVE_MULTIPLAYER_TESTS=PASS\n";
}
