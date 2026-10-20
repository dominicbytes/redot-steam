extends SceneTree


func _initialize() -> void:
	# This fixture process has no App ID; it never requests a client launch.
	OS.unset_environment("SteamAppId")
	OS.unset_environment("SteamGameId")
	ProjectSettings.set_setting("steam/initialization/app_data/app_id", 0)
	ProjectSettings.set_setting("steam/initialization/app_data/app_type", 0)
	if not Engine.has_singleton("Steam"):
		push_error("Steam singleton is missing")
		quit(1)
		return
	var steam: Object = Engine.get_singleton("Steam")
	var result: Dictionary = steam.call("steamInitEx", 0, false)
	var failed_cleanly: bool = result.has("status") and int(result.status) != 0 and result.has("verbal")
	for peer_class in ["SteamMultiplayerPeer", "SteamPacketPeer"]:
		var peer: RefCounted = ClassDB.instantiate(peer_class)
		var weak_peer: WeakRef = weakref(peer)
		peer = null
		failed_cleanly = failed_cleanly and weak_peer.get_ref() == null
	print("REDOTSTEAM_MISSING_APP_ID=%s" % JSON.stringify(result))
	var report := {
		"failure_count": 0 if failed_cleanly else 1,
		"failures": [] if failed_cleanly else ["Initialization failure or peer release check failed"],
	}
	print("REDOTSTEAM_INIT_FAILURE=%s" % JSON.stringify(report))
	quit(0 if failed_cleanly else 1)
