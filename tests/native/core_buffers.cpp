// Compile extracted production wrappers against offline boundary doubles.
#include <any>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using int64 = int64_t;
using HSteamNetConnection = uint32_t;
using HSteamListenSocket = uint32_t;
using UGCFileWriteStreamHandle_t = uint64_t;
using UGCHandle_t = uint64_t;
using SteamAPICall_t = uint64_t;
using EUGCReadAction = int;
using ESteamNetworkingConfigDataType = int;
using ESteamNetworkingConfigValue = int;
using ESteamNetworkingConfigScope = int;
using SteamNetworkingErrMsg = char[1024];
constexpr int OK = 0;
constexpr int STEAM_LARGE_BUFFER_SIZE = 8160;
constexpr int k_cbMaxSteamNetworkingSocketsMessageSizeSend = 512 * 1024;
constexpr int k_cchMaxSteamNetworkingPingLocationString = 1024;
static int errors = 0;
static bool fail_resize = false;
#define ERR_FAIL_COND_V_MSG(condition, value, ...) do { if (condition) { errors++; return value; } } while (false)
#define ERR_FAIL_COND_MSG(condition, ...) do { if (condition) { errors++; return; } } while (false)
#define ERR_FAIL_V_MSG(value, ...) do { errors++; return value; } while (false)

template<class T> struct Packed {
	std::vector<T> values;
	int64_t reported_size = -1;
	int64_t size() const { return reported_size < 0 ? (int64_t)values.size() : reported_size; }
	int resize(int64_t count) {
		assert(count >= 0 && count < 1000000);
		if (fail_resize) { fail_resize = false; return 1; }
		values.resize((size_t)count); return OK;
	}
	void clear() { values.clear(); reported_size = -1; }
	void append(T value) { values.push_back(value); }
	void set(int64_t index, T value) { values.at((size_t)index) = value; }
	const T *ptr() const { return values.data(); }
	T *ptrw() { return values.data(); }
	T operator[](size_t index) const { return values.at(index); }
};
using PackedByteArray = Packed<uint8_t>;
using PackedInt64Array = Packed<int64_t>;
using PackedInt32Array = Packed<int32_t>;
struct String : std::string {
	using std::string::string;
	struct Utf8 { const char *data; const char *get_data() const { return data; } };
	Utf8 utf8() const { return {c_str()}; }
};
struct Variant {
	enum { OTHER, PACKED_BYTE_ARRAY };
	std::any value;
	Variant() = default;
	Variant(const char *text) : value(String(text)) {}
	Variant(char *text) : value(String(text)) {}
	template<class T> Variant(T item) : value(item) {}
	int get_type() const { return value.type() == typeid(PackedByteArray) ? PACKED_BYTE_ARRAY : OTHER; }
	operator PackedByteArray() const { return std::any_cast<PackedByteArray>(value); }
	template<class T> T as() const { return std::any_cast<T>(value); }
};
struct Array {
	std::vector<Variant> values;
	int64_t reported_size = -1;
	int64_t size() const { return reported_size < 0 ? (int64_t)values.size() : reported_size; }
	Variant operator[](size_t index) const { return values.at(index); }
	template<class T> void append(T value) { values.emplace_back(value); }
};
struct Dictionary {
	std::map<std::string, Variant> values;
	Variant &operator[](const char *key) { return values[key]; }
	bool empty() const { return values.empty(); }
};
struct SteamNetworkPingLocation_t { uint8_t m_data[512]; };
struct SteamNetworkingIdentity { int m_eType = 1; };
struct SteamNetworkingFakeIPResult_t {
	static constexpr int k_nMaxReturnPorts = 8;
	int m_eResult = 1;
	SteamNetworkingIdentity m_identity;
	uint32_t m_unIP = 1;
	uint16_t m_unPorts[k_nMaxReturnPorts]{};
};
struct SteamNetworkingIPAddr { static constexpr int k_cchMaxString = 48; };
struct SteamPartyBeaconLocation_t { int m_eType = 0; uint64_t m_ulLocationID = 0; };
struct RemotePlayInputMouseMotion_t { bool m_bAbsolute; float m_flNormalizedX, m_flNormalizedY; int m_nDeltaX, m_nDeltaY; };
struct RemotePlayInputMouseWheel_t { int m_eDirection; float m_flAmount; };
struct RemotePlayInputKey_t { int m_eScancode, m_unModifiers, m_unKeycode; };
using ERemotePlayMouseButton = int;
enum ERemotePlayInputType { k_ERemotePlayInputMouseMotion, k_ERemotePlayInputMouseButtonDown, k_ERemotePlayInputMouseButtonUp, k_ERemotePlayInputMouseWheel, k_ERemotePlayInputKeyDown, k_ERemotePlayInputKeyUp };
struct RemotePlayInput_t {
	uint32_t m_unSessionID = 5;
	ERemotePlayInputType m_eType = k_ERemotePlayInputKeyDown;
	RemotePlayInputMouseMotion_t m_MouseMotion{};
	ERemotePlayMouseButton m_eMouseButton = 1;
	RemotePlayInputMouseWheel_t m_MouseWheel{};
	RemotePlayInputKey_t m_Key{1, 2, 3};
};

static uint32_t produced_ticket = 240, ticket_handle = 7;
static bool ticket_identity = false;
static void *SteamAPI_SteamUser() { return (void *)1; }
static uint32_t SteamAPI_ISteamUser_GetAuthSessionTicket(void *, void *buffer, int capacity, uint32_t *out_size, const void *identity) {
	assert(capacity == 1024 && bool(identity) == ticket_identity);
	memset(buffer, 0x5a, std::min(produced_ticket, uint32_t(capacity)));
	*out_size = produced_ticket;
	return ticket_handle;
}
static bool sockets_available = true, utils_available = true;
static void *SteamAPI_SteamNetworkingSockets_SteamAPI() { return sockets_available ? (void *)1 : nullptr; }
static void *SteamAPI_SteamNetworkingUtils_SteamAPI() { return utils_available ? (void *)1 : nullptr; }
static void *SteamAPI_SteamRemotePlay() { return (void *)1; }
static void *SteamAPI_SteamRemoteStorage() { return (void *)1; }
static void *SteamAPI_SteamParties() { return (void *)1; }
struct SteamNetworkingMessage_t {
	std::vector<uint8_t> storage;
	void *m_pData;
	int m_cbSize;
	HSteamNetConnection m_conn = 0;
	int m_nFlags = 0;
	bool released = false;
	explicit SteamNetworkingMessage_t(int size) : storage(size), m_pData(size ? storage.data() : nullptr), m_cbSize(size) {}
};
static std::vector<SteamNetworkingMessage_t *> allocated;
static int allocate_calls = 0, fail_allocate_at = -1, sends = 0;
static std::vector<int64> send_results;
static std::vector<std::vector<uint8_t>> submitted;
static SteamNetworkingMessage_t *SteamAPI_ISteamNetworkingUtils_AllocateMessage(void *, int size) {
	if (allocate_calls++ == fail_allocate_at) { return nullptr; }
	auto *message = new SteamNetworkingMessage_t(size); allocated.push_back(message); return message;
}
static void SteamAPI_SteamNetworkingMessage_t_Release(SteamNetworkingMessage_t *message) {
	assert(message && !message->released);
	assert(message->m_pData == (message->m_cbSize ? message->storage.data() : nullptr));
	message->released = true;
}
static void SteamAPI_ISteamNetworkingSockets_SendMessages(void *, int count, SteamNetworkingMessage_t **messages, int64 *results, bool delete_failed) {
	sends++;
	for (int i = 0; i < count; i++) {
		auto *message = messages[i];
		assert(message && !message->released && message->m_conn == 42 && message->m_nFlags == 8);
		assert(message->m_pData == (message->m_cbSize ? message->storage.data() : nullptr));
		submitted.push_back(message->storage);
		results[i] = send_results.at(i);
		if (delete_failed || results[i] > 0) {
			SteamAPI_SteamNetworkingMessage_t_Release(message);
			if (!delete_failed) { messages[i] = nullptr; }
		}
	}
}
static int ping_calls = 0;
static int SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations(void *, const SteamNetworkPingLocation_t &, const SteamNetworkPingLocation_t &) { ping_calls++; return 7; }
static int SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost(void *, const SteamNetworkPingLocation_t &) { ping_calls++; return 9; }
static void SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString(void *, const SteamNetworkPingLocation_t &, char *buffer, int capacity) {
	ping_calls++; assert(capacity == k_cchMaxSteamNetworkingPingLocationString);
	memset(buffer, 'p', capacity - 1); buffer[capacity - 1] = 0;
}
static int config_mode = 0, config_calls = 0;
static int SteamAPI_ISteamNetworkingUtils_GetConfigValue(void *, int, int, uint32_t, int *type, void *buffer, size_t *size) {
	config_calls++;
	if (config_mode == 1) { return -1; }
	*type = 1;
	if (!buffer) { assert(*size == 0); *size = 4; return -3; }
	assert(*size == 4);
	if (config_mode == 2) { *size = 8; return -3; }
	const uint8_t bytes[] = {1, 2, 3, 4}; memcpy(buffer, bytes, sizeof(bytes)); *size = 4;
	return config_mode == 3 ? 2 : 1;
}
static int status_mode = -1, status_calls = 0;
static int SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus(void *, uint32_t, char *buffer, int capacity) {
	status_calls++;
	if (status_mode < 0) { return -1; }
	if (status_mode > 0 && capacity < STEAM_LARGE_BUFFER_SIZE + 10) { return STEAM_LARGE_BUFFER_SIZE + 10; }
	strcpy(buffer, "connected"); return 0;
}
static bool certificate_success = true;
static bool SteamAPI_ISteamNetworkingSockets_SetCertificate(void *, const void *, int, char *error) {
	assert(error[0] == 0); if (!certificate_success) { strcpy(error, "rejected"); } return certificate_success;
}
static bool SteamAPI_ISteamNetworkingSockets_GetCertificateRequest(void *, int *size, void *buffer, char *error) {
	assert(error[0] == 0 && *size == 512); memset(buffer, 1, 4); *size = 4; return true;
}
static bool SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress(void *, uint32_t, SteamNetworkingIPAddr *) { return true; }
static void SteamAPI_SteamNetworkingIPAddr_ToString(SteamNetworkingIPAddr *, char *buffer, size_t capacity, bool with_port) {
	assert(capacity == SteamNetworkingIPAddr::k_cchMaxString); strcpy(buffer, with_port ? "127.0.0.1:27015" : "127.0.0.1");
}
static void SteamAPI_ISteamNetworkingSockets_GetFakeIP(void *, int, SteamNetworkingFakeIPResult_t *result) {
	*result = {}; for (int i = 0; i < result->k_nMaxReturnPorts; i++) { result->m_unPorts[i] = 27015 + i; }
}
static uint32_t beacon_count = 1;
static bool beacon_success = true;
static int beacon_calls = 0;
static bool SteamAPI_ISteamParties_GetNumAvailableBeaconLocations(void *, uint32_t *count) { *count = beacon_count; return true; }
static bool SteamAPI_ISteamParties_GetAvailableBeaconLocations(void *, SteamPartyBeaconLocation_t *locations, uint32_t capacity) {
	beacon_calls++; assert(capacity == beacon_count);
	for (uint32_t i = 0; i < capacity; i++) { locations[i] = {1, i + 1}; } return beacon_success;
}
static uint32_t input_count = 1;
static int input_calls = 0;
static uint32_t SteamAPI_ISteamRemotePlay_GetInput(void *, RemotePlayInput_t *events, uint32_t capacity) {
	input_calls++; if (!events) { return input_count; }
	assert(capacity <= input_count); for (uint32_t i = 0; i < capacity; i++) { events[i] = {}; } return capacity;
}
static int write_calls = 0;
static uint32_t write_size = 0;
static bool SteamAPI_ISteamRemoteStorage_FileWrite(void *, const char *, const void *, int32_t size) { write_calls++; write_size = size; return true; }
static SteamAPICall_t SteamAPI_ISteamRemoteStorage_FileWriteAsync(void *, const char *, const void *, uint32_t size) { write_calls++; write_size = size; return 1; }
static bool SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk(void *, uint64_t, const void *, int32_t size) { write_calls++; write_size = size; return true; }
static int32_t read_result = 2;
static int read_calls = 0;
static int32_t SteamAPI_ISteamRemoteStorage_UGCRead(void *, uint64_t, void *buffer, int32_t capacity, uint32_t, int) {
	read_calls++; if (read_result > 0 && read_result <= capacity) { memset(buffer, 5, read_result); } return read_result;
}
struct Steam {
	Dictionary getAuthSessionTicket(uint64_t remote_steam_id = 0);
	SteamNetworkingIdentity getIdentityFromSteamID(uint64_t) { return {}; }
	using NetworkingConfigValue = int;
	using NetworkingConfigScope = int;
	using NetworkingGetConfigValueResult = int;
	using UGCReadAction = int;
	enum { NETWORKING_GET_CONFIG_VALUE_BUFFER_TOO_SMALL = -3, NETWORKING_GET_CONFIG_VALUE_OK = 1, NETWORKING_GET_CONFIG_VALUE_OK_INHERITED = 2 };
	struct CallResult { template<class T> void Set(uint64_t, Steam *, T) {} } callResultFileWriteAsyncComplete;
	void file_write_async_complete() {}
	String getStringFromIP(uint32_t) { return "1.0.0.0"; }
	uint64_t getSteamIDFromIdentity(const SteamNetworkingIdentity &) { return 1; }
	PackedInt32Array emitted_ports;
	void emit_signal(const char *, int, uint64_t, String, PackedInt32Array ports) { emitted_ports = ports; }
	PackedInt64Array sendMessages(uint32_t, Array, int, bool);
	Dictionary getConfigValue(int, int, uint32_t);
	int estimatePingTimeBetweenTwoLocations(PackedByteArray, PackedByteArray);
	int estimatePingTimeFromLocalHost(PackedByteArray);
	String convertPingLocationToString(PackedByteArray);
	Dictionary getDetailedConnectionStatus(uint32_t);
	Dictionary getCertificateRequest();
	Dictionary setCertificate(const PackedByteArray &);
	String getListenSocketAddress(uint32_t, bool);
	Dictionary getFakeIP(int);
	void fake_ip_result(SteamNetworkingFakeIPResult_t *);
	Array getAvailableBeaconLocations(uint32_t);
	Dictionary createMouseCursor(int, int, int, int, int);
	Array getInput(uint32_t);
	bool fileWrite(const String &, PackedByteArray, int32_t);
	void fileWriteAsync(const String &, PackedByteArray, int32_t);
	bool fileWriteStreamWriteChunk(uint64_t, PackedByteArray);
	PackedByteArray ugcRead(uint64_t, int32_t, uint32_t, int);
};

// PRODUCTION_METHODS

static PackedByteArray bytes(int size) { PackedByteArray value; value.resize(size); return value; }
static void check_messages_released() { for (auto *message : allocated) { assert(message->released); } }
int main() {
	Steam steam;
	for (uint64_t remote : {uint64_t(0), uint64_t(42)}) {
		ticket_identity = remote != 0;
		for (uint32_t size : {0u, 240u, 1024u, 1025u}) {
			produced_ticket = size;
			for (uint32_t handle : {0u, 7u}) {
				ticket_handle = handle;
				auto ticket = steam.getAuthSessionTicket(remote);
				uint32_t expected = handle == 0 || size > 1024 ? 0 : size;
				assert(ticket["id"].as<uint32_t>() == (size > 1024 ? 0 : handle));
				assert(ticket["size"].as<uint32_t>() == expected);
				auto buffer = ticket["buffer"].as<PackedByteArray>();
				assert(buffer.size() == expected);
				for (uint8_t byte : buffer.values) { assert(byte == 0x5a); }
			}
		}
	}
	PackedByteArray payload; payload.values = {1, 2, 3};
	Array messages; messages.append(payload); messages.append(bytes(0)); messages.append(payload);
	for (bool delete_failed : {false, true}) {
		send_results = {100, -8, 0}; submitted.clear();
		auto results = steam.sendMessages(42, messages, 8, delete_failed);
		assert(results.values == send_results && submitted.size() == 3);
		assert(submitted[0] == payload.values && submitted[1].empty() && submitted[2] == payload.values);
		check_messages_released();
	}
	int previous = allocate_calls;
	Array invalid; invalid.append(123);
	assert(steam.sendMessages(42, invalid, 8, true).size() == 0 && allocate_calls == previous);
	invalid.values.clear(); invalid.append(bytes(k_cbMaxSteamNetworkingSocketsMessageSizeSend + 1));
	assert(steam.sendMessages(42, invalid, 8, true).size() == 0 && allocate_calls == previous);
	invalid.reported_size = (int64_t)INT32_MAX + 1;
	assert(steam.sendMessages(42, invalid, 8, true).size() == 0 && allocate_calls == previous);
	utils_available = false;
	assert(steam.sendMessages(42, messages, 8, true).size() == 0 && allocate_calls == previous);
	utils_available = true; sockets_available = false;
	assert(steam.sendMessages(42, messages, 8, true).size() == 0 && allocate_calls == previous);
	sockets_available = true;
	int previous_sends = sends;
	assert(steam.sendMessages(42, Array(), 8, false).size() == 0 && sends == previous_sends);
	for (int offset : {0, 1, 2}) {
		fail_allocate_at = allocate_calls + offset;
		assert(steam.sendMessages(42, messages, 8, true).size() == 0 && sends == previous_sends);
		check_messages_released();
	}
	fail_allocate_at = -1;
	Array boundary; boundary.append(bytes(k_cbMaxSteamNetworkingSocketsMessageSizeSend)); send_results = {1};
	assert(steam.sendMessages(42, boundary, 8, false).size() == 1); check_messages_released();

	auto location = bytes(512);
	for (int size : {0, 1, 511, 513}) {
		previous = ping_calls;
		assert(steam.estimatePingTimeBetweenTwoLocations(bytes(size), location) == -1);
		assert(steam.estimatePingTimeBetweenTwoLocations(location, bytes(size)) == -1);
		assert(steam.estimatePingTimeFromLocalHost(bytes(size)) == -1);
		assert(steam.convertPingLocationToString(bytes(size)).empty() && ping_calls == previous);
	}
	assert(steam.estimatePingTimeBetweenTwoLocations(location, location) == 7);
	assert(steam.estimatePingTimeFromLocalHost(location) == 9);
	assert(steam.convertPingLocationToString(location).size() == 1023);
	for (int mode : {0, 1, 2, 3}) {
		config_mode = mode; auto result = steam.getConfigValue(0, 0, 0);
		auto data = result["value"].as<PackedByteArray>();
		assert(data.size() == ((mode == 0 || mode == 3) ? 4 : 0));
		assert(result["result"].as<int>() == (mode == 0 ? 1 : mode == 1 ? -1 : mode == 2 ? -3 : 2));
	}
	config_mode = 0; fail_resize = true; assert(steam.getConfigValue(0, 0, 0).empty());
	for (int mode : {-1, 0, 1}) {
		status_mode = mode; status_calls = 0; auto result = steam.getDetailedConnectionStatus(0);
		assert(result["status"].as<String>() == (mode < 0 ? "" : "connected"));
		assert(status_calls == (mode > 0 ? 2 : 1));
	}
	for (bool success : {true, false}) {
		certificate_success = success; auto result = steam.setCertificate(payload);
		assert(result["response"].as<bool>() == success);
		assert(result["error"].as<String>() == (success ? "" : "rejected"));
	}
	assert(steam.getCertificateRequest()["error_message"].as<String>().empty());
	assert(steam.getListenSocketAddress(1, true) == "127.0.0.1:27015");
	assert(steam.getListenSocketAddress(1, false) == "127.0.0.1");
	SteamNetworkingFakeIPResult_t fake_ip;
	SteamAPI_ISteamNetworkingSockets_GetFakeIP(nullptr, 0, &fake_ip); steam.fake_ip_result(&fake_ip);
	auto ports = steam.getFakeIP(0)["ports"].as<PackedInt32Array>();
	assert(ports.size() == 8 && steam.emitted_ports.size() == 8);
	for (int i = 0; i < 8; i++) { assert(ports[i] == 27015 + i && steam.emitted_ports[i] == 27015 + i); }
	for (uint32_t count : {0u, 1u, 256u, 257u}) {
		beacon_count = count;
		for (uint32_t limit : {0u, 1u, 256u, 257u, UINT32_MAX}) {
			auto result = steam.getAvailableBeaconLocations(limit);
			assert(result.size() == (limit < count ? limit : count));
			for (int64_t i = 0; i < result.size(); i++) {
				assert(result[(size_t)i].as<Dictionary>()["location_id"].as<uint64_t>() == (uint64_t)i + 1);
			}
		}
	}
	beacon_success = false; assert(steam.getAvailableBeaconLocations(257).size() == 0);
	assert(steam.createMouseCursor(16, 16, 0, 0, 0).empty());
	for (uint32_t count : {0u, 1u, 4u}) {
		input_count = count;
		for (uint32_t limit : {0u, 1u, 8u, UINT32_MAX}) {
			auto result = steam.getInput(limit); assert(result.size() == (limit < count ? limit : count));
			if (result.size()) { assert(result[0].as<Dictionary>()["session_id"].as<uint32_t>() == 5); }
		}
	}
	for (int32_t size : {-1, 4, INT32_MAX}) {
		previous = write_calls;
		assert(!steam.fileWrite("save", payload, size)); steam.fileWriteAsync("save", payload, size);
		assert(write_calls == previous);
	}
	for (int32_t size : {0, 1, 3}) {
		assert(steam.fileWrite("save", payload, size) && write_size == (size == 0 ? 3u : (uint32_t)size));
		steam.fileWriteAsync("save", payload, size); assert(write_size == (size == 0 ? 3u : (uint32_t)size));
	}
	PackedByteArray large; large.reported_size = (int64_t)UINT32_MAX + 1;
	previous = write_calls;
	assert(!steam.fileWrite("save", large, 0)); steam.fileWriteAsync("save", large, 0);
	assert(!steam.fileWriteStreamWriteChunk(0, large) && write_calls == previous);
	assert(steam.fileWriteStreamWriteChunk(0, payload) && write_size == 3);
	previous = read_calls; assert(steam.ugcRead(0, -1, 0, 0).size() == 0 && read_calls == previous);
	for (int result : {-1, 0, 2, 4, 5}) {
		read_result = result; auto data = steam.ugcRead(0, 4, 0, 0);
		assert(data.size() == (result < 0 || result > 4 ? 0 : result));
	}
	fail_resize = true; assert(steam.ugcRead(0, 4, 0, 0).size() == 0);
	for (auto *message : allocated) { delete message; }
	std::cout << "NATIVE_CORE_BUFFER_TESTS=PASS\n";
}
