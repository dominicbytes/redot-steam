@tool
extends EditorPlugin

const EDITOR_PANEL = preload("uid://cyniebd6yahu5")

var link_changelog: String = "[url=https://godotsteam.com/changelog/gdextension/]changelog[/url]"
var link_website: String = "[url=https://godotsteam.com]website[/url]"
var steamworks_dock: Control


func _enable_plugin() -> void:
	pass


func _disable_plugin() -> void:
	pass


func _enter_tree() -> void:
	print_rich("GodotSteam v%s | %s | %s" % [Steam.get_godotsteam_version(), link_changelog, link_website])
	add_project_settings()
	add_steamworks_dock()


func _exit_tree() -> void:
	remove_steamworks_dock()


func _make_visible(visible: bool) -> void:
	if steamworks_dock:
		steamworks_dock.set_visible(visible)


#region Add and remove things
func add_project_settings() -> void:
	# Used for the Updater looking for redist files and SteamCMD
	if not ProjectSettings.has_setting("steam/settings/godotsteam/check_for_updates"):
		ProjectSettings.set_setting("steam/settings/godotsteam/check_for_updates", true)
	ProjectSettings.add_property_info({
		"name": "steam/settings/godotsteam/check_for_updates",
		"type": TYPE_BOOL
	})
	ProjectSettings.set_initial_value("steam/settings/godotsteam/check_for_updates", true)
	ProjectSettings.set_as_basic("steam/settings/godotsteam/check_for_updates", true)
	# Which channel of updates to pull from
	if not ProjectSettings.has_setting("steam/settings/godotsteam/update_channel"):
		ProjectSettings.set_setting("steam/settings/godotsteam/update_channel", 0)
	ProjectSettings.add_property_info({
		"name": "steam/settings/godotsteam/update_channel",
		"type": TYPE_INT,
		"hint": PROPERTY_HINT_ENUM,
		"hint_string": "Community, Sponsors"
	})
	ProjectSettings.set_initial_value("steam/settings/godotsteam/update_channel", 0)
	ProjectSettings.set_as_basic("steam/settings/godotsteam/update_channel", true)


# Use different methods based on Godot version
func add_steamworks_dock() -> void:
	steamworks_dock = EDITOR_PANEL.instantiate()
	add_control_to_dock(DOCK_SLOT_LEFT_BR, steamworks_dock)


func remove_steamworks_dock() -> void:
	remove_control_from_docks(steamworks_dock)
	steamworks_dock.queue_free()
	steamworks_dock = null
#endregion
