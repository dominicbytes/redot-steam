@tool
extends EditorPlugin

const EDITOR_PANEL = preload("uid://cyniebd6yahu5")

var editor_panel_instance: Control
var link_changelog: String = "[url=https://godotsteam.com/changelog/gdextension/]changelog[/url]"
var link_website: String = "[url=https://godotsteam.com]website[/url]"
var steamworks_dock: EditorDock


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


func _make_visible(visible) -> void:
	if editor_panel_instance:
		editor_panel_instance.set_visible(visible)


#region Add and remove things
func add_project_settings() -> void:
	# Used for the Updater looking for redist files and SteamCMD
	if not ProjectSettings.has_setting("steam/settings/check_for_updates"):
		ProjectSettings.set_setting("steam/settings/check_for_updates", true)
	ProjectSettings.add_property_info({
		"name": "steam/settings/check_for_updates",
		"type": TYPE_BOOL
	})
	ProjectSettings.set_initial_value("steam/settings/check_for_updates", true)
	ProjectSettings.set_as_basic("steam/settings/check_for_updates", true)
	# Which channel of updates to pull from
	if not ProjectSettings.has_setting("steam/settings/update_channel"):
		ProjectSettings.set_setting("steam/settings/update_channel", 0)
	ProjectSettings.add_property_info({
		"name": "steam/settings/update_channel",
		"type": TYPE_INT,
		"hint": PROPERTY_HINT_ENUM,
		"hint_string": "Community, Sponsors"
	})
	ProjectSettings.set_initial_value("steam/settings/update_channel", 0)
	ProjectSettings.set_as_basic("steam/settings/update_channel", true)


func add_steamworks_dock() -> void:
	steamworks_dock = EditorDock.new()
	steamworks_dock.title = "Steamworks"
	steamworks_dock.dock_icon = preload("uid://dhn3vkdxvetbn")
	steamworks_dock.default_slot = EditorDock.DOCK_SLOT_LEFT_BR
	var dock_content = EDITOR_PANEL.instantiate()
	steamworks_dock.add_child(dock_content)
	add_dock(steamworks_dock)


func remove_steamworks_dock() -> void:
	remove_dock(steamworks_dock)
	steamworks_dock.queue_free()
	steamworks_dock = null
#endregion
