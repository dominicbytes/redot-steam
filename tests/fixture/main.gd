extends Node

const EXPECTED_GODOTSTEAM_VERSION := "4.21"

var _failures: Array[String] = []


func _ready() -> void:
	_check(Engine.has_singleton("Steam"), "Steam singleton is registered")
	_check(ClassDB.class_exists("SteamMultiplayerPeer"), "SteamMultiplayerPeer is registered")
	_check(ClassDB.class_exists("SteamPacketPeer"), "SteamPacketPeer is registered")

	if Engine.has_singleton("Steam"):
		var steam: Object = Engine.get_singleton("Steam")
		_check(steam.has_method("get_godotsteam_version"), "version method is bound")
		_check(steam.has_method("steamInitEx"), "Steamworks initialization method is bound")
		_check(steam.has_method("run_callbacks"), "callback pump is bound")
		_check(steam.has_signal("steam_server_connected"), "representative callback signal is bound")
		_check(steam.call("get_godotsteam_version") == EXPECTED_GODOTSTEAM_VERSION, "GodotSteam API version is 4.21")
		var running: bool = bool(steam.call("isSteamRunning"))
		print("REDOTSTEAM_STEAM_RUNNING=%s" % running)
		if running:
			var init_result: Dictionary = steam.call("steamInitEx", 480, false)
			_check(init_result.has("status"), "Steam initialization result includes status")
			_check(init_result.has("verbal"), "Steam initialization result includes explanation")
			print("REDOTSTEAM_INIT=%s" % JSON.stringify(init_result))
			_check(int(init_result.get("status", -1)) == 0, "Steam App ID 480 initializes when the client is running")
			if int(init_result.get("status", -1)) == 0:
				steam.call("steamShutdown")
		else:
			print("REDOTSTEAM_INIT=SKIPPED_NO_STEAM_CLIENT")

	var report := {
		"engine": Engine.get_version_info().get("string", "unknown"),
		"expected_godotsteam": EXPECTED_GODOTSTEAM_VERSION,
		"failure_count": _failures.size(),
		"failures": _failures,
	}
	print("REDOTSTEAM_FIXTURE=%s" % JSON.stringify(report))
	get_tree().quit(0 if _failures.is_empty() else 1)


func _check(condition: bool, label: String) -> void:
	if condition:
		print("PASS: %s" % label)
	else:
		_failures.append(label)
		push_error("FAIL: %s" % label)
