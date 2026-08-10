extends Node

@onready var steam: Node = $SteamMock


func _ready() -> void:
	assert(steam.get_godotsteam_version() == "4.21")
	assert(not steam.isSteamRunning())
	var unavailable: Dictionary = steam.steamInitEx(480)
	assert(unavailable.status == 2)
	steam.steam_running = true
	var available: Dictionary = steam.steamInitEx(480)
	assert(available.status == 0)
	steam.run_callbacks()
	assert(steam.callback_runs == 1)
	print("REDOTSTEAM_MOCK_FIXTURE=PASS")
	get_tree().quit(0)
