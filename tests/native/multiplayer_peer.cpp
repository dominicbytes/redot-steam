// Offline regression tests for the production admission and receive methods.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <functional>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#define memnew(T) std::make_shared<T>()
#define ERR_FAIL_COND_V(condition, error) do { if (condition) { return error; } } while (false)
#define MAX_MESSAGE_COUNT 255
#define unlikely(x) (x)
#define ERR_FAIL_COND_V_MSG(condition, error, ...) do { if (condition) { return error; } } while (false)
template <typename... T> static void discard_log(const T &...) {}
template <typename... T> static int vformat(const char *, const T &...) { return 0; }
#define WARN_PRINT(...) discard_log(__VA_ARGS__)
#define ERR_PRINT(...) ((void)0)
#define ERR_FAIL_COND_MSG(condition, ...) do { if (condition) { return; } } while (false)

using uint64 = uint64_t;
using SteamNetworkingIdentity = uint64_t;
struct SteamNetworkingConfigValue_t {};
constexpr int k_ESteamNetworkingConfig_SymmetricConnect = 0;
constexpr int k_HSteamNetConnection_Invalid = 0;
constexpr int k_ESteamNetworkingConnectionState_None = 0;
constexpr int k_ESteamNetworkingConnectionState_Connecting = 1;
constexpr int k_ESteamNetworkingConnectionState_FindingRoute = 2;
constexpr int k_ESteamNetworkingConnectionState_Connected = 3;
constexpr int k_ESteamNetworkingConnectionState_ClosedByPeer = 4;
constexpr int k_ESteamNetworkingConnectionState_ProblemDetectedLocally = 5;
constexpr int k_ESteamNetConnectionEnd_Remote_BadCert = 1;
constexpr int k_HSteamListenSocket_Invalid = 0;
constexpr int k_EResultOK = 1;
struct SteamNetConnectionStatusChangedCallback_t {
	uint32_t m_hConn;
	int m_eOldState;
	struct {
		int m_eState;
		int64_t m_nUserData;
		uint64_t m_identityRemote;
		int m_hListenSocket;
		int m_eEndReason;
		const char *m_szEndDebug = "";
	} m_info;
};

enum Error { OK, ERR_INVALID_DATA, ERR_UNAVAILABLE, ERR_ALREADY_IN_USE, ERR_CANT_CREATE, ERR_UNCONFIGURED, ERR_INVALID_PARAMETER, ERR_CONNECTION_ERROR };
struct Math { static int abs(int x) { return std::abs(x); } };
constexpr int k_HSteamNetPollGroup_Invalid = 0;
using HSteamNetConnection = uint32_t;
template <typename T> using Ref = std::shared_ptr<T>;
template <typename K, typename V> struct KeyValue { K key; V value; };
template <typename K, typename V> struct HashMap {
	std::vector<KeyValue<K, V>> entries;
	auto begin() { return entries.begin(); }
	auto end() { return entries.end(); }
	void clear() { entries.clear(); }
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
	bool malformed = false;
	void *GetData() { return &claimed_id; }
	int GetSize() const { return sizeof(claimed_id); }
};
struct MessageList {
	std::vector<SteamNetworkingMessage_t *> entries;
	struct Element { SteamNetworkingMessage_t *value; SteamNetworkingMessage_t *get() { return value; } } front_element;
	Element *front() { front_element.value = entries.front(); return &front_element; }
	void pop_front() { entries.erase(entries.begin()); }
	void clear() { entries.clear(); }
	void push_back(SteamNetworkingMessage_t *message) { entries.push_back(message); }
	bool is_empty() const { return entries.empty(); }
	int size() const { return static_cast<int>(entries.size()); }
};
static HashMap<HSteamNetConnection, int64_t> user_data;
static std::vector<SteamNetworkingMessage_t *> receive_batch;
static int requested_messages = 0;
static int receive_calls = 0;
static int64_t configured_queue_limit = 256;
static int64_t configured_pending_limit = 32, configured_handshake_timeout = 15000;
static uint64_t now_msec = 0;
template <typename T> using LocalVector = std::vector<T>;
struct Time { static Time *get_singleton() { static Time t; return &t; } uint64_t get_ticks_msec() { return now_msec; } };
struct ProjectSettings {
	static ProjectSettings *get_singleton() { static ProjectSettings instance; return &instance; }
	int64_t get_setting_with_override(const char *name) {
		std::string key = name;
		if (key == "steam/multiplayer_peer/max_pending_connections") { return configured_pending_limit; }
		if (key == "steam/multiplayer_peer/handshake_timeout_ms") { return configured_handshake_timeout; }
		return configured_queue_limit;
	}
};
struct SteamProjectSettings {
	static int get_max_queued_packets();
	static int get_max_pending_connections();
	static int get_handshake_timeout_ms();
};
static bool steam_up = true;
static int sockets_token = 0;
static int listen_closes = 0;
static int poll_destroys = 0;
static void SteamAPI_ISteamNetworkingSockets_CloseListenSocket(void *, int) { listen_closes++; }
static void SteamAPI_ISteamNetworkingSockets_DestroyPollGroup(void *, int) { poll_destroys++; }
static void *SteamAPI_SteamNetworkingSockets_SteamAPI() { return steam_up ? &sockets_token : nullptr; }
static uint64_t SteamAPI_SteamNetworkingIdentity_GetSteamID64(uint64_t *id) { return *id; }
static HashMap<HSteamNetConnection, int> closes;
static HashMap<HSteamNetConnection, int> groups;
static int last_virtual_port = -1;
static HSteamNetConnection next_connection = 1000;
static bool fail_connect = false;
static bool fail_listen = false, fail_poll = false;
static void SteamAPI_ISteamNetworkingSockets_CloseConnection(void *, HSteamNetConnection conn, int, const char *, bool) { closes[conn]++; }
static void SteamAPI_SteamNetworkingIdentity_SetSteamID64(uint64_t *id, uint64_t value) { *id = value; }
static void SteamAPI_SteamNetworkingConfigValue_t_SetInt32(SteamNetworkingConfigValue_t *, int, int) {}
static HSteamNetConnection SteamAPI_ISteamNetworkingSockets_ConnectP2P(void *, uint64_t, int port, int, SteamNetworkingConfigValue_t *) {
	last_virtual_port = port;
	return fail_connect ? 0 : next_connection++;
}
static int SteamAPI_ISteamNetworkingSockets_AcceptConnection(void *, HSteamNetConnection) { return k_EResultOK; }
static bool SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup(void *, HSteamNetConnection conn, int group) { groups[conn] = group; return group != 0; }
static int SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup(void *, int, SteamNetworkingMessage_t **messages, int count) {
	requested_messages = count;
	receive_calls++;
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
	Error send_result = OK;
	int send_calls = 0;
	uint32_t peer_id = 0;
	uint64_t steam_id = 0;
	HSteamNetConnection connection_handle = 0;
	PeerState state = STATE_CONNECTED;
	bool closed = false;
	void set_steam_id(uint64_t value) { steam_id = value; }
	void set_connection_handle(HSteamNetConnection value) { connection_handle = value; }
	Error send(int, const uint8_t *, int, int) { send_calls++; return send_result; }
	void set_peer_id(uint32_t id) { peer_id = id; user_data[connection_handle] = id; }
	uint32_t get_peer_id() const { return peer_id; }
	uint64_t get_steam_id() const { return steam_id; }
	HSteamNetConnection get_connection_handle() const { return connection_handle; }
	PeerState get_state() const { return state; }
	void set_state(PeerState value) { state = value; }
	Error ping(uint32_t) { return OK; }
	Error process_ping(SteamNetworkingMessage_t *message) { if (message->malformed) { return ERR_INVALID_DATA; } set_peer_id(message->claimed_id); return OK; }
	void disconnect_peer(bool) { closed = true; state = STATE_NONE; SteamAPI_ISteamNetworkingSockets_CloseConnection(nullptr, connection_handle, 0, nullptr, false); }
};
struct SteamMultiplayerPeer {
	enum { CONNECTION_DISCONNECTED, CONNECTION_CONNECTING, CONNECTION_CONNECTED, DEBUG_LEVEL_NONE };
	HashMap<uint32_t, Ref<SteamPacketPeer>> peers;
	HashMap<HSteamNetConnection, Ref<SteamPacketPeer>> steam_connections;
	HashMap<HSteamNetConnection, uint64_t> pending_deadlines;
	void _expire_pending_connections();
	int _count_pending_connections();
	MessageList incoming_packets;
	SteamNetworkingMessage_t *current_packet = nullptr;
	~SteamMultiplayerPeer();
	void _close();
	Error _get_packet(const uint8_t **, int *);
	int _get_available_packet_count() const;
	int poll_group = 1;
	int debug_level = DEBUG_LEVEL_NONE;
	int listen_socket = k_HSteamListenSocket_Invalid;
	int connection_retries = 0;
	HSteamNetConnection host_connection = k_HSteamNetConnection_Invalid;
	int host_virtual_port = 0;
	int target_peer = 0;
	int _get_transfer_channel() { return 0; }
	int _get_steam_packet_flags() { return 0; }
	Error _put_packet(const uint8_t *, int);
Error _create_listen_socket(int) { if (fail_listen) { return ERR_CANT_CREATE; } listen_socket = 10; return OK; }
Error _create_poll_group() { if (fail_poll) { return ERR_CANT_CREATE; } poll_group = 1; return OK; }
Error create_host(int = 0);
	int generate_unique_id() { return 2; }
	Error create_client(uint64_t, int = 0);
	int unique_id = 2;
	bool server = false;
	uint64_t expected_host_steam_id = 100;
	int connection_status = CONNECTION_CONNECTING;
	std::vector<uint32_t> connected;
	std::function<void(uint32_t)> on_disconnect;
	std::function<void(uint32_t)> on_connect;
	void emit_signal(const char *name, uint32_t id) {
		if (std::string(name) == "peer_connected") { connected.push_back(id); if (on_connect) { on_connect(id); } }
		else { disconnected.push_back(id); if (on_disconnect) { on_disconnect(id); } }
	}
	std::vector<uint32_t> disconnected;
	void _disconnect_peer(int, bool = false);
	void _remove_peer(int, bool, bool);
	void _purge_connection_packets(HSteamNetConnection);
	bool refusing = false;
	bool is_refusing_new_connections() const { return refusing; }
	void set_refuse_new_connections(bool value) { refusing = value; }
	Error add_peer(uint64_t, int = 0);
	void _add_pending_peer(uint64_t, HSteamNetConnection, SteamPacketPeer::PeerState);
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
static void lifecycle_case(const std::string &name) {
	if (name == "C1") {
		SteamMultiplayerPeer owner, other, inactive;
		owner.listen_socket = 11; owner.poll_group = 11;
		other.listen_socket = 22; other.poll_group = 22;
		inactive.poll_group = 0;
		SteamNetConnectionStatusChangedCallback_t event{71, 0, {k_ESteamNetworkingConnectionState_Connecting, 0, 200, 11, 0}};
		owner.network_connection_status_changed(&event);
		auto peer = owner.steam_connections[71];
		other.network_connection_status_changed(&event); inactive.network_connection_status_changed(&event);
		assert(groups[71] == 11 && closes[71] == 0);
		assert(!other.steam_connections.has(71) && !inactive.steam_connections.has(71));
		assert(owner.add_peer(300, 7) == OK);
		auto conn = next_connection - 1;
		assert(owner.steam_connections.has(conn));
		auto outgoing = owner.steam_connections[conn];
		event = {conn, 0, {k_ESteamNetworkingConnectionState_Connecting, 0, 300, 0, 0}};
		owner.network_connection_status_changed(&event); other.network_connection_status_changed(&event);
		assert(owner.steam_connections[conn] == outgoing && !other.steam_connections.has(conn));
		event.m_hConn = 71; event.m_info.m_eState = k_ESteamNetworkingConnectionState_ClosedByPeer;
		other.network_connection_status_changed(&event); inactive.network_connection_status_changed(&event);
		assert(closes[71] == 0 && owner.steam_connections[71] == peer);
	} else if (name == "C2") {
		SteamMultiplayerPeer network; network.connection_status = SteamMultiplayerPeer::CONNECTION_CONNECTED;
		auto first = pending(network, 72), second = pending(network, 73);
		network.peers[3] = first; network.peers[4] = second;
		first->send_result = ERR_CONNECTION_ERROR;
		const uint8_t byte = 1;
		assert(network._put_packet(&byte, 1) == ERR_CONNECTION_ERROR);
		assert(first->send_calls == 1 && second->send_calls == 1);
		network.target_peer = -4;
		assert(network._put_packet(&byte, 1) == ERR_CONNECTION_ERROR);
		network.target_peer = 3;
		assert(network._put_packet(&byte, 1) == ERR_CONNECTION_ERROR);
	} else if (name == "C3") {
		for (int id : {1, 3}) {
			SteamMultiplayerPeer network;
			auto peer = pending(network, 74); peer->set_peer_id(id); network.peers[id] = peer;
			network.on_disconnect = [&](uint32_t gone) {
				assert(!network.peers.has(gone) && !network.steam_connections.has(74));
				if (network.disconnected.size() == 1) { network._disconnect_peer(gone, false); }
				if (id == 1) { assert(network.create_client(100, 9) == OK); }
			};
			network._disconnect_peer(id, false);
			assert(peer->closed && network.disconnected.size() == 1);
			if (id == 1) { assert(network.connection_status == SteamMultiplayerPeer::CONNECTION_CONNECTING); assert(network.steam_connections.has(next_connection - 1)); }
		}
	} else if (name == "C4") {
		SteamMultiplayerPeer network;
		auto host = pending(network, 75, 100); host->set_peer_id(1); network._upgrade_peer(75);
		SteamNetConnectionStatusChangedCallback_t event{75, k_ESteamNetworkingConnectionState_Connected,
			{k_ESteamNetworkingConnectionState_ClosedByPeer, -1, 100, 0, 0}};
		network.network_connection_status_changed(&event);
		assert(!network.peers.has(1) && network.disconnected.size() == 1 && host->closed);
		assert(network.connection_status == SteamMultiplayerPeer::CONNECTION_DISCONNECTED);
		assert(closes[75] == 1);
	} else if (name == "C5") {
		for (int state : {k_ESteamNetworkingConnectionState_Connecting, k_ESteamNetworkingConnectionState_FindingRoute, k_ESteamNetworkingConnectionState_Connected}) {
			SteamMultiplayerPeer network; network._close(); assert(network.create_client(100, 9) == OK);
			auto conn = next_connection - 1;
			SteamNetConnectionStatusChangedCallback_t event{conn, state, {k_ESteamNetworkingConnectionState_ProblemDetectedLocally, -1, 100, 0, 0}};
			network.network_connection_status_changed(&event);
			assert(network.connection_status == SteamMultiplayerPeer::CONNECTION_DISCONNECTED);
			assert(network.create_client(100, 9) == OK);
		}
		SteamMultiplayerPeer retry; retry._close(); assert(retry.create_client(100, 17) == OK);
		for (int i = 0; i < 6; i++) {
			SteamNetConnectionStatusChangedCallback_t event{next_connection - 1, k_ESteamNetworkingConnectionState_Connecting,
				{k_ESteamNetworkingConnectionState_ClosedByPeer, -1, 100, 0, k_ESteamNetConnectionEnd_Remote_BadCert}};
			retry.network_connection_status_changed(&event);
			if (i < 5) { assert(last_virtual_port == 17 && retry.connection_status == SteamMultiplayerPeer::CONNECTION_CONNECTING); }
		}
		assert(retry.connection_status == SteamMultiplayerPeer::CONNECTION_DISCONNECTED);
		SteamMultiplayerPeer mesh; mesh._close(); mesh.create_client(100, 9); mesh.add_peer(200, 8);
		SteamNetConnectionStatusChangedCallback_t event{next_connection - 1, 1, {k_ESteamNetworkingConnectionState_ClosedByPeer, -1, 200, 0, 0}};
		mesh.network_connection_status_changed(&event);
		assert(mesh.connection_status == SteamMultiplayerPeer::CONNECTION_CONNECTING);
		SteamMultiplayerPeer failed_retry; failed_retry._close(); failed_retry.create_client(100, 6);
		event = {next_connection - 1, 1, {k_ESteamNetworkingConnectionState_ClosedByPeer, -1, 100, 0, k_ESteamNetConnectionEnd_Remote_BadCert}};
		fail_connect = true; failed_retry.network_connection_status_changed(&event); fail_connect = false;
		assert(failed_retry.connection_status == SteamMultiplayerPeer::CONNECTION_DISCONNECTED);
	} else if (name == "C6") {
		SteamMultiplayerPeer network; network.listen_socket = 33; network.refusing = true;
		SteamNetConnectionStatusChangedCallback_t event{76, 0, {k_ESteamNetworkingConnectionState_Connecting, 0, 200, 33, 0}};
		network.network_connection_status_changed(&event);
		assert(closes[76] == 1 && !network.steam_connections.has(76) && network.peers.entries.empty());
		network.refusing = false; auto existing = pending(network, 77); existing->set_peer_id(3); network.peers[3] = existing;
		event.m_hConn = 78; event.m_info.m_nUserData = 3; network.network_connection_status_changed(&event);
		assert(closes[78] == 1 && !network.steam_connections.has(78) && network.peers[3] == existing);
	} else if (name == "C7") {
		// Steam shut down first: teardown drops references without touching the dead API.
		SteamMultiplayerPeer network; network.connection_status = SteamMultiplayerPeer::CONNECTION_CONNECTED;
		network.listen_socket = 12; network.poll_group = 12;
		auto peer = pending(network, 79); peer->set_peer_id(3); network.peers[3] = peer;
		SteamNetworkingMessage_t queued{79, 9};
		network.incoming_packets.push_back(&queued);
		int listens = listen_closes, polls = poll_destroys, calls = receive_calls;
		steam_up = false;
		network._poll();
		assert(receive_calls == calls);
		network._close();
		steam_up = true;
		assert(queued.releases == 0 && closes[79] == 0 && !peer->closed);
		assert(listen_closes == listens && poll_destroys == polls);
		assert(network.listen_socket == k_HSteamListenSocket_Invalid && network.poll_group == k_HSteamNetPollGroup_Invalid);
		assert(network.incoming_packets.is_empty() && network.peers.entries.empty() && network.steam_connections.entries.empty());
		// Packets queued or borrowed before shutdown are never handed out or released afterwards.
		SteamMultiplayerPeer reader;
		SteamNetworkingMessage_t borrowed{91, 1}, waiting{91, 2};
		reader.incoming_packets.push_back(&borrowed);
		reader.incoming_packets.push_back(&waiting);
		const uint8_t *buffer = nullptr;
		int size = 0;
		assert(reader._get_available_packet_count() == 2 && reader._get_packet(&buffer, &size) == OK);
		steam_up = false;
		assert(reader._get_available_packet_count() == 0);
		assert(reader._get_packet(&buffer, &size) == ERR_UNAVAILABLE && reader.current_packet == nullptr);
		reader._close();
		steam_up = true;
		assert(borrowed.releases == 0 && waiting.releases == 0);
	} else if (name == "C8") {
		// A failed create leaves no socket, poll group, or half-started session behind.
		for (int which = 0; which < 2; which++) {
			SteamMultiplayerPeer host; host._close();
			SteamMultiplayerPeer client; client._close();
			int before = listen_closes;
			fail_listen = which == 0; fail_poll = which == 1;
			assert(host.create_host(5) == ERR_CANT_CREATE);
			assert(host.listen_socket == k_HSteamListenSocket_Invalid && host.poll_group == k_HSteamNetPollGroup_Invalid);
			assert(!host.server && host.connection_status == SteamMultiplayerPeer::CONNECTION_DISCONNECTED);
			assert(listen_closes == before + which);
			assert(client.create_client(100, 5) == ERR_CANT_CREATE);
			assert(client.listen_socket == k_HSteamListenSocket_Invalid && client.steam_connections.entries.empty());
			assert(client.expected_host_steam_id == 0 && client.connection_status == SteamMultiplayerPeer::CONNECTION_DISCONNECTED);
			fail_listen = fail_poll = false;
			assert(host.create_host(5) == OK && client.create_client(100, 5) == OK);
		}
	} else if (name == "C9") {
		// Retiring a connection drops its queued messages; others keep theirs, in order.
		SteamMultiplayerPeer network; network.connection_status = SteamMultiplayerPeer::CONNECTION_CONNECTED;
		auto gone = pending(network, 80); gone->set_peer_id(3); network._upgrade_peer(80);
		auto kept = pending(network, 81); kept->set_peer_id(4); network._upgrade_peer(81);
		SteamNetworkingMessage_t a{80, 1}, b{81, 2}, c{80, 3}, d{81, 4};
		receive_batch = {&a, &b, &c, &d};
		network._poll();
		assert(network.incoming_packets.size() == 4);
		network._disconnect_peer(3, false);
		assert(a.releases == 1 && c.releases == 1 && b.releases == 0 && d.releases == 0);
		assert(network.incoming_packets.size() == 2 && network.incoming_packets.front()->get() == &b);
		assert(network.disconnected.size() == 1);
		// A remote close purges too, and still reports the peer.
		SteamNetConnectionStatusChangedCallback_t closed{81, k_ESteamNetworkingConnectionState_Connected,
			{k_ESteamNetworkingConnectionState_ClosedByPeer, 4, 200, 0, 0}};
		network.network_connection_status_changed(&closed);
		assert(b.releases == 1 && d.releases == 1 && network.incoming_packets.is_empty());
		assert(network.disconnected.size() == 2);
		// A forced local disconnect follows the MultiplayerPeer contract: no signal.
		auto forced = pending(network, 82); forced->set_peer_id(5); network._upgrade_peer(82);
		network._disconnect_peer(5, true);
		assert(network.disconnected.size() == 2 && !network.peers.has(5) && forced->closed);
	} else if (name == "C10") {
		// A malformed first message closes the connection instead of leaving it pending.
		SteamMultiplayerPeer network;
		auto peer = pending(network, 83);
		SteamNetworkingMessage_t junk{83, 3}; junk.malformed = true;
		receive_batch = {&junk};
		network._poll();
		assert(peer->closed && !network.steam_connections.has(83) && junk.releases == 1 && network.connected.empty());
		// Un-admitted connections are capped; the next incoming attempt is refused.
		configured_pending_limit = 2;
		SteamMultiplayerPeer host; host.server = true; host.unique_id = 1; host.listen_socket = 34;
		for (HSteamNetConnection conn : {84u, 85u, 86u}) {
			SteamNetConnectionStatusChangedCallback_t event{conn, 0, {k_ESteamNetworkingConnectionState_Connecting, 0, 200, 34, 0}};
			host.network_connection_status_changed(&event);
		}
		assert(host.steam_connections.has(84) && host.steam_connections.has(85));
		assert(!host.steam_connections.has(86) && closes[86] == 1);
		configured_pending_limit = 32;
	} else if (name == "C11") {
		// A connection that never completes the handshake expires; admitted peers never do.
		now_msec = 1000;
		SteamMultiplayerPeer host; host.server = true; host.unique_id = 1; host.listen_socket = 35;
		for (HSteamNetConnection conn : {87u, 88u}) {
			SteamNetConnectionStatusChangedCallback_t event{conn, 0, {k_ESteamNetworkingConnectionState_Connecting, 0, 200, 35, 0}};
			host.network_connection_status_changed(&event);
		}
		auto stalled = host.steam_connections[87];
		auto admitted = host.steam_connections[88];
		admitted->state = SteamPacketPeer::STATE_CONNECTED;
		SteamNetworkingMessage_t hello{88, 3};
		receive_batch = {&hello};
		host._poll();
		assert(host.peers.has(3));
		now_msec = 1000 + configured_handshake_timeout - 1; host._poll();
		assert(host.steam_connections.has(87) && !stalled->closed);
		now_msec = 1000 + configured_handshake_timeout; host._poll();
		assert(!host.steam_connections.has(87) && stalled->closed);
		assert(host.steam_connections.has(88) && host.peers.has(3) && !admitted->closed);
		// A client whose host never answers gives up instead of waiting forever.
		SteamMultiplayerPeer client; client._close();
		assert(client.create_client(100, 9) == OK);
		HSteamNetConnection timed_out_host = client.host_connection;
		now_msec += configured_handshake_timeout; client._poll();
		assert(closes[timed_out_host] == 1);
		assert(client.connection_status == SteamMultiplayerPeer::CONNECTION_DISCONNECTED);
		// Same, when the host's first message was malformed and erased the host connection.
		SteamMultiplayerPeer rejected; rejected._close();
		assert(rejected.create_client(100, 9) == OK);
		HSteamNetConnection host_conn = next_connection - 1;
		SteamNetworkingMessage_t bad{host_conn, 1}; bad.malformed = true;
		receive_batch = {&bad};
		rejected._poll();
		assert(!rejected.steam_connections.has(host_conn) && bad.releases == 1);
		assert(rejected.connection_status == SteamMultiplayerPeer::CONNECTION_CONNECTING);
		now_msec += configured_handshake_timeout; rejected._poll();
		assert(rejected.connection_status == SteamMultiplayerPeer::CONNECTION_DISCONNECTED);
		// A lobby member admitted first must not hide a host whose handshake was malformed.
		SteamMultiplayerPeer mesh_first; mesh_first._close();
		assert(mesh_first.create_client(100, 9) == OK);
		HSteamNetConnection mesh_host = next_connection - 1;
		auto member = pending(mesh_first, 94, 300);
		SteamNetworkingMessage_t member_hello{94, 5}, host_junk{mesh_host, 1};
		host_junk.malformed = true;
		receive_batch = {&member_hello, &host_junk};
		mesh_first._poll();
		assert(mesh_first.peers.has(5) && !mesh_first.steam_connections.has(mesh_host));
		assert(mesh_first.connection_status == SteamMultiplayerPeer::CONNECTION_CONNECTED);
		now_msec += configured_handshake_timeout; mesh_first._poll();
		assert(mesh_first.connection_status == SteamMultiplayerPeer::CONNECTION_DISCONNECTED);
		// A peer_connected handler that shuts Steam down ends the batch without touching Steam.
		SteamMultiplayerPeer stopping;
		pending(stopping, 92);
		pending(stopping, 93);
		stopping.on_connect = [&](uint32_t) { steam_up = false; };
		SteamNetworkingMessage_t hello_first{92, 3}, after_shutdown{93, 4};
		receive_batch = {&hello_first, &after_shutdown};
		stopping._poll();
		steam_up = true;
		assert(hello_first.releases == 1 && after_shutdown.releases == 0 && stopping.peers.has(3));
		stopping.on_connect = nullptr;
		receive_batch.clear();
		now_msec = 0;
	}
}
int main(int argc, char **argv) {
	std::string selected = argc > 1 ? argv[1] : "all";
	for (const char *name : {"C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "C10", "C11"}) {
		if (selected == "all" || selected == name) { lifecycle_case(name); }
	}
	if (selected != "all") { std::cout << selected << "=PASS\n"; return 0; }

	for (int64_t limit : {-10LL, 0LL, 1LL, 256LL, 2147483648LL}) {
		configured_queue_limit = limit;
		int expected = limit <= 0 ? 1 : limit > INT32_MAX ? INT32_MAX : static_cast<int>(limit);
		assert(SteamProjectSettings::get_max_queued_packets() == expected);
	}
	configured_queue_limit = 256;
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
		network._close();
		assert(data.releases == 1);
		network._close();
		assert(data.releases == 1);
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
	// Slow consumers retain at most the configured budget, not every received packet.
	std::vector<SteamNetworkingMessage_t> flood(2000, {50, 9});
	{
		SteamMultiplayerPeer queue;
		auto sender = pending(queue, 50);
		sender->set_peer_id(3);
		queue._upgrade_peer(50);
		receive_batch.clear();
		for (auto &message : flood) { receive_batch.push_back(&message); }
		queue._poll();
		assert(requested_messages == 255 && queue.incoming_packets.size() == 255);
		queue._poll();
		assert(requested_messages == 1 && queue.incoming_packets.size() == 256);
		int calls = receive_calls;
		queue._poll();
		assert(receive_calls == calls && receive_batch.size() == 1744);
		const uint8_t *buffer = nullptr;
		int size = 0;
		for (int i = 0; i < 64; i++) { assert(queue._get_packet(&buffer, &size) == OK); }
		assert(size == 4 && buffer != nullptr && queue.incoming_packets.size() == 192);
		queue._poll();
		assert(requested_messages == 64 && queue.incoming_packets.size() == 256);
		// Destructor releases both queued messages and the last borrowed packet.
	}
	for (int i = 0; i < 2000; i++) { assert(flood[i].releases == (i < 320 ? 1 : 0)); }
	receive_batch.clear();
	configured_queue_limit = 1;
	SteamMultiplayerPeer tiny;
	auto sender = pending(tiny, 60);
	sender->set_peer_id(3);
	tiny._upgrade_peer(60);
	SteamNetworkingMessage_t one{60, 9}, two{60, 9};
	receive_batch = {&one, &two};
	tiny._poll();
	assert(requested_messages == 1 && tiny.incoming_packets.size() == 1 && receive_batch.size() == 1);
	tiny._close();
	assert(one.releases == 1 && two.releases == 0);
	receive_batch.clear();
	std::cout << "NATIVE_MULTIPLAYER_TESTS=PASS\n";
}
