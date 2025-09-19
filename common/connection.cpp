#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "connection.h"
#include "messages.pb.h"


Connection::Connection(int socket_fd) : m_socket(socket_fd) {
}

Connection::~Connection() {
    if (m_socket > 0) {
        close(m_socket);
        m_socket = 0;
    }
}

void Connection::force_shutdown() {
    shutdown(m_socket, SHUT_RD);
}

std::string Connection::get_peer_name() const {
    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);

    if (getpeername(m_socket, (struct sockaddr*)&peer_addr, &peer_len) == 0) {
        return std::string(inet_ntoa(peer_addr.sin_addr));//TODO: ":" + ntohs(peer_addr.sin_port);
    }
    return "<error>";
}

ssize_t Connection::send_all(const void* data, size_t len, int flags) {
    const char* ptr = static_cast<const char*>(data);
    size_t total_bytes = 0;

    while (total_bytes < len) {
        ssize_t bytes = ::send(m_socket, ptr + total_bytes, len - total_bytes, flags);
        if (bytes < 0) {
            // Handle error (e.g., EINTR or EAGAIN for non-blocking)
            std::cerr << "send() error " << errno << std::endl;
            return -1;
        }
        total_bytes += bytes;
    }

    return total_bytes;
}

ssize_t Connection::recv_all(void* data, size_t len, int flags) {
    char* ptr = static_cast<char*>(data);
    size_t total_bytes = 0;

    while (total_bytes < len) {
        ssize_t bytes = ::recv(m_socket, ptr + total_bytes, len - total_bytes, flags);
        if (bytes <= 0) {
            // Handle error (e.g., EINTR or EAGAIN for non-blocking)
            // Zero means connection closed (still error as data are incomplete)
            if (bytes < 0) {
                std::cerr << "recv() error " << errno << std::endl;
            }
            return -1;
        }
        total_bytes += bytes;
    }

    return total_bytes;
}

bool Connection::send_protobuf(const PBMessage &message) {
    std::string buffer(message.SerializeAsString());

    // Send size
    uint32_t len = htonl(buffer.size());
    if (send_all(&len, sizeof(len), 0) < 0) {
        return false;
    }

    // Send serialied message
    if (send_all(buffer.data(), buffer.size(), 0) < 0) {
        return false;
    }
    return true;
}

bool Connection::recv_protobuf(PBMessage &message) {
    // Receive size
    uint32_t len;
    if (recv_all(&len, sizeof(len), 0) < 0) {
        return false;
    }
    len = ntohl(len);

    // Receive serialized message
    std::string buffer(len, '\0');
    if (recv_all(buffer.data(), len, 0) < 0) {
        return false;
    }

    return message.ParseFromString(buffer);
}

// Function to overload operator<<
std::ostream& operator<<(std::ostream& os, const Connection& obj) {
    os << "Connection(socket=" << obj.get_socket() << ")";
    return os;
}
