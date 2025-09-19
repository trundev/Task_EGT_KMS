/*
 * ClientConnection class implementation
 */
#include <iostream>
#include <format>

#include "client_connection.h"
#include "user_data.h"
#include "../common/defines.h"

ClientConnection::ClientConnection(int socket_fd) : Connection(socket_fd), m_user(nullptr) {
}

ClientConnection::~ClientConnection() {
}

std::string ClientConnection::get_user_name() const {
    //TODO: Better handlig of no-user case
    return m_user ? m_user->get_name() : std::format("Socket{}", get_socket());
}

ClientConnection::recv_status_t ClientConnection::recv_message(std::string &message) {
    std::array<char, MAX_MESSAGE_SIZE> buffer;
    ssize_t bytes = recv(buffer.data(), buffer.size());
    if (bytes < 0) {
        std::cerr << *this << ": recv() error " << errno << std::endl;
        return RECV_ERROR;
    }
    else if (bytes == 0) {
        std::cout << *this << ": disconnected" << std::endl;
        return RECV_DISCONNECT;
    }

#if DEBUG   // Dump receied data as hex
    std::cout << *this << " hex data:";
    for (size_t i = 0; i < bytes; ++i) {
       std::cout << std::format(" {:02X}", buffer[i]);
    }
    std::cout << std::endl;
#endif

    message = std::string(buffer.data(), bytes);

    // The very first time a terminated user-name must be sent
    if (m_user != nullptr) {
        return RECV_OK;
    }

    auto pos = message.find(USER_NAME_TERMINATOR);
    if (pos == std::string::npos) {
        std::cerr << *this << ": Incorrect username: " << message << std::endl;
        return RECV_ERROR;
    }
    auto username = message.substr(0, pos);
    message = message.substr(pos + 1);

    // Attach to user from database (create if needed)
    m_user = find_user(username, true);
    if (m_user == nullptr) {
        std::cerr << *this << ": Failed to find/create user: " << username << std::endl;
        return RECV_ERROR;
    }
    return RECV_CONNECT;
}
