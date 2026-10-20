extends Node

var _passed: bool = false


func _init() -> void:
	if Engine.is_editor_hint():
		return
	_passed = Engine.has_singleton("Steam") and Engine.get_singleton("Steam") == Steam
	_passed = _passed and Steam.get_godotsteam_version() == "4.21"
	_passed = _passed and ProjectSettings.has_setting("steam/multiplayer_peer/max_channels")
	var result: Dictionary = Steam.get_steam_init_result()
	var auto_init: bool = ProjectSettings.get_setting("steam/initialization/processes/initialize_on_startup")
	if auto_init:
		_passed = _passed and result.has("status") and result.has("verbal")
		if Steam.isSteamRunning():
			_passed = _passed and int(result.get("status", -1)) == 0
		if int(result.get("status", -1)) == 0:
			Steam.run_callbacks()
			Steam.steamShutdown()
	else:
		_passed = _passed and result.is_empty()
	print("REDOTSTEAM_AUTOLOAD=%s" % ("PASS" if _passed else "FAIL"))


func _ready() -> void:
	if not Engine.is_editor_hint():
		get_tree().quit(0 if _passed else 1)
