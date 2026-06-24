//================================================================================================//
// GodotSteam - godotsteam_project_settings.cpp
//================================================================================================//
//
// Copyright (c) 2015-Current | GP Garcia, Chris Ridenour, and Contributors
//
// View all contributors at https://godotsteam.com/contribute/contributors/
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//================================================================================================//


#include "godotsteam_project_settings.h"


namespace godot {

void SteamProjectSettings::register_settings() {
	// Set up our app types and their IDs
	if (!ProjectSettings::get_singleton()->has_setting("steam/initialization/app_data/app_type")) {
		ProjectSettings::get_singleton()->set("steam/initialization/app_data/app_type", 0);
	}
	Dictionary property_info;
	property_info["name"] = "steam/initialization/app_data/app_type";
	property_info["type"] = Variant::INT;
	property_info["hint"] = PROPERTY_HINT_ENUM;
	property_info["hint_string"] = "App / Game, Demo, Playtest, Tool";
	ProjectSettings::get_singleton()->add_property_info(property_info);
	ProjectSettings::get_singleton()->set_initial_value("steam/initialization/app_data/app_type", true);
	ProjectSettings::get_singleton()->set_as_basic("steam/initialization/app_data/app_type", true);

	if (!ProjectSettings::get_singleton()->has_setting("steam/initialization/app_data/app_id")) {
		ProjectSettings::get_singleton()->set("steam/initialization/app_data/app_id", 0);
	}
	ProjectSettings::get_singleton()->set_initial_value("steam/initialization/app_data/app_id", true);
	ProjectSettings::get_singleton()->set_as_basic("steam/initialization/app_data/app_id", true);

	if (!ProjectSettings::get_singleton()->has_setting("steam/initialization/app_data/demo_id")) {
		ProjectSettings::get_singleton()->set("steam/initialization/app_data/demo_id", 0);
	}
	ProjectSettings::get_singleton()->set_initial_value("steam/initialization/app_data/demo_id", true);
	ProjectSettings::get_singleton()->set_as_basic("steam/initialization/app_data/demo_id", true);

	if (!ProjectSettings::get_singleton()->has_setting("steam/initialization/app_data/playtest_id")) {
		ProjectSettings::get_singleton()->set("steam/initialization/app_data/playtest_id", 0);
	}
	ProjectSettings::get_singleton()->set_initial_value("steam/initialization/app_data/playtest_id", true);
	ProjectSettings::get_singleton()->set_as_basic("steam/initialization/app_data/playtest_id", true);

	if (!ProjectSettings::get_singleton()->has_setting("steam/initialization/app_data/tool_id")) {
		ProjectSettings::get_singleton()->set("steam/initialization/app_data/tool_id", 0);
	}
	ProjectSettings::get_singleton()->set_initial_value("steam/initialization/app_data/tool_id", true);
	ProjectSettings::get_singleton()->set_as_basic("steam/initialization/app_data/tool_id", true);

	// Set up our initialization process options
	if (!ProjectSettings::get_singleton()->has_setting("steam/initialization/processes/initialize_on_startup")) {
		ProjectSettings::get_singleton()->set("steam/initialization/processes/initialize_on_startup", false);
	}
	ProjectSettings::get_singleton()->set_initial_value("steam/initialization/processes/initialize_on_startup", true);
	ProjectSettings::get_singleton()->set_as_basic("steam/initialization/processes/initialize_on_startup", true);

	if (!ProjectSettings::get_singleton()->has_setting("steam/initialization/processes/embed_callbacks")) {
		ProjectSettings::get_singleton()->set("steam/initialization/processes/embed_callbacks", false);
	}
	ProjectSettings::get_singleton()->set_initial_value("steam/initialization/processes/embed_callbacks", true);
	ProjectSettings::get_singleton()->set_as_basic("steam/initialization/processes/embed_callbacks", true);

	// Set up our MultiplayerPeer stuff
	if (!ProjectSettings::get_singleton()->has_setting("steam/multiplayer_peer/max_channels")) {
		ProjectSettings::get_singleton()->set("steam/multiplayer_peer/max_channels", 4);
	}
	ProjectSettings::get_singleton()->set_initial_value("steam/multiplayer_peer/max_channels", true);
	ProjectSettings::get_singleton()->set_as_basic("steam/multiplayer_peer/max_channels", true);

	// Port in any pre-GodotSteam v4.20 settings
	if (ProjectSettings::get_singleton()->has_setting("steam/initialization/app_id")) {
		WARN_PRINT_ONCE("Found older app ID project setting, converting it");
		ProjectSettings::get_singleton()->set_setting("steam/initialization/app_data/app_id", ProjectSettings::get_singleton()->get_setting("steam/initialization/app_id"));
		ProjectSettings::get_singleton()->clear("steam/initialization/app_id");
	}
	if (ProjectSettings::get_singleton()->has_setting("steam/initialization/initialize_on_startup")) {
		WARN_PRINT_ONCE("Found older initialize on startup setting, converting it");
		ProjectSettings::get_singleton()->set_setting("steam/initialization/processes/initialize_on_startup", ProjectSettings::get_singleton()->get_setting("steam/initialization/initialize_on_startup"));
		ProjectSettings::get_singleton()->clear("steam/initialization/initialize_on_startup");
	}
	if (ProjectSettings::get_singleton()->has_setting("steam/initialization/embed_callbacks")) {
		WARN_PRINT_ONCE("Found older embed callbacks setting, converting it");
		ProjectSettings::get_singleton()->set_setting("steam/initialization/processes/embed_callbacks", ProjectSettings::get_singleton()->get_setting("steam/initialization/embed_callbacks"));
		ProjectSettings::get_singleton()->clear("steam/initialization/embed_callbacks");
	}
}


int SteamProjectSettings::get_app_id() {
	return ProjectSettings::get_singleton()->get_setting_with_override("steam/initialization/app_data/app_id");
}


int SteamProjectSettings::get_app_type() {
	return ProjectSettings::get_singleton()->get_setting_with_override("steam/initialization/app_data/app_type");
}


bool SteamProjectSettings::get_auto_init() {
	return ProjectSettings::get_singleton()->get_setting_with_override("steam/initialization/processes/initialize_on_startup");
}


int SteamProjectSettings::get_demo_id() {
	return ProjectSettings::get_singleton()->get_setting_with_override("steam/initialization/app_data/demo_id");
}


bool SteamProjectSettings::get_embed_callbacks() {
	return ProjectSettings::get_singleton()->get_setting_with_override("steam/initialization/processes/embed_callbacks");
}


int SteamProjectSettings::get_id_in_use() {
	if (get_app_type() == 1) {
		return get_demo_id();
	}
	else if (get_app_type() == 2) {
		return get_playtest_id();
	}
	else if (get_app_type() == 3) {
		return get_tool_id();
	}
	else {
		return get_app_id();
	}
}


int SteamProjectSettings::get_max_channels() {
	return ProjectSettings::get_singleton()->get_setting_with_override("steam/multiplayer_peer/max_channels");
}


int SteamProjectSettings::get_playtest_id() {
	return ProjectSettings::get_singleton()->get_setting_with_override("steam/initialization/app_data/playtest_id");
}


int SteamProjectSettings::get_tool_id() {
	return ProjectSettings::get_singleton()->get_setting_with_override("steam/initialization/app_data/tool_id");
}

}
