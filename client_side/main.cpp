#include <sstream>
#include <iomanip>
#include <format>

#include "../common/defines.h"
#include "../common/connection.h"
#include "messages.pb.h"


// Send protobuffer message (command or chat) based on input string
static bool process_input(Connection &server, const std::string &input) {
    std::string buffer;
    PBMessage message;
    if (input.starts_with('!')) {
        // This is a commmand, split into command and parameter(s)
        std::stringstream ss(input.substr(1));
        std::string token;
        getline(ss, token, ' ');
        message.mutable_command()->set_command(token);
        if (getline(ss, token)) {;
            message.mutable_command()->set_parameter(token);
        }
    }
    else {
        // This is plain text message
        message.mutable_chat()->set_text(input);
    }

    // Send the data to socket
    return server.send_protobuf(message);
}

std::string format_chat_message(const PBChatMessage &chat) {
    // Send the chat info and text to console
    const google::protobuf::Timestamp& ts = chat.sent_at();
    std::time_t raw_time = static_cast<std::time_t>(ts.seconds());
    std::tm* local_tm = std::localtime(&raw_time);

    std::ostringstream oss;
    oss << std::put_time(local_tm, "%Y-%m-%d %H:%M:%S ") << chat.from_user() << ":" << std::endl;
    oss << " " << chat.text() << std::endl;
    return oss.str();
}

// Run client loop
int client_loop(Connection &server, const std::string &user_name) {
    bool had_recv, had_stdin;
    while (server.wait_recv_or_stdin(had_recv, had_stdin)) {
        if (had_recv) {
            // Receive message from socket
            PBMessage message;
            if (!server.recv_protobuf(message)) {
                break;
            }

            if (message.has_chat()) {
                // Send the chat info and text to console
                std::cout << format_chat_message(message.chat());
            }
            else if (message.has_result()) {
                // Send command results to console
                for (auto &text: message.result().text()) {
                    std::cout << "> " << text << std::endl;
                }
            }
            else {
                std::cerr << "Unexpected protobuf message payload case: "
                        << message.payload_case() << std::endl;
            }

        }
        if (had_stdin) {
            // Receive data from console
            std::string input;
            std::getline(std::cin, input);
            // Check for EOF (Ctrl-D)
            if (input.size() == 0 && std::cin.eof()) {
                std::cout << "End of input reached" << std::endl;
                break;
            }

            if (!process_input(server, input)) {
                break;
            }
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    std::cout << "Chat client application" << std::endl;
    if (!connection_startup()) {
        return 255;
    }

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
    int ret;
    {
        Connection server(socket_fd);

        // First send the user-login
        PBMessage message;
        message.mutable_login()->set_user_name(user_name);
        if (!server.send_protobuf(message)) {
            return 1;
        }

        // Then run loop
        ret = client_loop(server, user_name);
    }

    connection_cleanup();
    return ret;
}
