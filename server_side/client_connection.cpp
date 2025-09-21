/*
 * ClientConnection class implementation
 */
#include <iostream>
#include <chrono>
#include <format>

#include "client_connection.h"
#include "user_data.h"
#include "../common/defines.h"

ClientConnection::ClientConnection(int socket_fd) : Connection(socket_fd), m_user(nullptr),
    m_connected_at(std::chrono::steady_clock::now()) {
    // recv need time-out to disconnected the client
    set_recv_timeout(CLIENT_DISCONNECT_TIMEOUT);
}

ClientConnection::~ClientConnection() {
}

ssize_t ClientConnection::recv_all(void* data, size_t len, int flags) {
    ssize_t bytes = Connection::recv_all(data, len, flags);
    if (bytes < 0) {
        // Set disconnect reason if recv was timed out
        if (is_last_error_timeout()) {
            m_discon_reason = "Disconnected due to inactivity";
        }
    }
    return bytes;
}

bool ClientConnection::do_login(const std::string &user_name) {
    // TODO: Create user from admin connections only, see this->is_admin()
    auto user = find_user(user_name, true);
    if (user == nullptr) {
        return false;
    }
    m_user = user;
    return true;
}

void ClientConnection::kickout(const std::string &reason) {
    if (reason.size()) {
        m_discon_reason = reason;
    }
    force_shutdown();
}

std::string ClientConnection::get_user_name() const {
    //TODO: Better handlig of no-user case
    return m_user ? m_user->get_name() : std::format("Socket{}", get_socket());
}

bool ClientConnection::is_admin() const {
    return m_user ? m_user->is_admin(): false;
}

std::string ClientConnection::get_info() const {
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - m_connected_at);
    return std::format("{}, IP: {}, time online {}", get_user_name(), get_peer_name(), duration);
}
