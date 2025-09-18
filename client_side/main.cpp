#include <iostream>
#include <cstring>
#include <format>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "../common/defines.h"
#include "../common/connection.h"


// Connect to listening server socket
static int connect_to_server(const std::string &host, int port) {
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
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

// Run client loop
int client_loop(Connection &server, const std::string &user_name) {
    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server.get_socket(), &readfds);

        // FIXME: This wouldn't work on Windows
        FD_SET(STDIN_FILENO, &readfds);  // Keyboard input
        int max_fd = std::max(server.get_socket(), STDIN_FILENO);

        /*
         * Wait for data from std::cin or socket
         */
        int activity = select(max_fd + 1, &readfds, nullptr, nullptr, nullptr);
        if (activity < 0) {
            std::cerr << "select() error " << errno << std::endl;
            break;
        }

        if (FD_ISSET(server.get_socket(), &readfds)) {
            // Receive data from socket
            std::array<char, MAX_MESSAGE_SIZE> buffer;
            ssize_t bytes = server.recv(buffer.data(), buffer.size());
            if (bytes <= 0) {
                // Server shutdown or error
                if (bytes < 0) {
                    std::cerr << "recv() error " << strerror(errno) << std::endl;
                }
                else {
                    std::cout << "Server was shutdown" << std::endl;
                }
                break;
            }
            std::string message_str(buffer.data(), bytes);

            // Send the data to console
            std::cout << message_str << std::endl;
        }
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            // Receive data from console
            std::string input;
            std::cin >> input;
            // Check for EOF (Ctrl-D)
            if (input.size() == 0 && std::cin.eof()) {
                std::cout << "End of input reached" << std::endl;
                break;
            }

            // Send the data to socket
            ssize_t bytes = server.send_all(input.c_str(), input.size());
            if (bytes < 0) {
                std::cerr << "send() error " << errno << std::endl;
                break;
            }
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    std::cout << "Chat client application" << std::endl;

    if (argc != 3) {
        std::cerr << std::format("Usage:\n{} <server> <user>", argv[0]) << std::endl;
        return 255;
    }
    std::string server_host(argv[1]);
    std::string user_name(argv[2]);

    std::cout << std::format("Connecting chat client to {} as user {}", server_host, user_name) << std::endl;

    int socket_fd = connect_to_server(server_host, SERVER_PORT);
    if (socket_fd < 0) {
        return socket_fd;
    }
    Connection server(socket_fd);

    // First send the user-name
    std::string data = user_name + "\n";
    ssize_t res = server.send_all(data.c_str(), data.size());
    if (res < 0) {
        std::cerr << "Socket send failed" << std::endl;
    }
    // Then run loop
    else {
        res = client_loop(server, user_name);
    }

    return res;
}
