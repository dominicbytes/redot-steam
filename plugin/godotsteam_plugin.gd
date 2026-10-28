@tool
class_name GodotSteamPlugin
extends EditorPlugin

const EDITOR_PANEL = preload("editor/steamworks_panel.tscn")

static var dock_frame: Control

var link_changelog: String = "[url=https://godotsteam.com/changelog/gdextension/]upstream changelog[/url]"
var link_website: String = "[url=https://github.com/dominicbytes/redot-steam]RedotSteam[/url]"
var steamworks_dock: Control


## Used specifically to add/remove additional dock content
static func get_dock_frame() -> Control:
	return dock_frame


func _enable_plugin() -> void:
	print("RedotSteam editor integration enabled")


func _disable_plugin() -> void:
	print("RedotSteam editor integration disabled")


func _enter_tree() -> void:
	if not ClassDB.class_exists("Steam") or not Engine.has_singleton("Steam"):
		return
	var steam: Object = Engine.get_singleton("Steam")
	if not is_instance_valid(steam) or not steam.has_method("get_godotsteam_version"):
		return
	print_rich("RedotSteam (GodotSteam API v%s) | %s | %s" % [steam.call("get_godotsteam_version"), link_website, link_changelog])
	add_project_settings()
	add_steamworks_dock()


func _exit_tree() -> void:
	remove_steamworks_dock()


func _make_visible(visible: bool) -> void:
	if is_instance_valid(steamworks_dock):
		steamworks_dock.set_visible(visible)


#region Add and remove things
func add_project_settings() -> void:
	# Network update checks are opt-in. Releases never bundle Valve SDK files.
	if not ProjectSettings.has_setting("steam/updates/redotsteam/check_for_updates"):
		ProjectSettings.set_setting("steam/updates/redotsteam/check_for_updates", false)
	ProjectSettings.add_property_info({
		"name": "steam/updates/redotsteam/check_for_updates",
		"type": TYPE_BOOL
	})
	ProjectSettings.set_initial_value("steam/updates/redotsteam/check_for_updates", false)
	ProjectSettings.set_as_basic("steam/updates/redotsteam/check_for_updates", true)


func add_steamworks_dock() -> void:
	steamworks_dock = EDITOR_PANEL.instantiate()
	# This will be used when 4.4.x is deprecated
	#add_control_to_dock(DockSlot.DOCK_SLOT_BOTTOM, steamworks_dock)
	# This is deprecated as of 4.6; when it is removed then 4.4.x will be deprecated for GodotSteam
	add_control_to_bottom_panel(steamworks_dock, "Steamworks")
	dock_frame = steamworks_dock


func remove_steamworks_dock() -> void:
	# This will be used when 4.4.x is deprecated
	#remove_control_from_docks(steamworks_dock)
	# This is deprecated as of 4.6; when it is removed then 4.4.x will be deprecated for GodotSteam
	if is_instance_valid(steamworks_dock):
		remove_control_from_bottom_panel(steamworks_dock)
		steamworks_dock.queue_free()
	steamworks_dock = null
	dock_frame = null
#endregion
