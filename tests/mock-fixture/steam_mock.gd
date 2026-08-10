class_name RedotSteamMock
extends Node

signal steam_server_connected
signal steam_server_disconnected

var steam_running := false
var initialized := false
var callback_runs := 0


func get_godotsteam_version() -> String:
	return "4.21"


func isSteamRunning() -> bool:
	return steam_running


func steamInitEx(_app_id: int = 0, _embed_callbacks: bool = false) -> Dictionary:
	initialized = steam_running
	return {
		"status": 0 if initialized else 2,
		"verbal": "OK" if initialized else "Steam client unavailable",
	}


func run_callbacks() -> void:
	callback_runs += 1
