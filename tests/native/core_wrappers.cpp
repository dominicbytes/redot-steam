// Production wrappers compiled with offline doubles; no Steam runtime is loaded.
#include <any>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define ERR_FAIL_COND_MSG(condition, message) do { if (condition) return; } while (0)
#define ERR_FAIL_COND_V_MSG(condition, value, message) do { if (condition) return value; } while (0)
using uint64 = uint64_t;
using int32 = int32_t;
using FriendsGroupID_t = int16_t;
using SteamInventoryResult_t = int32_t;
using SteamItemDef_t = int32_t;
using SteamItemInstanceID_t = uint64_t;
using SteamAPICall_t = uint64_t;
using InputHandle_t = uint64_t;
using EControllerHapticLocation = int;
using ESteamNetworkingConfigValue = int;
using ControllerHapticLocation = int;
struct Object {};

struct CharString {
	static std::map<const char *, std::weak_ptr<std::vector<char>>> allocations;
	std::shared_ptr<std::vector<char>> data;
	CharString() = default;
	explicit CharString(const std::string &text) : data(std::make_shared<std::vector<char>>(text.begin(), text.end())) {
		data->push_back(0);
		allocations[data->data()] = data;
	}
	const char *get_data() const { return data ? data->data() : ""; }
	char *ptrw() { return data ? data->data() : nullptr; }
	int size() const { return data ? int(data->size()) : 0; }
	int length() const { return data ? int(data->size()) - 1 : 0; }
	static bool alive(const char *p) { return allocations.count(p) && !allocations[p].expired(); }
};
std::map<const char *, std::weak_ptr<std::vector<char>>> CharString::allocations;
struct Variant {
	enum Type { INT, FLOAT, STRING, OBJECT };
	Type type = INT;
	int64_t number = 0;
	double real = 0;
	std::string text;
	Variant() = default;
	template<class T> Variant(T value) : number(int64_t(value)) {}
	Variant(double value) : type(FLOAT), real(value) {}
	Variant(const char *value) : type(STRING), text(value) {}
	Variant(const std::string &value) : type(STRING), text(value) {}
	Type get_type() const { return type; }
	operator int64_t() const { return number; }
	operator Object *() const { return nullptr; }
};
struct String {
	std::string text;
	String() = default;
	String(const char *s) : text(s) {}
	String(const Variant &v) : text(v.text) {}
	CharString utf8() const { return CharString(text); }
	bool is_valid_int() const {
		size_t i = !text.empty() && (text[0] == '-' || text[0] == '+') ? 1 : 0;
		if (i == text.size()) return false;
		for (; i < text.size(); i++) if (text[i] < '0' || text[i] > '9') return false;
		return true;
	}
	int64_t to_int() const { try { return std::stoll(text); } catch (...) { return text[0] == '-' ? INT64_MIN : INT64_MAX; } }
};
struct Dictionary;
struct Array {
	std::vector<Variant> values;
	void append(const Variant &v) { values.push_back(v); }
	void append(const Dictionary &) { values.emplace_back(0); }
	Variant operator[](size_t i) const { return values.at(i); }
	size_t size() const { return values.size(); }
};
struct Dictionary {
	std::map<std::string, Variant> fields;
	std::map<int, Variant> options;
	Variant &operator[](const char *key) { return fields[key]; }
	Variant &operator[](int key) { return options[key]; }
	int size() const { return int(options.size()); }
	Array keys() const { Array a; for (auto &p : options) a.append(p.first); return a; }
};
template<class T> struct PackedArray {
	std::vector<T> values;
	PackedArray(std::initializer_list<T> init) : values(init) {}
	PackedArray() = default;
	int64_t size() const { return int64_t(values.size()); }
	bool is_empty() const { return values.empty(); }
	const T *ptr() const { return values.data(); }
	const T &operator[](int64_t i) const { return values.at(size_t(i)); }
};
using PackedInt64Array = PackedArray<int64_t>;
using PackedInt32Array = PackedArray<int32_t>;
using PackedStringArray = PackedArray<String>;
struct CSteamID {
	uint64_t value = 0;
	CSteamID() = default;
	CSteamID(uint64_t v) : value(v) {}
	uint64_t ConvertToUint64() const { return value; }
	bool IsValid() const { return value != 0; }
};
struct FriendGameInfo_t {
	struct Game { int AppID() const { return 480; } } m_gameID;
	uint32_t m_unGameIP = 123;
	uint16_t m_usGamePort = 27015, m_usQueryPort = 27016;
	CSteamID m_steamIDLobby;
};
struct SteamItemDetails_t { uint64_t m_itemId = 91; int m_iDefinition = 7; uint16_t m_unQuantity = 2, m_unFlags = 0; };
struct HTML_BrowserRestarted_t { uint32_t unBrowserHandle, unOldBrowserHandle; };
struct HTML_FileOpenDialog_t { uint32_t unBrowserHandle; const char *pchTitle; const char *pchInitialFile; };
struct SteamNetworkingConfigValue_t { int key = 0; int64_t value = 0; const char *text = nullptr; };
void SteamAPI_SteamNetworkingConfigValue_t_SetInt64(SteamNetworkingConfigValue_t *v, int k, int64_t n) { v->key=k; v->value=n; }
void SteamAPI_SteamNetworkingConfigValue_t_SetInt32(SteamNetworkingConfigValue_t *v, int k, int n) { v->key=k; v->value=n; }
void SteamAPI_SteamNetworkingConfigValue_t_SetFloat(SteamNetworkingConfigValue_t *v, int k, Variant n) { v->key=k; v->value=int64_t(n.real); }
void SteamAPI_SteamNetworkingConfigValue_t_SetString(SteamNetworkingConfigValue_t *v, int k, const char *p) { v->key=k; v->text=p; }
void SteamAPI_SteamNetworkingConfigValue_t_SetPtr(SteamNetworkingConfigValue_t *v, int k, Object *) { v->key=k; }

int service = 1, native_calls = 0, result_queries = 0, haptic_calls = 0, html_responses = 0;
std::vector<uint64_t> clan_ids;
std::vector<int32_t> item_defs;
std::vector<uint32_t> quantities;
std::vector<std::string> selected;
uint64_t requested_clan = 0;
int requested_user = 0, gain_seen = 0, other_gain_seen = 0;
uint32_t response_browser = 0, body_size = 0;
bool cancelled = false;
int *SteamAPI_SteamFriends() { return &service; }
int *SteamAPI_SteamInventory() { return &service; }
int *SteamAPI_SteamHTTP() { return &service; }
int *SteamAPI_SteamInput() { return &service; }
int *SteamAPI_SteamHTMLSurface() { return &service; }
int *SteamAPI_SteamNetworkingSockets_SteamAPI() { return &service; }
void SteamAPI_ISteamFriends_DownloadClanActivityCounts(int *, CSteamID *ids, int n) {
	clan_ids.clear(); for (int i=0; i<n; i++) clan_ids.push_back(ids[i].value);
}
uint64_t SteamAPI_ISteamFriends_GetChatMemberByIndex(int *, uint64_t clan, int user) { requested_clan=clan; requested_user=user; return 99; }
bool SteamAPI_ISteamFriends_GetFriendGamePlayed(int *, uint64_t, FriendGameInfo_t *info) { *info=FriendGameInfo_t(); return true; }
void SteamAPI_ISteamFriends_GetFriendsGroupMembersList(int *, FriendsGroupID_t, CSteamID *ids, int n) {
	native_calls++; for (int i=0; i<n; i++) ids[i]=CSteamID(100+i);
}
bool SteamAPI_ISteamInventory_ExchangeItems(int *, int32_t *handle, const SteamItemDef_t *, const uint32_t *, uint32_t out_n, const SteamItemInstanceID_t *, const uint32_t *, uint32_t in_n) { native_calls++; assert(out_n==2 && in_n==1); *handle=8; return true; }
bool SteamAPI_ISteamInventory_GenerateItems(int *, int32_t *handle, const SteamItemDef_t *items, const uint32_t *qty, uint32_t n) { native_calls++; item_defs.assign(items,items+n); quantities.assign(qty,qty+n); *handle=9; return true; }
SteamAPICall_t SteamAPI_ISteamInventory_StartPurchase(int *, const SteamItemDef_t *items, const uint32_t *qty, uint32_t n) { native_calls++; item_defs.assign(items,items+n); quantities.assign(qty,qty+n); return 10; }
bool SteamAPI_ISteamInventory_GetResultItems(int *, int32_t handle, SteamItemDetails_t *items, uint32_t *size) {
	result_queries++; assert(handle==7); if (items) { assert(*size==1); items[0]=SteamItemDetails_t(); } *size=1; return true;
}
bool SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody(int *, uint32_t, const char *, uint8_t *body, uint32_t n) { body_size=n; if (n) assert(body[n-1]!='\0'); return true; }
void SteamAPI_ISteamInput_TriggerSimpleHapticEvent(int *, uint64_t, int, uint8_t, char gain, uint8_t, char other) { haptic_calls++; gain_seen=int8_t(gain); other_gain_seen=int8_t(other); }
void SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse(int *, uint32_t browser, const char **files) {
	html_responses++; response_browser=browser; cancelled=files==nullptr; selected.clear();
	if (files) for (int i=0; files[i]; i++) { assert(i<3); assert(CharString::alive(files[i])); selected.emplace_back(files[i]); }
}
uint32_t consume_options(int n, const SteamNetworkingConfigValue_t *options) {
	native_calls++; assert(n==0 || n==4);
	for (int i=0; i<n; i++) if (options[i].text) { assert(CharString::alive(options[i].text)); assert(std::strlen(options[i].text)>80); }
	return 42;
}
uint32_t SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP(int *, int, int n, const SteamNetworkingConfigValue_t *o) { return consume_options(n,o); }
uint32_t SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P(int *, int, int n, const SteamNetworkingConfigValue_t *o) { return consume_options(n,o); }
uint32_t SteamAPI_ISteamNetworkingSockets_ConnectP2P(int *, uint64_t, int, int n, const SteamNetworkingConfigValue_t *o) { return consume_options(n,o); }
uint32_t SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress(int *, int, int n, const SteamNetworkingConfigValue_t *o) { return consume_options(n,o); }
uint32_t SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer(int *, uint64_t, int, int n, const SteamNetworkingConfigValue_t *o) { return consume_options(n,o); }
uint32_t SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP(int *, int, int n, const SteamNetworkingConfigValue_t *o) { return consume_options(n,o); }
struct Steam {
	static constexpr int NETWORKING_CONFIG_CONNECTION_USER_DATA = 1;
	uint32_t current_browser_handle=0;
	uint64_t current_clan_id=999;
	int32_t inventory_handle=7;
	struct CallResult { template<class T> void Set(SteamAPICall_t, Steam *, T) {} } callResultStartPurchase;
	void inventory_start_purchase_result() {}
	int getSteamIPFromString(String) { return 1; }
	uint64_t getIdentityFromSteamID(uint64_t id) { return id; }
	std::string getStringFromIP(uint32_t) { return "127.0.0.1"; }
	template<class... T> void emit_signal(const char *, T...) {}
	// PRODUCTION_DECLARATIONS
};
// PRODUCTION_METHODS

int main() {
	Steam steam;
	steam.downloadClanActivityCounts({}); assert(clan_ids.empty());
	steam.downloadClanActivityCounts({1}); assert(clan_ids==std::vector<uint64_t>{1});
	PackedInt64Array many; for (int i=0;i<24;i++) many.values.push_back(i+1);
	steam.downloadClanActivityCounts(many); assert(clan_ids.size()==24 && clan_ids.back()==24);
	native_calls=0;
	assert(steam.getFriendsGroupMembersList(1,-1).size()==0 && native_calls==0);
	assert(steam.getFriendsGroupMembersList(1,0).size()==0);
	Array friends=steam.getFriendsGroupMembersList(1,3); assert(friends.size()==3 && friends[0].number==100 && friends[2].number==102);
	assert(steam.getChatMemberByIndex(123,4)==99 && requested_clan==123 && requested_user==4);
	Dictionary game=steam.getFriendGamePlayed(1); assert(game["lobby"].number==0 && game["ip"].text=="127.0.0.1" && game["game_port"].number==27015 && game["query_port"].number==27016);
	native_calls=0;
	assert(steam.exchangeItems({1,2},{1},{3},{1})==0);
	assert(steam.exchangeItems({1,2},{1,2},{3},{})==0);
	assert(steam.generateItems({1,2},{1})==0);
	steam.startPurchase({1,2},{1}); assert(native_calls==0);
	assert(steam.exchangeItems({1,2},{1,2},{3},{1})==8);
	assert(steam.generateItems({1,2},{3,4})==9 && quantities==std::vector<uint32_t>({3,4}));
	steam.startPurchase({5,6},{7,8}); assert(item_defs==std::vector<int32_t>({5,6}) && quantities==std::vector<uint32_t>({7,8}));
	steam.inventory_handle=7; assert(steam.getResultItems(0).size()==1 && result_queries==2);
	assert(steam.getResultItems(7).size()==1 && result_queries==4);
	assert(steam.setHTTPRequestRawPostBody(1,"text/plain","") && body_size==0);
	assert(steam.setHTTPRequestRawPostBody(1,"application/json","{\"ok\":true}") && body_size==11);
	steam.triggerSimpleHapticEvent(1,0,1,"-6",1,"0"); assert(haptic_calls==1 && gain_seen==-6 && other_gain_seen==0);
	steam.triggerSimpleHapticEvent(1,0,1,"-128",1,"127"); assert(haptic_calls==2 && gain_seen==-128 && other_gain_seen==127);
	for (auto bad : {"", "x", "1.5", "128", "-129", "99999999999999999999999"}) {
		steam.triggerSimpleHapticEvent(1,0,1,bad,1,"0"); steam.triggerSimpleHapticEvent(1,0,1,"0",1,bad);
	}
	assert(haptic_calls==2);
	steam.current_browser_handle=5;
	HTML_BrowserRestarted_t restarted{9,5}; steam.html_browser_restarted(&restarted); assert(steam.current_browser_handle==9);
	restarted={12,3}; steam.html_browser_restarted(&restarted); assert(steam.current_browser_handle==9);
	HTML_FileOpenDialog_t dialog{4,"Select","hint.txt"}; steam.html_file_open_dialog(&dialog); assert(html_responses==0);
	steam.fileLoadDialogResponse({"chosen.txt","second.txt"},4); assert(response_browser==4 && !cancelled && selected==std::vector<std::string>({"chosen.txt","second.txt"}));
	steam.fileLoadDialogResponse({},0); assert(response_browser==4 && cancelled && html_responses==2);
	Dictionary config;
	config[1]=int64_t(1)<<40; config[2]=7;
	config[3]=std::string(100,'a'); config[4]=std::string(120,'b');
	for (int i=0;i<40;i++) {
		assert(steam.createListenSocketIP("endpoint",config)==42);
		assert(steam.createListenSocketP2P(0,config)==42);
		assert(steam.connectP2P(1,0,config)==42);
		assert(steam.connectByIPAddress("endpoint",config)==42);
		assert(steam.connectToHostedDedicatedServer(1,0,config)==42);
		assert(steam.createListenSocketP2PFakeIP(0,config)==42);
	}
	assert(steam.createListenSocketIP("endpoint",Dictionary())==42);
	for (auto &allocation : CharString::allocations) assert(allocation.second.expired());
	std::cout << "CORE_WRAPPER_TESTS=PASS\n";
}
