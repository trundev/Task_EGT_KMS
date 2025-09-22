#include <iostream>

// Socket headers differs between Linux and Windows
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define socket_close closesocket
#pragma comment(lib, "Ws2_32.lib")
#else // _WIN32
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#define socket_close close
#endif // _WIN32

#include "connection.h"
#include "messages.pb.h"
#include "../common/defines.h"


Connection::Connection(int socket_fd) : m_socket(socket_fd) {
}

Connection::~Connection() {
    if (m_socket > 0) {
#ifdef _WIN32
        shutdown(m_socket, SD_SEND);
#endif
        socket_close(m_socket);
        m_socket = 0;
    }
}

void Connection::force_shutdown() {
#ifdef _WIN32
    shutdown(m_socket, SD_RECEIVE);
    // HACK: Ensure pending recv() will be unblocked
    socket_close(m_socket);
    m_socket = 0;
#else // _WIN32
    shutdown(m_socket, SHUT_RD);
#endif // _WIN32
}

std::string Connection::get_peer_name() const {
    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);

    if (getpeername(m_socket, (struct sockaddr*)&peer_addr, &peer_len) == 0) {
        return std::string(inet_ntoa(peer_addr.sin_addr)) + ":"
                + std::to_string(ntohs(peer_addr.sin_port));
    }
    return "<error>";
}

int Connection::send_all(const void* data, size_t len, int flags) {
    const char* ptr = static_cast<const char*>(data);
    size_t total_bytes = 0;

    while (total_bytes < len) {
        int bytes = ::send(m_socket, ptr + total_bytes, len - total_bytes, flags);
        if (bytes < 0) {
            // Handle error (e.g., EINTR or EAGAIN for non-blocking)
            std::cerr << "send() error " << errno << std::endl;
            return -1;
        }
        total_bytes += bytes;
    }

    return total_bytes;
}

int Connection::recv_all(void* data, size_t len, int flags) {
    char* ptr = static_cast<char*>(data);
    size_t total_bytes = 0;

    while (total_bytes < len) {
        int bytes = ::recv(m_socket, ptr + total_bytes, len - total_bytes, flags);
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

bool Connection::is_last_error_timeout() const {
#ifdef _WIN32
    return WSAGetLastError() == WSAETIMEDOUT;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

int Connection::accept() {
    return ::accept(m_socket, nullptr, nullptr);
}

int Connection::set_recv_timeout(int seconds) {
#ifdef _WIN32
    DWORD msec_timeout = seconds * 1000;

    return setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<char*>(&msec_timeout), sizeof(msec_timeout));
#else // _WIN32
    struct timeval timeout {
        .tv_sec = seconds,
        .tv_usec = 0
    };

    return setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif // _WIN32
}

bool Connection::wait_recv_or_stdin(bool &had_recv, bool &had_stdin) {
    had_recv = false;
    had_stdin = false;

#ifdef _WIN32
    /*
     * Windows: Use WaitForMultipleObjects on m_socket and STD_INPUT_HANDLE
     */
    WSAEVENT sockEvent = WSACreateEvent();
    WSAEventSelect(m_socket, sockEvent, FD_READ);
    HANDLE handles[] = {
        sockEvent,
        GetStdHandle(STD_INPUT_HANDLE),
    };

    DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    WSACloseEvent(sockEvent);

    if (result == WAIT_OBJECT_0) {
        // Socket is ready to recv
        had_recv = true;
    }
    else if (result == WAIT_OBJECT_0 + 1) {
        // Stdin has input
        // FIXME: std::getline may still block!
        had_stdin = true;
    }
    else {
        std::cerr << "WaitForMultipleObjects() error" << std::endl;
        return false;
    }
#else // _WIN32
    /*
     * Linux: Use select on m_socket and STDIN_FILENO
     */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(m_socket, &readfds);

    // FIXME: This wouldn't work on Windows
    FD_SET(STDIN_FILENO, &readfds);  // Keyboard input
    int max_fd = std::max(m_socket, STDIN_FILENO);

    /*
     * Wait for data from std::cin or socket
     */
    int activity = select(max_fd + 1, &readfds, nullptr, nullptr, nullptr);
    if (activity < 0) {
        std::cerr << "select() error " << errno << std::endl;
        return false;
    }

    had_recv = FD_ISSET(m_socket, &readfds);
    had_stdin = FD_ISSET(STDIN_FILENO, &readfds);
#endif // _WIN32
    return true;
}

bool Connection::send_protobuf(const PBMessage &message) {
    std::string buffer(message.SerializeAsString());

#ifdef _WIN32
    int flags = 0;
#else // _WIN32
    int flags = MSG_NOSIGNAL;   // Avoid SIGPIPE crash when peer was closed prematurelly
#endif // _WIN32

    // Send size
    uint32_t len = htonl(buffer.size());
    if (send_all(&len, sizeof(len), flags) < 0) {
        return false;
    }

    // Send serialied message
    if (send_all(buffer.data(), buffer.size(), flags) < 0) {
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

bool connection_startup() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
#endif
    return true;
}

void connection_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// Connect to listening server socket
int connect_to_server(const std::string &host, int port) {
    // Parse server-host using getaddrinfo
    addrinfo hints{0}, *res;
    hints.ai_family = SERVER_SOCKET_FAMILY;
    hints.ai_socktype = SERVER_SOCKET_TYPE;

    int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return 1;
    }

    // Create socket using getaddrinfo results
    int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socket_fd < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return -1;
    }

    if (connect(socket_fd, res->ai_addr, res->ai_addrlen) < 0) {
        std::cerr << "Server connect failed: " << strerror(errno) << std::endl;
        socket_close(socket_fd);
        return -1;
    }

    return socket_fd;
}

// Create and configure server socket
int create_server_socket(int port, int max_clients) {
    int server_fd = socket(SERVER_SOCKET_FAMILY, SERVER_SOCKET_TYPE, 0);
    if (server_fd < 0) {
        std::cerr << "Socket creation failed " << errno << std::endl;
        return -1;
    }

#if 0 // Avoid "address already in use" errors
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in address{0};
    address.sin_family = SERVER_SOCKET_FAMILY;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        socket_close(server_fd);
        return -1;
    }

    if (listen(server_fd, max_clients) < 0) {
        std::cerr << "Listen failed " << errno << std::endl;
        socket_close(server_fd);
        return -1;
    }

    return server_fd;
}
