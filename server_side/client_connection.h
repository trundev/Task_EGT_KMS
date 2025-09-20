/*
 * ClientConnection class declaration
 */
#include "../common/connection.h"


class UserData;

 class ClientConnection : public Connection {
    UserData *m_user = nullptr;
    std::chrono::steady_clock::time_point m_connected_at;
    std::string m_discon_reason;

    // Override Connection::recv_all to set disconnect reason
    virtual ssize_t recv_all(void* data, size_t len, int flags);

public:
    ClientConnection(int socket_fd);
    ~ClientConnection();

    bool do_login(const std::string &user_name);
    void kickout(const std::string &reason);

    std::string get_user_name() const;
    bool is_admin() const;
    std::string get_info() const;
    const std::string &get_disconnect_reason() const { return m_discon_reason;}
};
