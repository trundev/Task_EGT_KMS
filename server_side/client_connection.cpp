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
}

ClientConnection::~ClientConnection() {
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
