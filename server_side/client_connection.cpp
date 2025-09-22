/*
 * ClientConnection class implementation
 */
#include <iostream>
#include <chrono>
#include <format>
#include <google/protobuf/util/time_util.h>

#include "client_connection.h"
#include "user_data.h"
#include "../common/defines.h"
#include "messages.pb.h"


ClientConnection::ClientConnection(int socket_fd) : Connection(socket_fd), m_user(nullptr),
    m_connected_at(std::chrono::steady_clock::now()) {
    // recv need time-out to disconnected the client
    set_recv_timeout(CLIENT_DISCONNECT_TIMEOUT);
}

ClientConnection::~ClientConnection() {
}

int ClientConnection::recv_all(void* data, size_t len, int flags) {
    int bytes = Connection::recv_all(data, len, flags);
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

bool ClientConnection::make_user(const std::string &user_name, bool is_admin) {
    // Must be a connection from admin user
    if (!this->is_admin()) {
        return false;
    }

    auto new_user = find_user(user_name, true);
    if (new_user == nullptr) {
        return false;
    }
    return new_user->set_admin(is_admin);
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
    return m_user ? m_user->is_admin() : false;
}

std::string ClientConnection::get_info() const {
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - m_connected_at);
    return std::format("{}{}, IP: {}, time online {}", get_user_name(),
            (is_admin() ? " [admin]" : ""),
            get_peer_name(), duration);
}

bool ClientConnection::store_chat(const PBChatMessage &chat) {
    if (m_user == nullptr) {
        return false;
    }

    // Convert to nanoseconds since epoch
    auto sent_at_ns = google::protobuf::util::TimeUtil::TimestampToNanoseconds(chat.sent_at());
    UserData::TimePoint sent_at{std::chrono::nanoseconds(sent_at_ns)};

    return m_user->store_chat(sent_at, chat.text());
}
