// Offline regression test: a manual steamShutdown must not be repeated by the destructor.
#include <cassert>
#include <iostream>

#define ERR_FAIL_COND_MSG(condition, ...) do { if (condition) { return; } } while (false)
#define callable_mp(object, method) 0
static int shutdowns = 0;
static void SteamAPI_Shutdown() { shutdowns++; }
struct RenderingServer {
	static RenderingServer *get_singleton() { static RenderingServer server; return &server; }
	void disconnect(const char *, int) {}
};
struct Steam {
	bool were_callbacks_embedded = false;
	bool is_init_success = false;
	static Steam *singleton;
	void run_internal_callbacks() {}
	void steamShutdown();
	~Steam();
};
Steam *Steam::singleton = nullptr;

// PRODUCTION_LIFECYCLE

int main() {
	{
		Steam steam;
		steam.is_init_success = true;
		steam.steamShutdown();
		assert(shutdowns == 1);
	}
	assert(shutdowns == 1);
	{
		Steam steam;
		steam.is_init_success = true;
	}
	assert(shutdowns == 2);
	std::cout << "NATIVE_LIFECYCLE_TESTS=PASS\n";
}
