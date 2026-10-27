//================================================================================================//
// GodotSteam - godotsteam_multiplayer_peer.h
//================================================================================================//
//
// Copyright (c) 2017-Current | Chris Ridenour, Ryan Leverenz, GP Garcia, and Contributors
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
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
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


#ifndef GODOTSTEAM_MULTIPLAYER_PEER_H
#define GODOTSTEAM_MULTIPLAYER_PEER_H


// Include Godot headers
#include <godot_cpp/classes/multiplayer_peer_extension.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/local_vector.hpp>

// Include GodotSteam headers
#include "steam_packet_peer.h"


namespace godot {

class SteamMultiplayerPeer : public MultiplayerPeerExtension {
	GDCLASS(SteamMultiplayerPeer, MultiplayerPeerExtension);


public:
	enum DebugLevel {
		DEBUG_LEVEL_NONE,
		DEBUG_LEVEL_PEER,
		DEBUG_LEVEL_STEAM,
	};


private:
	void _remove_peer(int p_peer_id, bool p_force, bool p_emit);
	void _purge_connection_packets(HSteamNetConnection p_connection);
	HashMap<uint32_t, Ref<SteamPacketPeer>> peers;
	HashMap<HSteamNetConnection, Ref<SteamPacketPeer>> steam_connections;
	HashMap<HSteamNetConnection, uint64_t> pending_deadlines;
	void _expire_pending_connections();
	int _count_pending_connections();
	List<SteamNetworkingMessage_t *> incoming_packets;
	SteamNetworkingMessage_t *current_packet = nullptr;

	int target_peer = 0;
	TransferMode transfer_mode = TRANSFER_MODE_RELIABLE;
	int transfer_channel = 0;

	int unique_id = 0;
	ConnectionStatus connection_status = CONNECTION_DISCONNECTED;
	bool server = false;
	uint64_t expected_host_steam_id = 0;
	int connection_retries = 0;
	HSteamNetConnection host_connection = k_HSteamNetConnection_Invalid;
	int host_virtual_port = 0;

	bool no_nagle = false;
	bool no_delay = false;
	bool server_relay = false;

	HSteamListenSocket listen_socket = k_HSteamListenSocket_Invalid;
	HSteamNetPollGroup poll_group = k_HSteamNetPollGroup_Invalid;
	uint64_t tracked_lobby = 0;

	DebugLevel debug_level = DEBUG_LEVEL_NONE;

	STEAM_CALLBACK(SteamMultiplayerPeer, network_connection_status_changed,
			SteamNetConnectionStatusChangedCallback_t,
			callback_network_connection_status_changed
			);
	STEAM_CALLBACK(SteamMultiplayerPeer, lobby_chat_update, LobbyChatUpdate_t,
			callback_lobby_chat_update
			);


public:
	SteamMultiplayerPeer();
	~SteamMultiplayerPeer();

	void _set_target_peer(int p_peer_id) override;
	int _get_packet_peer() const override;
	TransferMode _get_packet_mode() const override;
	int _get_packet_channel() const override;
	void _set_transfer_channel(int p_channel) override;
	int _get_transfer_channel() const override;
	void _set_transfer_mode(MultiplayerPeer::TransferMode p_mode) override;
	MultiplayerPeer::TransferMode _get_transfer_mode() const override;
	void _disconnect_peer(int p_peer_id, bool p_force = false) override;
	bool _is_server() const override;
	bool _is_server_relay_supported() const override;
	void _poll() override;
	void _close() override;
	int _get_unique_id() const override;
	ConnectionStatus _get_connection_status() const override;

	int _get_available_packet_count() const override;
	Error _get_packet(const uint8_t **r_buffer, int *r_buffer_size) override;
	Error _put_packet(const uint8_t *p_buffer, int p_buffer_size) override;
	int _get_max_packet_size() const override;

	// Low level functions for more controlled connections
	Error create_host(int p_virtual_port = 0);
	Error create_client(uint64_t p_host_steam_id, int p_virtual_port = 0);
	Error add_peer(uint64_t p_steam_id, int p_virtual_port = 0);
	Ref<SteamPacketPeer> get_peer(int p_peer_id);

	// Helper lobby mode
	Error host_with_lobby(uint64_t p_lobby_id);
	Error connect_to_lobby(uint64_t p_lobby_id);

	// Helper functions
	uint64_t get_steam_id_for_peer_id(int p_peer_id);
	int get_peer_id_for_steam_id(uint64_t p_steam_id);

	void set_no_nagle(const bool p_no_nagle);
	bool get_no_nagle() const;
	void set_no_delay(const bool p_no_delay);
	bool get_no_delay() const;
	void set_server_relay(const bool p_server_relay);
	bool get_server_relay() const;
	void set_debug_level(DebugLevel p_debug_level);
	DebugLevel get_debug_level() const;


protected:
	static void _bind_methods();

	const int _get_steam_packet_flags();
	Error _create_listen_socket(int p_virtual_port);
	Error _create_poll_group();
	void _add_pending_peer(uint64_t p_steam_id, HSteamNetConnection p_connection_handle,
			SteamPacketPeer::PeerState p_peer_state
			);
	void _upgrade_peer(HSteamNetConnection p_connection_handle);
};

}


VARIANT_ENUM_CAST(SteamMultiplayerPeer::DebugLevel)


#endif // GODOTSTEAM_MULTIPLAYER_PEER_H
