#include <list>
#include <format>
#include <thread>
#include <fstream>
#include <chrono>
#include <google/protobuf/util/time_util.h>

#include "../common/defines.h"
#include "client_connection.h"
#include "logger.h"
#include "user_data.h"
#include "messages.pb.h"


#define USER_DATABASE_FILE  "user_database.db"


// Global flag to
bool g_server_running = true;

// All connected clients
// Use std::list to avoid move of ClientConnection objects in memory and
// to allow keeping iterators for the whole object lifecycle
typedef std::list<ClientConnection> ConnectionList;
ConnectionList client_connections;
std::mutex clients_mutex;

bool kickout_client(ClientConnection &by_client, const std::string &user_name, bool kick_all=false) {
    bool client_found = false;

    std::string reason = std::format("kicked out by {}", by_client.get_user_name());

    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &client: client_connections) {
        if (kick_all || client.get_user_name() == user_name) {
            client.kickout(reason);
            client_found = true;
        }
    }
    return client_found;
}

bool make_admin(ClientConnection &by_client, const std::string &user_name) {
    bool res = by_client.make_user(user_name, true);
    if (res) {
        Logger::log("[SYSTEM] {}: Is now admin", user_name);
    }
    return res;
}

/*
 * Map of command-string to call-back function
 */
const std::map<const std::string,
         std::function<bool(const PBChatCommand &command,
                            ClientConnection &client,
                            PBCommandResult &result)>>
    g_command_map = {
    /*
     * !help command
     */
    {"help", [](const PBChatCommand &command, ClientConnection &client, PBCommandResult &result) {
        result.add_text("Available commands:");
        result.add_text(" !help");
        result.add_text(" !quit");
        result.add_text(" !list");
        result.add_text(" !kickout");
        result.add_text(" !make-admin");
        return true;
    }},
    /*
     * !quit command
     */
    {"quit", [](const PBChatCommand &command, ClientConnection &client, PBCommandResult &result) {
        if (!client.is_admin()) {
            result.add_text("Unathorized operation");
            return false;
        }
        // TODO: Still need to wakeup server_loop
        g_server_running = false;
        // HACK: Wakeup all client threads
        kickout_client(client, "", true);
        result.add_text("quiting server...");
        return true;
    }},
    /*
     * !list command
     */
    {"list", [](const PBChatCommand &command, ClientConnection &_, PBCommandResult &result) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        result.add_text(std::format("{} connections:", client_connections.size()));
        for (const auto &client: client_connections) {
            //TODO: More connection details
            result.add_text(std::format("  {}", client.get_info()));
        }
        return true;
    }},
    /*
     * !kickout command
     */
    {"kickout", [](const PBChatCommand &command, ClientConnection &client, PBCommandResult &result) {
        if (!client.is_admin()) {
            result.add_text("Unathorized operation");
            return false;
        }
        const auto &user_name = command.parameter();
        bool client_found = kickout_client(client, user_name);
        result.add_text(client_found ?
                std::format("{} kicked out", user_name) :
                std::format("User '{}' is not connected", user_name));
        return true;
    }},
    /*
     * !make-admin command
     */
    {"make-admin", [](const PBChatCommand &command, ClientConnection &client, PBCommandResult &result) {
        if (!client.is_admin()) {
            result.add_text("Unathorized operation");
            return false;
        }
        const auto &user_name = command.parameter();
        bool user_found = make_admin(client, user_name);
        result.add_text(user_found ?
                std::format("{} is now admin", user_name) :
                std::format("Can't create user '{}'", user_name));
        if (user_found) {
            std::cout << client << ": " << user_name << " is now admin" << std::endl;
        }
        return user_found;
    }},
};

static bool run_command(const PBChatCommand &command, ClientConnection &from_client) {
    Logger::log("[SYSTEM] {}: Invoking command: {} {}", from_client.get_user_name(),
            command.command(), command.parameter());

    // Obtain command call-back from the global map
    auto it = g_command_map.find(command.command());

    PBMessage message;
    message.mutable_result()->set_command(command.command());
    if (it != g_command_map.end()) {
        // Invoke the command
        auto result = it->second(command, from_client, *message.mutable_result());
    }
    else {
        // Reply with unsupported command result
        message.mutable_result()->add_text(std::format(
                "Unsupported command '{}'", command.command()));
    }
    return from_client.send_protobuf(message);
}

static void prepare_chat_message(PBChatMessage &chat) {
    const google::protobuf::Timestamp now = google::protobuf::util::TimeUtil::GetCurrentTime();
    chat.mutable_sent_at()->CopyFrom(now);
}

static bool do_login(UserDatabase *database,
        const PBUserLogin &login, ClientConnection &client) {
    bool success = client.do_login(database, login.user_name());

    PBMessage message;
    prepare_chat_message(*message.mutable_chat());
    if (success) {
        Logger::log("[SYSTEM] {}: Login (is_admin {})", client.get_user_name(), client.is_admin());

        message.mutable_chat()->set_text(std::format(
                "Hello {}, Type !help to see avaible commands", login.user_name()));

        std::cout << client << ": login " << login.user_name() << ", is_admin: " << client.is_admin() << std::endl;
    }
    else {
        message.mutable_chat()->set_text(std::format(
                "Can't login {}", login.user_name()));
    }

    if (!client.send_protobuf(message)) {
        success = false;
    }
    return success;
}

bool broadcast_chat(const PBChatMessage &chat,
        ClientConnection &from_client,
        bool suppress_echo=true) {
    Logger::log("[CHAT] {}: {} ", from_client.get_user_name(), chat.text());

    // Prepare message to broadcast
    PBMessage message;
    prepare_chat_message(*message.mutable_chat());
    message.mutable_chat()->set_from_user(from_client.get_user_name());
    message.mutable_chat()->set_text(chat.text());

    // Send to all "other" clients (w/o suppress_echo - all clients)
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &client: client_connections) {
        if (suppress_echo && &client == &from_client) {
            continue;
        }
        client.send_protobuf(message);
    }
    return true;
}

// Loop to handle specific client
void client_connection_loop(std::shared_ptr<UserDatabase> database,
        ConnectionList::iterator client_it) {
    ClientConnection &client = *client_it;

    while (true) {
        PBMessage message;
        if (!client.recv_protobuf(message)) {
            break;
        }

        if (message.has_chat()) {
            // Store chat message in user data-base
            PBChatMessage &chat = *message.mutable_chat();
            prepare_chat_message(chat);
            client.store_chat(chat);

            if (!broadcast_chat(chat, client)) {
                // TODO: Send chat failed
            }
        }
        else if (message.has_command()) {
            if (!run_command(message.command(), client)) {
                // TODO: Send command result failed
            }
        }
        else if (message.has_login()) {
            if (!do_login(database.get(), message.login(), client)) {
                client.force_shutdown();
            }
        }
        else {
            std::cerr << client << ": Unexpected protobuf message payload case: "
                    << message.payload_case() << std::endl;
        }
    }

    std::cout << client << ": disconnected " << client.get_user_name() << std::endl;

    // Explain why was disconnected
    auto discon_reason = client.get_disconnect_reason();
    if (discon_reason.size()) {
        Logger::log("[SYSTEM] {}: {}", client.get_user_name(), discon_reason);

        PBMessage message;
        prepare_chat_message(*message.mutable_chat());
        message.mutable_chat()->set_text(discon_reason);
        client.send_protobuf(message);
    }

    auto user_name = client.get_user_name();
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client_connections.erase(client_it);
        // Note: client object is no longer valid
    }

    Logger::log("[SYSTEM] {}: Disconnected", user_name);
}

// Run server loop
int server_loop(Connection &server) {
    auto user_database = open_user_dadabase(USER_DATABASE_FILE);

    Logger::log("[SYSTEM] Server started, port {}", SERVER_PORT);

    while (g_server_running) {
        int client_fd = server.accept();
        ConnectionList::iterator client_it;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            // Construct ClientConnection object in-place using client_fd
            client_connections.emplace_back(client_fd);
            client_it = std::prev(client_connections.end());
        }

        std::thread(client_connection_loop, user_database, client_it).detach();
    }

    Logger::log("[SYSTEM] Server stopped");
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
