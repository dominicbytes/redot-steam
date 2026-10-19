#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

using uint16 = uint16_t;
enum Error { OK, ERR_UNAVAILABLE, ERR_INVALID_PARAMETER, ERR_CANT_CREATE };
static int allocations = 0;
static int sends = 0;
static int warnings = 0;
static int errors = 0;
static int sent_lane = -1;
#define unlikely(x) (x)
#define WARN_PRINT(...) do { warnings++; } while (false)
#define ERR_FAIL_COND_V(condition, error) do { if (condition) { return error; } } while (false)
#define ERR_FAIL_COND_V_MSG(condition, error, ...) do { if (condition) { errors++; return error; } } while (false)

struct SteamNetworkingMessage_t {
	int m_conn;
	int m_nFlags;
	uint16 m_idxLane;
	uint8_t bytes[16];
	void *m_pData = bytes;
};
static void *SteamAPI_SteamNetworkingUtils_SteamAPI() { return nullptr; }
static void *SteamAPI_SteamNetworkingSockets_SteamAPI() { return nullptr; }
static SteamNetworkingMessage_t *SteamAPI_ISteamNetworkingUtils_AllocateMessage(void *, int size) {
	assert(size > 0 && size <= 16);
	allocations++;
	return new SteamNetworkingMessage_t;
}
static void SteamAPI_ISteamNetworkingSockets_SendMessages(void *, int count, SteamNetworkingMessage_t **messages, void *, bool) {
	assert(count == 1);
	sends++;
	sent_lane = messages[0]->m_idxLane;
	delete messages[0];
}
struct SteamPacketPeer {
	enum { STATE_CONNECTED };
	int state = STATE_CONNECTED;
	int configured_lanes = 4;
	int connection_handle = 1;
	Error send(int, const uint8_t *, int, int);
};

// PRODUCTION_SEND

int main() {
	SteamPacketPeer peer;
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
