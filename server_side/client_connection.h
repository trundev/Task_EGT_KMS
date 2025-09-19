/*
 * ClientConnection class declaration
 */
#include "../common/connection.h"


class UserData;

 class ClientConnection : public Connection {
    UserData *m_user = nullptr;

public:
    enum recv_status_t {
        RECV_OK, RECV_CONNECT, RECV_DISCONNECT, RECV_ERROR
    };

    ClientConnection(int socket_fd);
    ~ClientConnection();

    std::string get_user_name() const;

    recv_status_t recv_message(std::string &message);
};
