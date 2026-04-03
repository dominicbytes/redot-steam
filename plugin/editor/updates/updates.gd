@tool
extends FoldableContainer
###
# Super-huge thanks to Nathan Hoad and Marcus Skov for ideas and inspiration on how to create a
# plug-in updater.
###

const CONFIRMATION = preload("uid://bb6pyk4s77qvo")
const TEMP_FILE = "user://update.zip"
const VERSION_URL: StringName = "https://godotengine.org/asset-library/api/asset/2445"

var confirmation_popup = null
var download_location: String = ""
var downloading_update: bool = false
# This will act as both our version checker and update downloader
var http_request: HTTPRequest = null
var is_actively_downloading: bool = false
var new_version: String = ""

@onready var installed_label: Label = %InstalledLabel
@onready var update_label: Label = %UpdateLabel
@onready var update_button: Button = %UpdateButton


func _ready() -> void:
	set_defaults()
	check_for_updates()


func _process(delta: float) -> void:
	if is_actively_downloading:
		confirmation_popup.update_progress(http_request.get_downloaded_bytes() * 100 / http_request.get_body_size())


#region Setup
func set_defaults() -> void:
	installed_label.text = "Installed version %s" % Steam.get_godotsteam_version()
	update_label.text = ""
	update_button.text = "Up-to-date"
	update_button.disabled = true
	update_button.pressed.connect(_on_update_pressed)
#endregion


#region Checking for updates
func check_for_updates() -> void:
	if not ProjectSettings.get_setting("steam/settings/godotsteam/check_for_updates"):
		return

	http_request = HTTPRequest.new()
	add_child(http_request)
	http_request.request_completed.connect(_on_http_request_completed)
	if http_request.request(VERSION_URL) != OK:
		printerr("Failed to request GodotSteam plug-in remote current version")


func _on_http_request_completed(result: int, _response_code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	http_request.request_completed.disconnect(_on_http_request_completed)
	http_request.queue_free()

	if result != HTTPRequest.RESULT_SUCCESS:
		return
	var response = JSON.parse_string(body.get_string_from_utf8())
	if response == null:
		return

	download_location = response.download_url
	new_version = response.version_string

	if convert_version(new_version) > convert_version(Steam.get_godotsteam_version()):
		print("New GodotSteam version %s available" % new_version)
		update_label.text = "New version %s" % new_version
		update_button.text = "Download"
		update_button.disabled = false
#endregion


#region Updating versions
func install_update() -> void:
	print("Removing older GodotSteam %s plug-in" % Steam.get_godotsteam_version())
	OS.move_to_trash(ProjectSettings.globalize_path("res://addons/godotsteam"))

	update_button.text = "Unpacking"
	print("Unpacking new %s plug-in" % new_version)
	var zip_reader: ZIPReader = ZIPReader.new()
	zip_reader.open(TEMP_FILE)
	var files: PackedStringArray = zip_reader.get_files()

	var base_path := files[1]
	# Remove archive folder
	files.remove_at(0)
	# Remove assets folder
	files.remove_at(0)

	for path in files:
		var new_file_path: String = path.replace(base_path, "")
		if path.ends_with("/"):
			DirAccess.make_dir_recursive_absolute("res://addons/%s" % new_file_path)
		else:
			var file: FileAccess = FileAccess.open("res://addons/%s" % new_file_path, FileAccess.WRITE)
			file.store_buffer(zip_reader.read_file(path))

	zip_reader.close()
	restart_post_update()


func _on_cancel_pressed() -> void:
	if update_button.text == "Downloading":
		print("Canceling %s plug-in update download" % new_version)
		http_request.cancel_request()
		http_request.queue_free()
		is_actively_downloading = false
		update_button.disabled = false
	else:
		print("Canceling %s plug-in update and deleting temporary files" % new_version)
		DirAccess.remove_absolute(TEMP_FILE)
	update_button.text = "Download"
	update_button.disabled = false


func _on_continue_pressed() -> void:
	install_update()


func _on_update_pressed() -> void:
	print("Downloading %s plug-in update, please wait" % new_version)
	is_actively_downloading = true
	update_button.text = "Downloading"
	update_button.disabled = true
	show_download_popup()

	http_request = HTTPRequest.new()
	add_child(http_request)
	http_request.request_completed.connect(_on_download_request_completed)
	if http_request.request(download_location) != OK:
		printerr("Failed to request %s plug-in update download" % new_version)
		is_actively_downloading = false
		update_button.text = "Failed"
		update_button.disabled = false
		return


func _on_download_request_completed(result: int, _response_code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	http_request.request_completed.disconnect(_on_download_request_completed)
	http_request.queue_free()
	is_actively_downloading = false

	if result != HTTPRequest.RESULT_SUCCESS:
		printerr("Failed to download new GodotSteam version %s" % result)
		update_button.disabled = false
		return

	print("Update downloaded, asking for confirmation to install")
	update_button.text = "Confirming"
	confirmation_popup.update_interface(new_version, false)

	var zip_file: FileAccess = FileAccess.open(TEMP_FILE, FileAccess.WRITE)
	zip_file.store_buffer(body)
	zip_file.close()


func restart_post_update() -> void:
	update_button.text = "Restarting"
	DirAccess.remove_absolute(TEMP_FILE)
	print("GodotSteam updated to version %s, restarting the editor" % new_version)
	EditorInterface.restart_editor(true)


func show_download_popup() -> void:
	confirmation_popup = CONFIRMATION.instantiate()
	confirmation_popup.update_interface(new_version, true)
	confirmation_popup.cancel_update.connect(_on_cancel_pressed)
	confirmation_popup.continue_update.connect(_on_continue_pressed)
	EditorInterface.get_editor_main_screen().add_child(confirmation_popup)
	confirmation_popup.visible = true
#endregion


#region Helpers
func convert_version(version_string: String) -> int:
	return int(version_string.replace(".", "").rpad(4, "0"))
#endregion
