#include <iostream>
#include <list>
#include <array>
#include <cstring>
#include <chrono>
#include <format>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>

#include "../common/defines.h"
#include "client_connection.h"
#include "user_data.h"


// Create and configure server socket
static int create_server_socket(int port, int max_clients) {
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
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, max_clients) < 0) {
        std::cerr << "Listen failed " << errno << std::endl;
        close(server_fd);
        return -1;
    }

    return server_fd;
}

static std::string format_client_message(const ClientConnection &client,
    const std::string &message, ClientConnection::recv_status_t status) {

    // Get current system time
    auto now = std::chrono::system_clock::now();

    // Convert to time_t for formatting
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    // Convert to local time
    std::tm local_tm = *std::localtime(&now_c);

    // Format the result message
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    switch (status) {
        case ClientConnection::RECV_OK:
            return std::format("{} {}:\n {}", oss.str(), client.get_user_name(), message);

        case ClientConnection::RECV_CONNECT:
            return std::format("{} {}: connected", oss.str(), client.get_user_name());

        case ClientConnection::RECV_DISCONNECT:
            return std::format("{} {}: disconnected", oss.str(), client.get_user_name());

        default:
        case ClientConnection::RECV_ERROR:
            return std::format("{} {}: error", oss.str(), client.get_user_name());
    }
}

static bool handle_client_data(ClientConnection &client,
    std::list<ClientConnection> &client_connections,
    bool suppress_echo=true) {

    // Receive message from a client
    std::string message;
    ClientConnection::recv_status_t status = client.recv_message(message);
    if (status == ClientConnection::RECV_ERROR) {
        return false;
    }

    message = format_client_message(client, message, status);

    // Send data to all "other" clients (all clients - if suppress_echo is not set)
    for (ClientConnection &out_client: client_connections) {
        if (suppress_echo && &out_client == &client) {
            continue;
        }

        if (out_client.send_all(message.c_str(), message.size()) < 0) {
            std::cerr << out_client << ": send() error " << errno << std::endl;
        }
    }

    return status != ClientConnection::RECV_DISCONNECT;
}

// Run server loop
int server_loop(Connection &server) {
    // All connected clients
    std::list<ClientConnection> client_connections;

    int delay_sec = 10;

    while (true) {
        fd_set readfds, exceptfds;
        FD_ZERO(&readfds);
        FD_ZERO(&exceptfds);

        int server_fd = server.get_socket();
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;
        for (const ClientConnection &client : client_connections) {
            int client_fd = client.get_socket();
            FD_SET(client_fd, &readfds);
            max_fd = std::max(max_fd, client_fd);
        }

        // Wait up to delay_sec
        struct timeval timeout{
            .tv_sec = delay_sec,
            .tv_usec = 0,
        };

        /*
         * Wait for data (or error) from client sockets or server (new client)
         */
        int activity = select(max_fd + 1, &readfds, nullptr, &exceptfds, &timeout);
        if (activity < 0) {
            std::cerr << "select() error " << errno << std::endl;
            return -1;
        }
        else if (activity == 0) {
            std::cout << std::format("No data within {:.2f} minutes", delay_sec/60.) << std::endl;
        }

        // Check for server data
        if (FD_ISSET(server_fd, &readfds)) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            std::cout << "New client connected: " << client_fd << "\n";

            // Construct ClientConnection object in-place using client_fd
            client_connections.emplace_back(client_fd);
        }
        // Check for server error
        if (FD_ISSET(server_fd, &exceptfds)) {
            std::cerr << "Exceptional condition returned on the server socket" << std::endl;
            return -1;
        }

        // Check for client data / error
        for (auto it = client_connections.begin(); it != client_connections.end(); ) {
            auto& client = *it;
            int client_fd = client.get_socket();
            auto next_it = std::next(it);

            // In case of error, remove client
            if (FD_ISSET(client_fd, &exceptfds)) {
                std::cerr << "Exceptional condition returned on client: " << client << std::endl;
                next_it = client_connections.erase(it);
            }
            // Handle received data
            else if (FD_ISSET(client_fd, &readfds)) {
                //std::cout << "Data received from client: " << client << std::endl;
                if (!handle_client_data(client, client_connections)) {
                    // Remove client if disconnected or receive failure
                    next_it = client_connections.erase(it);
                }
            }

            it = next_it;
        }
    }

    return 0;
}

int main() {
    std::cout << "Start server application on port " << SERVER_PORT << std::endl;

    // Create/bind server socket
    int server_fd = create_server_socket(SERVER_PORT, MAX_CLIENTS);
    if (server_fd < 0) {
        return 255;
    }
    Connection server(server_fd);

    // Run the main loop
    int ret = server_loop(server);

    return ret;
}
