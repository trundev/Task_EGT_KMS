/*
 * ClientConnection class implementation
 */
#include <iostream>
#include <format>

#include "client_connection.h"


ClientConnection::ClientConnection(int socket_fd) : Connection(socket_fd) {
}

std::string ClientConnection::get_user_name() const
{
    //TODO: Use actual name
    return std::format("Socket{}", get_socket());
}
