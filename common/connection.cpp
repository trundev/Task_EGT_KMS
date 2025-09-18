#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

#include "connection.h"


Connection::Connection(int socket_fd) : m_socket(socket_fd) {
}

Connection::~Connection() {
    if (m_socket > 0) {
        close(m_socket);
        m_socket = 0;
    }
}

ssize_t Connection::recv(void* data, size_t len, int flags) {
    return ::recv(m_socket, data, len, flags);
}

ssize_t Connection::send_all(const void* data, size_t len, int flags) {
    const char* ptr = static_cast<const char*>(data);
    size_t total_bytes = 0;

    while (total_bytes < len) {
        ssize_t bytes = ::send(m_socket, ptr + total_bytes, len - total_bytes, flags);
        if (bytes < 0) {
            // Handle error (e.g., EINTR or EAGAIN for non-blocking)
            return -1;
        }
        total_bytes += bytes;
    }

    return total_bytes;
}


// Function to overload operator<<
std::ostream& operator<<(std::ostream& os, const Connection& obj) {
    os << "Connection(socket=" << obj.get_socket() << ")";
    return os;
}
