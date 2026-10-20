@tool
extends MarginContainer

const RELEASE_API := "https://api.github.com/repos/dominicbytes/redot-steam/releases/latest"
const RELEASE_PAGE := "https://github.com/dominicbytes/redot-steam/releases"

var http_request: HTTPRequest
var release_url := RELEASE_PAGE

@onready var cancel_button: Button = %CancelButton
@onready var downloading: ProgressBar = %Downloading
@onready var install_button: Button = %InstallButton
@onready var installed_label: Label = %InstalledLabel
@onready var update_label: Label = %UpdateLabel
@onready var update_button: Button = %UpdateButton


func _ready() -> void:
	cancel_button.hide()
	downloading.hide()
	install_button.hide()
	installed_label.text = "GodotSteam API %s for Redot 26.2 / 26.3" % Steam.get_godotsteam_version()
	update_button.text = "Releases"
	update_button.disabled = false
	update_button.pressed.connect(_open_release_page)
	if ProjectSettings.get_setting("steam/updates/redotsteam/check_for_updates", false):
		_check_for_updates()
	else:
		update_label.text = "Update checks are disabled"


func _check_for_updates() -> void:
	http_request = HTTPRequest.new()
	add_child(http_request)
	http_request.request_completed.connect(_on_request_completed)
	if http_request.request(RELEASE_API) != OK:
		update_label.text = "Unable to check for RedotSteam updates"


func _on_request_completed(result: int, response_code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	http_request.queue_free()
	if result != HTTPRequest.RESULT_SUCCESS or response_code != 200:
		update_label.text = "No published RedotSteam release found"
		return
	var response: Variant = JSON.parse_string(body.get_string_from_utf8())
	if not response is Dictionary:
		update_label.text = "Invalid RedotSteam release response"
		return
	var release: Dictionary = response
	release_url = str(release.get("html_url", RELEASE_PAGE))
	update_label.text = "Latest: %s" % str(release.get("tag_name", "unknown"))


func _open_release_page() -> void:
	OS.shell_open(release_url)
