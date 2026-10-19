#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <variant>

using uint16 = uint16_t;
using int64 = int64_t;
using HSteamNetConnection = int;
constexpr int k_HSteamNetConnection_Invalid = 0;
constexpr int k_cbMaxSteamNetworkingSocketsMessageSizeSend = 512 * 1024;
constexpr int k_nSteamNetworkingSend_ReliableNoNagle = 9;
constexpr int k_EResultOK = 1;
struct Value {
	std::variant<int64_t, bool, std::string> data = int64_t(0);
	Value() = default;
	Value(int x) : data(int64_t(x)) {}
	Value(int64_t x) : data(x) {}
	Value(bool x) : data(x) {}
	Value(const char *x) : data(std::string(x)) {}
	operator int64_t() const { return std::holds_alternative<bool>(data) ? std::get<bool>(data) : std::get<int64_t>(data); }
};
using Dictionary = std::map<std::string, Value>;
struct Variant { enum { INT }; };
constexpr int PROPERTY_HINT_ENUM = 1;
constexpr int PROPERTY_HINT_RANGE = 2;
struct ProjectSettings {
	std::map<std::string, Value> settings, initial;
	std::map<std::string, Dictionary> info;
	static ProjectSettings *get_singleton() { static ProjectSettings instance; return &instance; }
	bool has_setting(const char *key) { return settings.count(key) != 0; }
	void set(const char *key, Value value) { settings[key] = value; }
	void set_setting(const char *key, Value value) { set(key, value); }
	Value get_setting(const char *key) { return settings[key]; }
	Value get_setting_with_override(const char *key) { return settings[key]; }
	void clear(const char *key) { settings.erase(key); }
	void set_initial_value(const char *key, Value value) { initial[key] = value; }
	void set_as_basic(const char *, bool) {}
	void add_property_info(Dictionary value) { info[std::get<std::string>(value["name"].data)] = value; }
};
struct SteamProjectSettings {
	static void register_settings();
	static int get_max_channels();
};
enum Error { OK, ERR_UNAVAILABLE, ERR_INVALID_PARAMETER, ERR_CANT_CREATE, ERR_CONNECTION_ERROR };
static int allocations = 0;
static int sends = 0;
static int warnings = 0;
static int errors = 0;
static int sent_lane = -1;
static int live_messages = 0;
static int closed_connections = 0;
static int64_t send_result = 1;
static bool fail_allocation = false;
static bool fail_configuration = false;
static std::map<int, int> actual_lanes;
#define WARN_PRINT_ONCE(...) ((void)0)
#define unlikely(x) (x)
#define WARN_PRINT(...) do { warnings++; } while (false)
#define ERR_FAIL_COND_V(condition, error) do { if (condition) { return error; } } while (false)
#define ERR_FAIL_COND_V_MSG(condition, error, ...) do { if (condition) { errors++; return error; } } while (false)

struct SteamNetworkingMessage_t {
	int m_conn = 0;
	int m_nFlags = 0;
	uint16 m_idxLane = 0;
	uint8_t *m_pData = nullptr;
};
static void *SteamAPI_SteamNetworkingUtils_SteamAPI() { return nullptr; }
static void *SteamAPI_SteamNetworkingSockets_SteamAPI() { return nullptr; }
static SteamNetworkingMessage_t *SteamAPI_ISteamNetworkingUtils_AllocateMessage(void *, int size) {
	assert(size > 0 && size <= k_cbMaxSteamNetworkingSocketsMessageSizeSend);
	allocations++;
	if (fail_allocation) { return nullptr; }
	live_messages++;
	auto *message = new SteamNetworkingMessage_t;
	message->m_pData = new uint8_t[size];
	return message;
}
static void SteamAPI_ISteamNetworkingSockets_SendMessages(void *, int count, SteamNetworkingMessage_t **messages, int64_t *result, bool delete_failed) {
	assert(count == 1);
	sends++;
	sent_lane = messages[0]->m_idxLane;
	if (result) { *result = send_result; }
	if (send_result > 0 || delete_failed) {
		delete[] messages[0]->m_pData;
		delete messages[0];
		live_messages--;
		if (!delete_failed) { messages[0] = nullptr; }
	}
}
static void SteamAPI_ISteamNetworkingSockets_CloseConnection(void *, int, int, const char *, bool) { closed_connections++; }
static int SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes(void *, int conn, int lanes, void *, void *) {
	if (fail_configuration || lanes < 1 || lanes > 255) { return 8; }
	actual_lanes[conn] = lanes;
	return k_EResultOK;
}
struct SteamPacketPeer {
	enum { STATE_NONE, STATE_CONNECTED };
	SteamPacketPeer();
	void set_connection_handle(HSteamNetConnection);
	uint64_t steam_id = 0;
	uint32_t peer_id = 0;
	SteamNetworkingMessage_t *last_packet = nullptr;
	struct PeerIDPacket { uint32_t peer_id = 0; };
	Error ping(uint32_t);
	int state = STATE_CONNECTED;
	int configured_lanes = 4;
	int connection_handle = 1;
	Error send(int, const uint8_t *, int, int);
};

// PRODUCTION_SEND

static void transport_case(const std::string &name) {
	auto *settings = ProjectSettings::get_singleton();
	if (name == "C2") {
		settings->set("steam/multiplayer_peer/max_channels", 4);
		SteamPacketPeer peer; peer.set_connection_handle(1); peer.state = SteamPacketPeer::STATE_CONNECTED;
		const uint8_t data = 42;
		for (int64_t result : {-25LL, -8LL, -3LL, 0LL}) {
			send_result = result;
			assert(peer.send(0, &data, 1, 0) == ERR_CONNECTION_ERROR);
			assert(live_messages == 0);
			assert(peer.ping(2) == ERR_CONNECTION_ERROR);
			assert(live_messages == 0);
		}
		send_result = 1;
		assert(peer.send(0, &data, 1, 0) == OK && peer.ping(2) == OK && live_messages == 0);
		int previous = allocations;
		assert(peer.send(0, &data, k_cbMaxSteamNetworkingSocketsMessageSizeSend + 1, 0) == ERR_INVALID_PARAMETER);
		assert(allocations == previous);
		fail_allocation = true;
		assert(peer.send(0, &data, 1, 0) == ERR_CANT_CREATE && peer.ping(2) == ERR_CANT_CREATE);
		fail_allocation = false;
	} else if (name == "C7") {
		for (int64_t count : {-10LL, 0LL, 1LL, 4LL, 255LL, 256LL, 65537LL, 4294967296LL}) {
			settings->set("steam/multiplayer_peer/max_channels", count);
			int expected = count < 1 ? 1 : count > 255 ? 255 : int(count);
			assert(SteamProjectSettings::get_max_channels() == expected);
			SteamPacketPeer peer; peer.set_connection_handle(1); peer.state = SteamPacketPeer::STATE_CONNECTED;
			assert(peer.configured_lanes == expected && actual_lanes[1] == expected);
		}
		settings->set("steam/multiplayer_peer/max_channels", 4);
		SteamPacketPeer peer;
		fail_configuration = true;
		peer.set_connection_handle(1); peer.state = SteamPacketPeer::STATE_CONNECTED;
		assert(peer.configured_lanes == 0 && peer.connection_handle == k_HSteamNetConnection_Invalid && closed_connections == 1);
		const uint8_t data = 42;
		assert(peer.send(0, &data, 1, 0) == ERR_INVALID_PARAMETER);
		fail_configuration = false;
		peer.set_connection_handle(1);
		assert(peer.configured_lanes == 4 && peer.send(3, &data, 1, 0) == OK);
		assert(peer.send(4, &data, 1, 0) == ERR_INVALID_PARAMETER);
		SteamProjectSettings::register_settings();
		auto info = settings->info["steam/multiplayer_peer/max_channels"];
		assert(int64_t(info["type"]) == Variant::INT && int64_t(info["hint"]) == PROPERTY_HINT_RANGE);
		assert(std::get<std::string>(info["hint_string"].data) == "1,255,1");
	} else if (name == "C8") {
		settings->settings.clear(); settings->initial.clear();
		SteamProjectSettings::register_settings();
		assert(settings->settings.size() == 9 && settings->initial.size() == 9);
		for (const auto &entry : settings->settings) { assert(settings->initial[entry.first].data == entry.second.data); }
		settings->set("steam/multiplayer_peer/max_channels", 7);
		settings->set("steam/initialization/processes/embed_callbacks", true);
		SteamProjectSettings::register_settings();
		assert(int64_t(settings->settings["steam/multiplayer_peer/max_channels"]) == 7);
		assert(int64_t(settings->initial["steam/multiplayer_peer/max_channels"]) == 4);
		assert(!std::get<bool>(settings->initial["steam/initialization/processes/embed_callbacks"].data));
	}
	errors = 0; warnings = 0;
}
int main(int argc, char **argv) {
	std::string selected = argc > 1 ? argv[1] : "all";
	for (const char *name : {"C2", "C7", "C8"}) {
		if (selected == "all" || selected == name) { transport_case(name); }
	}
	if (selected != "all") { std::cout << selected << "=PASS\n"; return 0; }

	SteamPacketPeer peer;
	peer.state = SteamPacketPeer::STATE_CONNECTED;
	const uint8_t data = 42;
	for (int lanes : {1, 4, 8}) {
		peer.configured_lanes = lanes;
		for (int channel = 0; channel < lanes; channel++) {
			assert(peer.send(channel, &data, 1, 0) == OK);
			assert(sent_lane == channel && warnings == 0 && errors == 0);
		}
		for (int channel : {INT32_MIN, -1, lanes, lanes + 1, INT32_MAX}) {
			int previous_allocations = allocations;
			int previous_sends = sends;
			int previous_errors = errors;
			assert(peer.send(channel, &data, 1, 0) == ERR_INVALID_PARAMETER);
			assert(allocations == previous_allocations && sends == previous_sends);
			assert(errors == previous_errors + 1 && warnings == 0);
		}
		errors = 0;
	}
	std::cout << "NATIVE_LANE_TESTS=PASS\n";
}
