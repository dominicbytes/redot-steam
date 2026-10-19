// Production wrapper methods exercised with offline engine and transport doubles.
#include <algorithm>
#include <any>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define ERR_FAIL_COND_V_MSG(c, v, ...) do { if (c) return v; } while (false)
#define ERR_FAIL_COND_MSG(c, ...) do { if (c) return; } while (false)
#define MIN(a, b) std::min(a, b)
#define CLAMP(x, a, b) std::clamp(x, a, b)
#define STEAM_BUFFER_SIZE 255
#define PUBLISHED_FILE_URL_MAX 256
#define memnew(T) new T
#define memdelete(p) delete p
using int64 = int64_t;
using PublishedFileId_t = uint64_t;
using UGCQueryHandle_t = uint64_t;
using SteamAPICall_t = uint64_t;
using EUGCContentDescriptorID = int;
using EResult = int;
constexpr SteamAPICall_t k_uAPICallInvalid = 0;
static int array_allocations = 0;
void *operator new[](size_t n) { ++array_allocations; return std::malloc(n ? n : 1); }
void operator delete[](void *p) noexcept { --array_allocations; std::free(p); }
void operator delete[](void *p, size_t) noexcept { operator delete[](p); }

struct String : std::string {
	using std::string::string;
	struct Utf8 { const char *data; const char *get_data() const { return data; } };
	Utf8 utf8() const { return {c_str()}; }
	static String utf8(const char *p, int n) { return String(p, n); }
};
template<class T> struct Packed : std::vector<T> {
	using std::vector<T>::vector;
	void append(T v) { this->push_back(v); }
	T *ptrw() { return this->data(); }
	const T *ptr() const { return this->data(); }
	int64_t size() const { return static_cast<int64_t>(std::vector<T>::size()); }
};
using PackedByteArray = Packed<uint8_t>;
using PackedInt64Array = Packed<int64_t>;
using PackedInt32Array = Packed<int32_t>;
using PackedFloat64Array = Packed<double>;
struct Array : std::vector<std::any> { template<class T> void append(T v) { emplace_back(v); } };
using Dictionary = std::map<std::string, std::any>;
struct FileDetailsResult_t { uint32_t m_eResult; uint64_t m_ulFileSize; uint32_t m_unFlags; uint8_t m_FileSHA[20]; };
struct GamepadTextInputDismissed_t { bool m_bSubmitted; uint32_t m_unAppID; };
struct SteamUGCQueryCompleted_t {
	uint64_t m_handle; int m_eResult; uint32_t m_unNumResultsReturned, m_unTotalMatchingResults;
	bool m_bCachedData; char m_rgchNextCursor[256];
};
struct LeaderboardEntry_t {
	int m_nScore, m_nGlobalRank, m_cDetails;
	struct { uint64_t ConvertToUint64() const { return 5; } } m_steamIDUser;
	uint64_t m_hUGC;
};
struct LeaderboardScoresDownloaded_t { uint64_t m_hSteamLeaderboard, m_hSteamLeaderboardEntries; int m_cEntryCount; };
struct Steam;
struct CallResult {
	bool active = false; Steam *owner = nullptr;
	void Set(uint64_t, Steam *p, void (Steam::*)(SteamUGCQueryCompleted_t*, bool)) { active = true; owner = p; }
	bool IsActive() const { return active; }
};
static bool ugc_available = true, text_success = true, setter_success = true;
static int auth_calls = 0, download_calls = 0, text_calls = 0, send_calls = 0;
static int auth_written = 3;
static uint32_t descriptor_count = 1, text_length = 1500;
static uint64_t next_call = 100;
static std::map<std::string, std::vector<std::any>> signals;
static void *SteamAPI_SteamUser() { return reinterpret_cast<void*>(1); }
static void *SteamAPI_SteamUGC() { return ugc_available ? reinterpret_cast<void*>(1) : nullptr; }
static void *SteamAPI_SteamUserStats() { return reinterpret_cast<void*>(1); }
static void *SteamAPI_SteamUtils() { return reinterpret_cast<void*>(1); }
static int SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED(void*, void *p, int n, uint64_t, uint32_t, uint16_t, bool) {
	assert(n == 2048); assert(p); if (auth_written > 0 && auth_written <= n) std::memcpy(p, "abc", 3); return auth_written;
}
static int SteamAPI_ISteamUser_BeginAuthSession(void*, const void*, int, uint64_t) { ++auth_calls; return 0; }
static uint32_t SteamAPI_ISteamUGC_GetDownloadedItems(void*, PublishedFileId_t *p, uint32_t n) {
	++download_calls; if (!n) return 0; p[0] = 123; return 1;
}
static bool SteamAPI_ISteamUGC_GetQueryUGCChildren(void*, uint64_t, uint32_t, PublishedFileId_t *p, uint32_t n) {
	for (uint32_t i = 0; i < n; ++i) { p[i] = (uint64_t(1) << 54) + i; }
	return true;
}
static uint32_t SteamAPI_ISteamUGC_GetQueryUGCContentDescriptors(void*, uint64_t, uint32_t, int *p, uint32_t n) {
	for (uint32_t i = 0; i < std::min(n, descriptor_count); ++i) { p[i] = 3 + i; }
	return descriptor_count;
}
static uint32_t SteamAPI_ISteamUGC_GetUserContentDescriptorPreferences(void *s, int *p, uint32_t n) {
	return SteamAPI_ISteamUGC_GetQueryUGCContentDescriptors(s, 0, 0, p, n);
}
static bool SteamAPI_ISteamUGC_SetItemsDisabledLocally(void*, uint64_t *p, uint32_t n, bool) { assert(n == 2 && p[0] == 5 && p[1] == 6); return setter_success; }
static bool SteamAPI_ISteamUGC_SetSubscriptionsLoadOrder(void *s, uint64_t *p, uint32_t n) { return SteamAPI_ISteamUGC_SetItemsDisabledLocally(s, p, n, false); }
static bool SteamAPI_ISteamUGC_GetSupportedGameVersionData(void*, uint64_t, uint32_t, uint32_t, char *a, char *b, uint32_t n) {
	assert(n == STEAM_BUFFER_SIZE); std::strcpy(a, "min"); std::strcpy(b, "max"); return true;
}
static int32_t SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64(void*, const char*, int64 *p, uint32_t n) { assert(p && n == 60 * sizeof(*p)); p[0] = 123; p[1] = 456; return 2; }
static int32_t SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble(void*, const char*, double *p, uint32_t n) { assert(p && n == 60 * sizeof(*p)); p[0] = 1.5; return 1; }
static bool SteamAPI_ISteamUtils_GetImageSize(void*, int, uint32_t*, uint32_t*) { return false; }
static uint32_t SteamAPI_ISteamUtils_GetEnteredGamepadTextLength(void*) { return text_length; }
static bool SteamAPI_ISteamUtils_GetEnteredGamepadTextInput(void*, char *p, uint32_t n) {
	++text_calls; assert(n == text_length + 1); if (!text_success) return false;
	std::memset(p, 'x', text_length); p[text_length] = 0; return true;
}
static bool SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry(void*, uint64_t, int i, LeaderboardEntry_t *p, int32_t *d, int n) {
	assert(n == 3); p->m_nScore = 10; p->m_nGlobalRank = i+1; p->m_hUGC = 0;
	p->m_cDetails = i == 0 ? 3 : i == 1 ? 1 : 0;
	for (int j = 0; j < p->m_cDetails; ++j) { d[j] = i * 10 + j; }
	return true;
}
static uint64_t SteamAPI_ISteamUGC_SendQueryUGCRequest(void*, uint64_t) { ++send_calls; return next_call; }
static bool SteamAPI_ISteamUGC_ReleaseQueryUGCRequest(void*, uint64_t) { return true; }
struct Steam {
	enum BeginAuthSessionResult { BEGIN_AUTH_SESSION_RESULT_INVALID_TICKET = 1 };
	Dictionary initiateGameConnection(uint64_t, String, uint16_t, bool);
	BeginAuthSessionResult beginAuthSession(PackedByteArray, int, uint64_t);
	uint32_t getIPFromString(String) { return 0; }
	PackedInt64Array getDownloadedItems(uint32_t);
	Dictionary getQueryUGCChildren(uint64_t, uint32_t, uint32_t);
	Dictionary getQueryUGCContentDescriptors(uint64_t, uint32_t, uint32_t);
	Array getUserContentDescriptorPreferences(uint32_t);
	bool setItemsDisabledLocally(PackedInt64Array, bool);
	bool setSubscriptionsLoadOrder(PackedInt64Array);
	Dictionary getSupportedGameVersionData(uint64_t, uint32_t, uint32_t);
	PackedInt64Array getGlobalStatIntHistory(const String&);
	PackedFloat64Array getGlobalStatFloatHistory(const String&);
	Dictionary getImageSize(int);
	void gamepad_text_input_dismissed(GamepadTextInputDismissed_t*);
	void leaderboard_scores_downloaded(LeaderboardScoresDownloaded_t*, bool);
	int leaderboard_details_max = 3; Array leaderboard_entries_array;
	std::map<uint64_t, CallResult> callResultsUGCQueryCompleted;
	void sendQueryUGCRequest(uint64_t);
	bool releaseQueryUGCRequest(uint64_t);
	void ugc_query_completed(SteamUGCQueryCompleted_t*, bool);
	template<class... Args> void emit_signal(const char *name, Args... args) { signals[name] = {args...}; }
};

// PRODUCTION_METHODS

int main() {
	Steam s;
	auto connection = s.initiateGameConnection(1, "", 1, false);
	auto auth = std::any_cast<PackedByteArray>(connection["auth_blob"]);
	assert(auth.size() == 3 && auth[0] == 'a');
	auth_written = 0; assert(s.initiateGameConnection(1, "", 1, false).empty());
	PackedByteArray ticket(1); s.beginAuthSession(ticket, -1, 1); s.beginAuthSession(ticket, 2, 1);
	assert(auth_calls == 0); s.beginAuthSession(ticket, 1, 1); assert(auth_calls == 1);
	ugc_available = false; assert(s.getDownloadedItems(5).empty()); assert(download_calls == 0); ugc_available = true;
	assert(s.getDownloadedItems(5)[0] == 123);
	auto children = std::any_cast<Array>(s.getQueryUGCChildren(1, 0, 2)["children"]);
	assert(children.size() == 2 && std::any_cast<uint64_t>(children[1]) == (uint64_t(1) << 54) + 1);
	assert(std::any_cast<Array>(s.getQueryUGCChildren(1, 0, 0)["children"]).empty());
	for (uint32_t count : {0u, 1u, 5u}) {
		descriptor_count = count;
		auto descriptors = std::any_cast<Array>(s.getQueryUGCContentDescriptors(1, 0, 5)["descriptors"]);
		assert(descriptors.size() == count && s.getUserContentDescriptorPreferences(5).size() == count);
		if (count) assert(std::any_cast<int>(descriptors[0]) == 3);
	}
	PackedInt64Array ids{5, 6};
	for (bool success : {true, false}) {
		setter_success = success; int before = array_allocations;
		assert(s.setItemsDisabledLocally(ids, false) == success);
		assert(s.setSubscriptionsLoadOrder(ids) == success); assert(array_allocations == before);
	}
	auto versions = s.getSupportedGameVersionData(1, 0, 0);
	assert(versions.count("min") && versions.count("max"));
	assert(s.getGlobalStatIntHistory("stat") == PackedInt64Array({123, 456}));
	assert(s.getGlobalStatFloatHistory("stat") == PackedFloat64Array({1.5}));
	auto image = s.getImageSize(0); assert(!std::any_cast<bool>(image["success"]));
	assert(std::any_cast<uint32_t>(image["width"]) == 0 && std::any_cast<uint32_t>(image["height"]) == 0);
	GamepadTextInputDismissed_t input{true, 480}; s.gamepad_text_input_dismissed(&input);
	assert(std::any_cast<bool>(signals["gamepad_text_input_dismissed"][0]));
	assert(std::any_cast<String>(signals["gamepad_text_input_dismissed"][1]).size() == text_length);
	text_success = false; s.gamepad_text_input_dismissed(&input);
	assert(!std::any_cast<bool>(signals["gamepad_text_input_dismissed"][0]));
	assert(std::any_cast<String>(signals["gamepad_text_input_dismissed"][1]).empty());
	int previous_calls = text_calls; input.m_bSubmitted = false; s.gamepad_text_input_dismissed(&input); assert(text_calls == previous_calls);
	input.m_bSubmitted = true; text_length = UINT32_MAX; s.gamepad_text_input_dismissed(&input); assert(text_calls == previous_calls);
	LeaderboardScoresDownloaded_t board{1, 2, 3}; s.leaderboard_scores_downloaded(&board, false);
	for (size_t i=0; i<3; ++i) {
		auto row = std::any_cast<Dictionary>(s.leaderboard_entries_array[i]);
		auto details = std::any_cast<PackedInt32Array>(row["details"]);
		assert(details.size() == (i == 0 ? 3 : i == 1 ? 1 : 0));
		if (i == 1) assert(details[0] == 10);
	}
	s.sendQueryUGCRequest(1); s.sendQueryUGCRequest(2); assert(s.callResultsUGCQueryCompleted.size() == 2);
	int previous_sends = send_calls; s.sendQueryUGCRequest(1); assert(send_calls == previous_sends);
	SteamUGCQueryCompleted_t query{1, 1, 0, 0, false, ""};
	s.callResultsUGCQueryCompleted[1].active = false; s.ugc_query_completed(&query, false);
	assert(s.callResultsUGCQueryCompleted.size() == 1 && s.callResultsUGCQueryCompleted.at(2).IsActive());
	s.callResultsUGCQueryCompleted[2].active = false; s.ugc_query_completed(nullptr, true); assert(s.callResultsUGCQueryCompleted.empty());
	s.sendQueryUGCRequest(3); assert(s.releaseQueryUGCRequest(3) && s.callResultsUGCQueryCompleted.empty());
	next_call = 0; s.sendQueryUGCRequest(4); assert(s.callResultsUGCQueryCompleted.empty());
	std::puts("PASS: user, UGC, statistics, and utility wrapper contracts");
}
