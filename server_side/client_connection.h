/*
 * ClientConnection class declaration
 */
#include "../common/connection.h"


class UserData;

 class ClientConnection : public Connection {
    UserData *m_user = nullptr;
    std::chrono::steady_clock::time_point m_connected_at;

public:
    ClientConnection(int socket_fd);
    ~ClientConnection();

    bool do_login(const std::string &user_name);

    std::string get_user_name() const;
    bool is_admin() const;
    std::string get_info() const;
};
