//================================================================================================//
// GodotSteam - godotsteam_multiplayer_peer.cpp
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


#include <godot_cpp/core/math.hpp>
#include <godot_cpp/classes/time.hpp>

#include "godotsteam_multiplayer_peer.h"


#define MAX_MESSAGE_COUNT 255


namespace godot {


SteamMultiplayerPeer::SteamMultiplayerPeer() :
	callback_network_connection_status_changed(this,
			&SteamMultiplayerPeer::network_connection_status_changed),
	callback_lobby_chat_update(this,
			&SteamMultiplayerPeer::lobby_chat_update) {
}

SteamMultiplayerPeer::~SteamMultiplayerPeer() {
	_close();
}

void SteamMultiplayerPeer::_set_target_peer(int p_peer_id) {
	target_peer = p_peer_id;
}

int SteamMultiplayerPeer::_get_packet_peer() const {
	ERR_FAIL_COND_V(incoming_packets.is_empty(), 1);
	if (SteamAPI_SteamNetworkingSockets_SteamAPI() == nullptr) {
		return 0;
	}

	return SteamAPI_ISteamNetworkingSockets_GetConnectionUserData(
			SteamAPI_SteamNetworkingSockets_SteamAPI(),
			incoming_packets.front()->get()->m_conn
			);
}

MultiplayerPeer::TransferMode SteamMultiplayerPeer::_get_packet_mode() const {
	ERR_FAIL_COND_V(incoming_packets.is_empty(), TRANSFER_MODE_RELIABLE);
	if (incoming_packets.front()->get()->m_nFlags & k_nSteamNetworkingSend_Reliable) {
		return TRANSFER_MODE_RELIABLE;
	} else {
		return TRANSFER_MODE_UNRELIABLE;
	}
}

int SteamMultiplayerPeer::_get_packet_channel() const {
	ERR_FAIL_COND_V(incoming_packets.is_empty(), 1);
	return incoming_packets.front()->get()->m_idxLane;
}

void SteamMultiplayerPeer::_set_transfer_channel(int p_channel) {
	transfer_channel = p_channel;
}

int SteamMultiplayerPeer::_get_transfer_channel() const {
	return transfer_channel;
}

void SteamMultiplayerPeer::_set_transfer_mode(MultiplayerPeer::TransferMode p_mode) {
	transfer_mode = p_mode;
}

MultiplayerPeer::TransferMode SteamMultiplayerPeer::_get_transfer_mode() const {
	return transfer_mode;
}

void SteamMultiplayerPeer::_disconnect_peer(int p_peer_id, bool p_force) {
	// MultiplayerPeer contract: a forced disconnect does not emit peer_disconnected.
	_remove_peer(p_peer_id, p_force, !p_force);
}

void SteamMultiplayerPeer::_remove_peer(int p_peer_id, bool p_force, bool p_emit) {
	if (!peers.has(p_peer_id)) {
		return;
	}

	Ref<SteamPacketPeer> peer = peers[p_peer_id];
	HSteamNetConnection connection = peer->get_connection_handle();
	steam_connections.erase(connection);
	peers.erase(p_peer_id);
	_purge_connection_packets(connection);
	peer->disconnect_peer(p_force);

	// Complete teardown before a signal handler can start another session.
	if (p_peer_id == 1 && connection_status != CONNECTION_DISCONNECTED) {
		_close();
	}
	if (p_emit) {
		emit_signal("peer_disconnected", p_peer_id);
	}
}

void SteamMultiplayerPeer::_purge_connection_packets(HSteamNetConnection p_connection) {
	// Rotate once through the queue so surviving packets keep their order.
	int count = incoming_packets.size();
	for (int i = 0; i < count; i++) {
		SteamNetworkingMessage_t *message = incoming_packets.front()->get();
		incoming_packets.pop_front();
		if (message->m_conn == p_connection) {
			incoming_bytes -= message->GetSize();
			// Same rule as _close(): never release into a Steam that has shut down.
			if (SteamAPI_SteamNetworkingSockets_SteamAPI() != nullptr) {
				SteamAPI_SteamNetworkingMessage_t_Release(message);
			}
		} else {
			incoming_packets.push_back(message);
		}
	}
}

bool SteamMultiplayerPeer::_is_server() const {
	return server;
}

bool SteamMultiplayerPeer::_is_server_relay_supported() const {
	return server_relay;
}

int SteamMultiplayerPeer::_count_pending_connections() {
	int pending = 0;
	for (KeyValue<HSteamNetConnection, Ref<SteamPacketPeer>> &E : steam_connections) {
		uint32_t peer_id = E.value->get_peer_id();
		if (!peers.has(peer_id) || peers[peer_id]->get_connection_handle() != E.key) {
			pending++;
		}
	}
	return pending;
}

void SteamMultiplayerPeer::_expire_pending_connections() {
	uint64_t now = Time::get_singleton()->get_ticks_msec();
	LocalVector<HSteamNetConnection> expired;
	for (KeyValue<HSteamNetConnection, uint64_t> &E : pending_deadlines) {
		if (now >= E.value) {
			expired.push_back(E.key);
		}
	}
	for (HSteamNetConnection connection : expired) {
		pending_deadlines.erase(connection);
		// The host connection can be erased without _close() (malformed or rejected
		// handshake), and another lobby member may already have marked the client
		// connected. A client whose host never completed the handshake must still give up.
		bool host_admitted = peers.has(1) && peers[1]->get_connection_handle() == connection;
		if (connection == host_connection && !host_admitted) {
			_close();
			return;
		}
		if (!steam_connections.has(connection)) {
			continue;
		}
		Ref<SteamPacketPeer> peer = steam_connections[connection];
		uint32_t peer_id = peer->get_peer_id();
		if (peers.has(peer_id) && peers[peer_id]->get_connection_handle() == connection) {
			continue; // Admitted in time.
		}
		peer->disconnect_peer(true);
		steam_connections.erase(connection);
		if (connection == host_connection) {
			_close();
			return;
		}
	}
}

void SteamMultiplayerPeer::_poll() {
	_expire_pending_connections();
	if (SteamAPI_SteamNetworkingSockets_SteamAPI() == nullptr || poll_group == k_HSteamNetPollGroup_Invalid) {
		return;
	}
	int remaining = SteamProjectSettings::get_max_queued_packets() - incoming_packets.size();
	if (remaining <= 0) {
		return;
	}
	int receive_count = remaining < MAX_MESSAGE_COUNT ? remaining : MAX_MESSAGE_COUNT;
	SteamNetworkingMessage_t *messages[MAX_MESSAGE_COUNT];
	const int64_t max_bytes = SteamProjectSettings::get_max_queued_bytes();
	const int64_t max_message = SteamProjectSettings::get_max_message_size();
	int received_total = 0;
	while (received_total < receive_count) {
		int64_t byte_room = max_bytes - incoming_bytes;
		if (byte_room <= 0) {
			break;
		}
		// Steam closes any connection that sends more than max_message bytes, so a
		// batch this size keeps the queue under budget + one message.
		int batch = receive_count - received_total;
		int64_t by_bytes = byte_room / max_message + 1;
		if (by_bytes < batch) {
			batch = (int)by_bytes;
		}
		int number_messages = SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup(
				SteamAPI_SteamNetworkingSockets_SteamAPI(), poll_group, messages, batch);
		if (number_messages <= 0) {
			break;
		}
		received_total += number_messages;
		bool drained = number_messages < batch;
		for (int i = 0; i < number_messages; i++) {
			SteamNetworkingMessage_t *message = messages[i];
			if (SteamAPI_SteamNetworkingSockets_SteamAPI() == nullptr) {
				// A signal handler shut Steam down; the rest of the batch died with it.
				return;
			}

			if (!steam_connections.has(message->m_conn)) {
				SteamAPI_SteamNetworkingMessage_t_Release(message);
				continue;
			}

			Ref<SteamPacketPeer> peer = steam_connections[message->m_conn];
			uint32_t peer_id = peer->get_peer_id();
			if (peer_id != 0) {
				// User data may already contain an ID before admission, or after rejection.
				if (peers.has(peer_id) && peers[peer_id]->get_connection_handle() == message->m_conn) {
					incoming_packets.push_back(message);
					incoming_bytes += message->GetSize();
				} else {
					SteamAPI_SteamNetworkingMessage_t_Release(message);
				}
			} else {
				HSteamNetConnection connection = message->m_conn;
				Error handshake = peer->process_ping(message);
				SteamAPI_SteamNetworkingMessage_t_Release(message);
				if (handshake == OK) {
					_upgrade_peer(connection);
				} else {
					// A first message that is not a peer-ID handshake ends the connection.
					peer->disconnect_peer(true);
					steam_connections.erase(connection);
				}
			}
		}
		if (drained || poll_group == k_HSteamNetPollGroup_Invalid) {
			break;
		}
	}
}

void SteamMultiplayerPeer::network_connection_status_changed(
		SteamNetConnectionStatusChangedCallback_t *p_status_change) {
	bool owned = steam_connections.has(p_status_change->m_hConn);
	bool incoming = !owned && listen_socket != k_HSteamListenSocket_Invalid &&
			p_status_change->m_info.m_hListenSocket == listen_socket &&
			p_status_change->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting;
	if (!owned && !incoming) {
		return;
	}

	if (unlikely(debug_level > DEBUG_LEVEL_NONE)) {
		WARN_PRINT(vformat(
				"Connection from %ud changed from %ud to %ud",
				(uint64_t)SteamAPI_SteamNetworkingIdentity_GetSteamID64(
						&p_status_change->m_info.m_identityRemote),
				p_status_change->m_eOldState, p_status_change->m_info.m_eState)
				);
	}

	// Check the state of the connection
	switch (p_status_change->m_info.m_eState) {
		// Do nothing
		case k_ESteamNetworkingConnectionState_None:
		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
			if (unlikely(debug_level > DEBUG_LEVEL_NONE)) {
				WARN_PRINT(vformat("Connection closed with reason %ud: %s",
						p_status_change->m_info.m_eEndReason,
						p_status_change->m_info.m_szEndDebug)
						);
			}
			Ref<SteamPacketPeer> peer = steam_connections[p_status_change->m_hConn];
			uint32_t connection_peer_id = peer->get_peer_id();
			if (peers.has(connection_peer_id) &&
					peers[connection_peer_id]->get_connection_handle() == p_status_change->m_hConn) {
				_remove_peer(connection_peer_id, true, true);
				return;
			}

			steam_connections.erase(p_status_change->m_hConn);
			peer->disconnect_peer(true);

			// Only failure of the designated host ends an initial client connection.
			if (p_status_change->m_hConn == host_connection) {
				host_connection = k_HSteamNetConnection_Invalid;
				if (p_status_change->m_eOldState == k_ESteamNetworkingConnectionState_Connecting &&
						p_status_change->m_info.m_eEndReason == k_ESteamNetConnectionEnd_Remote_BadCert && connection_retries < 5) {
					connection_retries++;
					if (add_peer(expected_host_steam_id, host_virtual_port) == OK) {
						break;
					}
				}
				_close();
			}
			break;
		}
		// A new incoming connection we should handle
		case k_ESteamNetworkingConnectionState_Connecting: {
			if (incoming) {
				if (is_refusing_new_connections()) {
					if (unlikely(debug_level > DEBUG_LEVEL_NONE)) {
						WARN_PRINT("Connection refused because Godot peer is refusing connections.");
					}
					SteamAPI_ISteamNetworkingSockets_CloseConnection(
							SteamAPI_SteamNetworkingSockets_SteamAPI(), p_status_change->m_hConn, 0,
							"Connection refused by application.", false);
					return;
				}

				if (_count_pending_connections() >= SteamProjectSettings::get_max_pending_connections()) {
					SteamAPI_ISteamNetworkingSockets_CloseConnection(
							SteamAPI_SteamNetworkingSockets_SteamAPI(), p_status_change->m_hConn, 0,
							"Too many pending connections.", false);
					return;
				}

				if (unlikely(debug_level > DEBUG_LEVEL_NONE)) {
					WARN_PRINT(vformat(
							"Connection attempt from %ud.",
							(uint64_t)SteamAPI_SteamNetworkingIdentity_GetSteamID64(
								&p_status_change->m_info.m_identityRemote
								))
							);
				}

				uint32_t connection_peer_id = p_status_change->m_info.m_nUserData;
				if (peers.has(connection_peer_id)) {
					SteamAPI_ISteamNetworkingSockets_CloseConnection(
							SteamAPI_SteamNetworkingSockets_SteamAPI(), p_status_change->m_hConn, 0,
							"Peer ID already connected.", false);
					return;
				}

				if (SteamAPI_ISteamNetworkingSockets_AcceptConnection(
						SteamAPI_SteamNetworkingSockets_SteamAPI(),
						p_status_change->m_hConn) != k_EResultOK) {
					SteamAPI_ISteamNetworkingSockets_CloseConnection(
						SteamAPI_SteamNetworkingSockets_SteamAPI(),
						p_status_change->m_hConn, 0,nullptr, false
						);
					WARN_PRINT("A connection was started but couldn't be accepted.");
					return;
				} else {
					if (unlikely(debug_level > DEBUG_LEVEL_NONE)) {
						WARN_PRINT("Connection accepted.");
					}
				}
			}

			// Regardless of who created the connection, since we're symmetrical we
			// want to keep track of everything Add it to our poll group
			if (!SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup(
					SteamAPI_SteamNetworkingSockets_SteamAPI(), p_status_change->m_hConn, poll_group)) {
				SteamAPI_ISteamNetworkingSockets_CloseConnection(
						SteamAPI_SteamNetworkingSockets_SteamAPI(), p_status_change->m_hConn, 0,
						nullptr, false
						);
				steam_connections.erase(p_status_change->m_hConn);
				if (p_status_change->m_hConn == host_connection) {
					_close();
				}
				WARN_PRINT("A connection was abandoned because it could not be added to a poll group.");
				return;
			}

			if (!owned) {
				_add_pending_peer(SteamAPI_SteamNetworkingIdentity_GetSteamID64(
						&p_status_change->m_info.m_identityRemote),
						p_status_change->m_hConn,
						SteamPacketPeer::PeerState::STATE_CONNECTING);
			}
			break;
		}
		case k_ESteamNetworkingConnectionState_Connected: {
			// Someone has finished connecting to us
			if (p_status_change->m_hConn == host_connection) {
				connection_retries = 0;
			}
			if (unlikely(debug_level > DEBUG_LEVEL_NONE)) {
				WARN_PRINT(vformat( "Attempting to send peer ID to %ud",
						(uint64_t)SteamAPI_SteamNetworkingIdentity_GetSteamID64(
								&p_status_change->m_info.m_identityRemote))
						);
			}
			if (steam_connections.has(p_status_change->m_hConn)) {
				steam_connections[p_status_change->m_hConn]->set_state(
						(SteamPacketPeer::PeerState)p_status_change->m_info.m_eState);
				// Then send our Peer ID
				Error ping_result =
						steam_connections[p_status_change->m_hConn]->ping(_get_unique_id());
				if (unlikely(debug_level > DEBUG_LEVEL_NONE)) {
					WARN_PRINT(vformat(
							"Ping sent to %ud: %d",
							(uint64_t)SteamAPI_SteamNetworkingIdentity_GetSteamID64(
									&p_status_change->m_info.m_identityRemote),
							ping_result)
							);
				}
				
				// If we already have their peer ID, upgrade it
				if (steam_connections[p_status_change->m_hConn]->get_peer_id() > 0) {
					_upgrade_peer(p_status_change->m_hConn);
				}
			} else {
				ERR_PRINT("A connection was established without a peer being created.");
			}

			break;
		}
		// Pass this off to the peer, and update its state
		// If it doesn't exist, do nothing
		default: {
			if (steam_connections.has(p_status_change->m_hConn)) {
				steam_connections[p_status_change->m_hConn]->set_state(
						(SteamPacketPeer::PeerState)p_status_change->m_info.m_eState
						);
			}
			break;
		}
	}
}

void SteamMultiplayerPeer::lobby_chat_update(LobbyChatUpdate_t *p_chat_update) {
	if (p_chat_update->m_ulSteamIDLobby != tracked_lobby) {
		if (unlikely(debug_level > DEBUG_LEVEL_NONE)) {
			WARN_PRINT(vformat("LobbyChatUpdate ignored due to lobby ID mismatch: "
					"Expecting: %ud, Received: %d",
				 	tracked_lobby, (uint64_t)p_chat_update->m_ulSteamIDLobby));
		}
		return;
	}

	if (p_chat_update->m_ulSteamIDUserChanged ==
			SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser())) {
		// Ignore updates about ourselves
		return;
	}

	if (p_chat_update->m_rgfChatMemberStateChange &
			k_EChatMemberStateChangeEntered) {
		add_peer(p_chat_update->m_ulSteamIDUserChanged);
	} else {
		// If they didn't enter, it doesn't matter why, they are leaving
		// Collect connections to remove to avoid iterator invalidation
		LocalVector<int> peer_ids_to_disconnect;
		LocalVector<HSteamNetConnection> connections_to_remove;
		for (KeyValue<HSteamNetConnection, Ref<SteamPacketPeer>> &E :
				steam_connections) {
			// Guard on the underlying object pointer, not just the Ref wrapper.
			// A dangling/half-torn-down entry can pass is_null() yet dereference
			// to a null object, which crashed get_steam_id() (KERN_INVALID_ADDRESS).
			SteamPacketPeer *peer = E.value.ptr();
			if (peer == nullptr) {
				connections_to_remove.push_back(E.key);
				continue;
			}
			if (peer->get_steam_id() == p_chat_update->m_ulSteamIDUserChanged) {
				if (peer->get_peer_id() > 0) {
					peer_ids_to_disconnect.push_back(peer->get_peer_id());
				} else {
					// We have an open connection but no peer
					peer->disconnect_peer(true);
					connections_to_remove.push_back(peer->get_connection_handle());
				}
			}
		}
		// Disconnect peers outside the iteration
		for (int peer_id : peer_ids_to_disconnect) {
			disconnect_peer(peer_id);
		}
		// Remove collected connections outside the iteration
		for (HSteamNetConnection conn : connections_to_remove) {
			steam_connections.erase(conn);
		}
	}
}

void SteamMultiplayerPeer::_close() {
	connection_status = CONNECTION_DISCONNECTED;
	server = false;
	expected_host_steam_id = 0;
	host_connection = k_HSteamNetConnection_Invalid;
	host_virtual_port = 0;
	connection_retries = 0;

	// Steam may already be shut down (steamShutdown, or Steam freed first on exit).
	// Its handles and messages died with it, so only drop our references.
	const bool steam_alive = SteamAPI_SteamNetworkingSockets_SteamAPI() != nullptr;

	for (KeyValue<HSteamNetConnection, Ref<SteamPacketPeer>> &E :
			steam_connections) {
		if (steam_alive) {
			E.value->disconnect_peer(true);
		}
	}
	steam_connections.clear();
	peers.clear();
	pending_deadlines.clear();
	incoming_bytes = 0;

	if (current_packet != nullptr) {
		if (steam_alive) {
			SteamAPI_SteamNetworkingMessage_t_Release(current_packet);
		}
		current_packet = nullptr;
	}

	while (!incoming_packets.is_empty()) {
		SteamNetworkingMessage_t *packet = incoming_packets.front()->get();
		incoming_packets.pop_front();
		if (steam_alive) {
			SteamAPI_SteamNetworkingMessage_t_Release(packet);
		}
	}

	if (listen_socket != k_HSteamListenSocket_Invalid) {
		if (steam_alive) {
			SteamAPI_ISteamNetworkingSockets_CloseListenSocket(
					SteamAPI_SteamNetworkingSockets_SteamAPI(), listen_socket);
		}
		listen_socket = k_HSteamListenSocket_Invalid;
	}

	if (poll_group != k_HSteamNetPollGroup_Invalid) {
		if (steam_alive) {
			SteamAPI_ISteamNetworkingSockets_DestroyPollGroup(
					SteamAPI_SteamNetworkingSockets_SteamAPI(), poll_group);
		}
		poll_group = k_HSteamNetPollGroup_Invalid;
	}
}

int SteamMultiplayerPeer::_get_unique_id() const {
	return unique_id;
}

MultiplayerPeer::ConnectionStatus
SteamMultiplayerPeer::_get_connection_status() const {
	return connection_status;
}

int SteamMultiplayerPeer::_get_available_packet_count() const {
	// Messages queued before Steam shut down died with it; never hand them out.
	if (SteamAPI_SteamNetworkingSockets_SteamAPI() == nullptr) {
		return 0;
	}
	return incoming_packets.size();
}

Error SteamMultiplayerPeer::_get_packet(const uint8_t **r_buffer, int *r_buffer_size) {
	ERR_FAIL_COND_V_MSG(incoming_packets.is_empty(), ERR_UNAVAILABLE,
			"No incoming packets available."
			);

	if (SteamAPI_SteamNetworkingSockets_SteamAPI() == nullptr) {
		current_packet = nullptr;
		return ERR_UNAVAILABLE;
	}

	if (current_packet != nullptr) {
		SteamAPI_SteamNetworkingMessage_t_Release(current_packet);
		current_packet = nullptr;
	}

	current_packet = incoming_packets.front()->get();
	incoming_packets.pop_front();
	incoming_bytes -= current_packet->GetSize();

	*r_buffer = (uint8_t *)current_packet->GetData();
	*r_buffer_size = (int)current_packet->GetSize();

	return OK;
}

Error SteamMultiplayerPeer::_put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	ERR_FAIL_COND_V_MSG(connection_status != CONNECTION_CONNECTED, ERR_UNCONFIGURED,
			"The multiplayer instance isn't currently connected to any server or client."
			);
	ERR_FAIL_COND_V_MSG(target_peer != 0 && !peers.has(Math::abs(target_peer)),
			ERR_INVALID_PARAMETER,
			vformat("Invalid target peer: %d", target_peer)
			);

	Error result = OK;
	if (target_peer == 0) {
		// Send to all peers
		for (KeyValue<uint32_t, Ref<SteamPacketPeer>> &E : peers) {
			Error sent = E.value->send(_get_transfer_channel(), p_buffer, p_buffer_size, _get_steam_packet_flags());
			if (result == OK && sent != OK) {
				result = sent;
			}
		}
	} else if (peers.has(target_peer)) {
		// Send to specific peer
		Ref<SteamPacketPeer> peer = peers[target_peer];
		return peer->send(_get_transfer_channel(), p_buffer, p_buffer_size, _get_steam_packet_flags());
	} else if (target_peer < 0) {
		// We're in weird exclusion territory
		uint32_t exclude = Math::abs(target_peer);
		for (KeyValue<uint32_t, Ref<SteamPacketPeer>> &E : peers) {
			// GCC complains of different signedness: const unsigned int and int at 428
			if (E.key == exclude) {
				continue;
			}
			Error sent = E.value->send(_get_transfer_channel(), p_buffer, p_buffer_size, _get_steam_packet_flags());
			if (result == OK && sent != OK) {
				result = sent;
			}
		}
	}

	return result;
}

int SteamMultiplayerPeer::_get_max_packet_size() const {
	return SteamProjectSettings::get_max_message_size();
}

Error SteamMultiplayerPeer::_create_listen_socket(int p_virtual_port) {
	SteamNetworkingConfigValue_t opt[2];
	opt[0].SetInt32(k_ESteamNetworkingConfig_SymmetricConnect, 1);
	opt[1].SetInt32(k_ESteamNetworkingConfig_RecvMaxMessageSize, SteamProjectSettings::get_max_message_size());
	listen_socket = SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P(
			SteamAPI_SteamNetworkingSockets_SteamAPI(), p_virtual_port, 2, opt
			);

	ERR_FAIL_COND_V(listen_socket == k_HSteamListenSocket_Invalid, ERR_CANT_CREATE);

	return OK;
}

Error SteamMultiplayerPeer::_create_poll_group() {
	poll_group = SteamAPI_ISteamNetworkingSockets_CreatePollGroup(
			SteamAPI_SteamNetworkingSockets_SteamAPI()
			);
	ERR_FAIL_COND_V(poll_group == k_HSteamNetPollGroup_Invalid, ERR_CANT_CREATE);

	return OK;
}

Error SteamMultiplayerPeer::create_host(int p_virtual_port) {
	ERR_FAIL_COND_V(connection_status != CONNECTION_DISCONNECTED, ERR_ALREADY_IN_USE);

	server = true;
	expected_host_steam_id = 0;
	unique_id = 1; // Server is always peer ID 1

	Error created = _create_listen_socket(p_virtual_port);
	if (created == OK) {
		created = _create_poll_group();
	}
	if (created != OK) {
		_close();
	}
	ERR_FAIL_COND_V(created != OK, created);

	set_refuse_new_connections(false);
	connection_status = CONNECTION_CONNECTED;
	return OK;
}

Error SteamMultiplayerPeer::create_client(uint64_t p_host_steam_id, int p_virtual_port) {
	ERR_FAIL_COND_V(connection_status != CONNECTION_DISCONNECTED, ERR_ALREADY_IN_USE);

	server = false;
	expected_host_steam_id = p_host_steam_id;
	host_virtual_port = p_virtual_port;
	connection_retries = 0;
	unique_id = generate_unique_id();

	// We create a listen socket in all cases as we need to handle true P2P
	Error created = _create_listen_socket(p_virtual_port);
	if (created == OK) {
		created = _create_poll_group();
	}
	if (created != OK) {
		_close();
	}
	ERR_FAIL_COND_V(created != OK, created);

	Error peer_created = add_peer(p_host_steam_id, p_virtual_port);
	if (peer_created != OK) {
		_close();
		return peer_created;
	}

	set_refuse_new_connections(false);
	connection_status = CONNECTION_CONNECTING;

	return OK;
}

Error SteamMultiplayerPeer::add_peer(uint64_t p_steam_id, int p_virtual_port) {
	SteamNetworkingIdentity remote_identity;
	SteamAPI_SteamNetworkingIdentity_SetSteamID64(&remote_identity, p_steam_id);

	SteamNetworkingConfigValue_t opt[2];
	SteamAPI_SteamNetworkingConfigValue_t_SetInt32(&opt[0],
			k_ESteamNetworkingConfig_SymmetricConnect, 1
			);

	SteamAPI_SteamNetworkingConfigValue_t_SetInt32(&opt[1],
			k_ESteamNetworkingConfig_RecvMaxMessageSize, SteamProjectSettings::get_max_message_size());

	HSteamNetConnection connection = SteamAPI_ISteamNetworkingSockets_ConnectP2P(
			SteamAPI_SteamNetworkingSockets_SteamAPI(),
			remote_identity, p_virtual_port, 2, opt
			);

	ERR_FAIL_COND_V(connection == k_HSteamNetConnection_Invalid, ERR_CANT_CREATE);

	_add_pending_peer(p_steam_id, connection, SteamPacketPeer::STATE_CONNECTING);
	ERR_FAIL_COND_V(!steam_connections.has(connection), ERR_CANT_CREATE);
	if (!server && p_steam_id == expected_host_steam_id && host_connection == k_HSteamNetConnection_Invalid) {
		host_connection = connection;
	}

	return OK;
}

void SteamMultiplayerPeer::_add_pending_peer(
		uint64_t p_steam_id, HSteamNetConnection p_connection_handle,
		SteamPacketPeer::PeerState p_peer_state) {
	Ref<SteamPacketPeer> peer = memnew(SteamPacketPeer);
	peer->set_steam_id(p_steam_id);
	peer->set_connection_handle(p_connection_handle);
	if (peer->get_connection_handle() == k_HSteamNetConnection_Invalid) {
		return;
	}
	peer->set_state(p_peer_state);

	steam_connections[p_connection_handle] = peer;
	pending_deadlines[p_connection_handle] = Time::get_singleton()->get_ticks_msec() +
			SteamProjectSettings::get_handshake_timeout_ms();
}

void SteamMultiplayerPeer::_upgrade_peer(
		HSteamNetConnection p_connection_handle) {
	if (!steam_connections.has(p_connection_handle)) {
		return;
	}

	Ref<SteamPacketPeer> peer = steam_connections[p_connection_handle];
	uint32_t peer_id = peer->get_peer_id();
	if (peers.has(peer_id) && peers[peer_id]->get_connection_handle() == p_connection_handle) {
		return;
	}
	if (peer_id == 0 || peer_id > INT32_MAX || peer_id == (uint32_t)unique_id || peers.has(peer_id) ||
			(peer_id == 1 && (server || expected_host_steam_id == 0 || peer->get_steam_id() != expected_host_steam_id))) {
		// Clear untrusted user data before closing; callbacks may still reference it.
		peer->set_peer_id(0);
		peer->disconnect_peer(true);
		steam_connections.erase(p_connection_handle);
		return;
	}
	if (peer->get_state() != SteamPacketPeer::STATE_CONNECTED) {
		return;
	}

	peers[peer_id] = peer;
	if (connection_status == CONNECTION_CONNECTING) {
		connection_status = CONNECTION_CONNECTED;
	}
	emit_signal("peer_connected", peer_id);
}

Ref<SteamPacketPeer> SteamMultiplayerPeer::get_peer(int p_peer_id) {
	if (peers.has(p_peer_id)) {
		return peers[p_peer_id];
	}
	return nullptr;
}

Error SteamMultiplayerPeer::host_with_lobby(uint64_t p_lobby_id) {
	ERR_FAIL_COND_V(connection_status != CONNECTION_DISCONNECTED, ERR_ALREADY_IN_USE);
	// k_steamIDNil is a CSteamID in the SDK but doesn't jive with the Flat API, should be 0?
	ERR_FAIL_COND_V_MSG(SteamAPI_ISteamMatchmaking_GetLobbyOwner(
			SteamAPI_SteamMatchmaking(), p_lobby_id) == 0, ERR_CANT_CREATE,
			"You must be a member of the lobby you are trying to connect with the "
			"SteamMultiplayerPeer."
			);
	ERR_FAIL_COND_V_MSG(
			SteamAPI_ISteamMatchmaking_GetLobbyOwner(SteamAPI_SteamMatchmaking(), p_lobby_id) !=
			SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser()),
			ERR_CANT_CREATE,
			vformat("You must be the owner of the lobby you are trying to host with "
			"SteamMultiplayerPeer.")
			);

	tracked_lobby = p_lobby_id;

	Error host_created = create_host();
	ERR_FAIL_COND_V(host_created != OK, host_created);

	// In case the lobby already has members, let's connect to them
	int count = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(
			SteamAPI_SteamMatchmaking(), p_lobby_id
			);
	for (int i = 0; i < count; i++) {
		uint64_t member = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(
				SteamAPI_SteamMatchmaking(), p_lobby_id, i
				);
		if (member != SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser())) {
			add_peer(member);
		}
	}

	return OK;
}

Error SteamMultiplayerPeer::connect_to_lobby(uint64_t p_lobby_id) {
	ERR_FAIL_COND_V(connection_status != CONNECTION_DISCONNECTED, ERR_ALREADY_IN_USE);
	// k_steamIDNil is a CSteamID in the SDK but doesn't jive with the Flat API, should be 0?
	ERR_FAIL_COND_V_MSG(SteamAPI_ISteamMatchmaking_GetLobbyOwner(
			SteamAPI_SteamMatchmaking(), p_lobby_id) == 0,
			ERR_CANT_CREATE,
			"You must be a member of the lobby you are trying to connect with the "
			"SteamMultiplayerPeer."
			);

	tracked_lobby = p_lobby_id;

	Error client_created = create_client(SteamAPI_ISteamMatchmaking_GetLobbyOwner(
			SteamAPI_SteamMatchmaking(), p_lobby_id)
			);
	ERR_FAIL_COND_V(client_created != OK, client_created);

	// Connect to the rest of the members
	int count = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(
			SteamAPI_SteamMatchmaking(), p_lobby_id
			);
	for (int i = 0; i < count; i++) {
		uint64_t member = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(
				SteamAPI_SteamMatchmaking(), p_lobby_id, i
				);
		if (member != SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser()) &&
				member != SteamAPI_ISteamMatchmaking_GetLobbyOwner(
						SteamAPI_SteamMatchmaking(), p_lobby_id)) {
			add_peer(member);
		}
	}

	return OK;
}

void SteamMultiplayerPeer::set_no_nagle(const bool p_no_nagle) {
	no_nagle = p_no_nagle;
}

bool SteamMultiplayerPeer::get_no_nagle() const {
	return no_nagle;
}

void SteamMultiplayerPeer::set_no_delay(const bool p_no_delay) {
	no_delay = p_no_delay;
}

bool SteamMultiplayerPeer::get_no_delay() const {
	return no_delay;
}

void SteamMultiplayerPeer::set_server_relay(const bool p_server_relay) {
	server_relay = p_server_relay;
}

bool SteamMultiplayerPeer::get_server_relay() const {
	return server_relay;
}

extern "C" void __cdecl SteamAPIDebugTextHook(int nSeverity, const char *pchDebugText) {
	WARN_PRINT(pchDebugText);
}

void SteamMultiplayerPeer::set_debug_level(DebugLevel p_debug_level) {
	debug_level = p_debug_level;
	if (debug_level >= DEBUG_LEVEL_STEAM) {
		SteamAPI_ISteamUtils_SetWarningMessageHook(SteamAPI_SteamUtils(), &SteamAPIDebugTextHook);
	} else {
		SteamAPI_ISteamUtils_SetWarningMessageHook(SteamAPI_SteamUtils(), nullptr);
	}
}

SteamMultiplayerPeer::DebugLevel SteamMultiplayerPeer::get_debug_level() const {
	return debug_level;
}

const int SteamMultiplayerPeer::_get_steam_packet_flags() {
	int32_t flags = (k_nSteamNetworkingSend_NoNagle * no_nagle) | 
			(k_nSteamNetworkingSend_NoDelay * no_delay);

	switch (get_transfer_mode()) {
		case TransferMode::TRANSFER_MODE_RELIABLE:
			return k_nSteamNetworkingSend_Reliable | flags;
			break;
		case TransferMode::TRANSFER_MODE_UNRELIABLE:
			return k_nSteamNetworkingSend_Unreliable | flags;
			break;
		case TransferMode::TRANSFER_MODE_UNRELIABLE_ORDERED:
			// No equivalent
			return k_nSteamNetworkingSend_Reliable | flags;
			break;
	}

	ERR_FAIL_V_MSG(-1, "Error determining SteamNetworkingSend flags.");
}

uint64_t SteamMultiplayerPeer::get_steam_id_for_peer_id(int p_peer_id) {
	if (p_peer_id == unique_id) {
		return SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser());
	}

	if (peers.has(p_peer_id)) {
		return peers[p_peer_id]->get_steam_id();
	}

	return 0;
}

int SteamMultiplayerPeer::get_peer_id_for_steam_id(uint64_t p_steam_id) {
	if (p_steam_id == SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser())) {
		return unique_id;
	}

	for (KeyValue<uint32_t, Ref<SteamPacketPeer>> &E : peers) {
		if (E.value->get_steam_id() == p_steam_id) {
			return E.key;
		}
	}

	return 0;
}


void SteamMultiplayerPeer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_host", "virtual_port"),
			&SteamMultiplayerPeer::create_host, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("create_client", "steam_id", "virtual_port"),
			&SteamMultiplayerPeer::create_client, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("add_peer", "steam_id", "virtual_port"),
			&SteamMultiplayerPeer::add_peer, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_peer", "peer_id"),
			&SteamMultiplayerPeer::get_peer);

	ClassDB::bind_method(D_METHOD("host_with_lobby", "lobby_id"),
			&SteamMultiplayerPeer::host_with_lobby);
	ClassDB::bind_method(D_METHOD("connect_to_lobby", "lobby_id"),
			&SteamMultiplayerPeer::connect_to_lobby);

	ClassDB::bind_method(D_METHOD("get_steam_id_for_peer_id", "peer_id"),
			&SteamMultiplayerPeer::get_steam_id_for_peer_id);
	ClassDB::bind_method(D_METHOD("get_peer_id_for_steam_id", "steam_id"),
			&SteamMultiplayerPeer::get_peer_id_for_steam_id);

	ClassDB::bind_method(D_METHOD("get_no_delay"),
			&SteamMultiplayerPeer::get_no_delay);
	ClassDB::bind_method(D_METHOD("set_no_delay"),
			&SteamMultiplayerPeer::set_no_delay);
	ClassDB::bind_method(D_METHOD("get_no_nagle"),
			&SteamMultiplayerPeer::get_no_nagle);
	ClassDB::bind_method(D_METHOD("set_no_nagle"),
			&SteamMultiplayerPeer::set_no_nagle);
	ClassDB::bind_method(D_METHOD("get_server_relay"),
			&SteamMultiplayerPeer::get_server_relay);
	ClassDB::bind_method(D_METHOD("set_server_relay"),
			&SteamMultiplayerPeer::set_server_relay);
	ClassDB::bind_method(D_METHOD("get_debug_level"),
			&SteamMultiplayerPeer::get_debug_level);
	ClassDB::bind_method(D_METHOD("set_debug_level"),
			&SteamMultiplayerPeer::set_debug_level);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "no_delay"),
			"set_no_delay", "get_no_delay");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "no_nagle"),
			"set_no_nagle", "get_no_nagle");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "server_relay"), "set_server_relay",
			"get_server_relay");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "debug_level", PROPERTY_HINT_ENUM, "None,Peer,Steam"),
			"set_debug_level", "get_debug_level");

	BIND_ENUM_CONSTANT(DEBUG_LEVEL_NONE);
	BIND_ENUM_CONSTANT(DEBUG_LEVEL_PEER);
	BIND_ENUM_CONSTANT(DEBUG_LEVEL_STEAM);
}

}
