/*
 * ClientConnection class declaration
 */
#include "../common/connection.h"


class ClientConnection : public Connection {
public:
    ClientConnection(int socket_fd);

    std::string get_user_name() const;
};
