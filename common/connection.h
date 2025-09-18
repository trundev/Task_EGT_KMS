/*
 * Socket connection helper
 */

class Connection {
    int m_socket;

public:
    Connection(int socket_fd);
    ~Connection();

    int get_socket() const { return m_socket;}

    ssize_t recv(void* data, size_t len, int flags = 0);
    ssize_t send_all(const void* data, size_t len, int flags = 0);
    //TODO: ssize_t recv_all(const void* data, size_t len, int flags = 0);

    // Function to overload operator<<
    friend std::ostream& operator<<(std::ostream& os, const Connection& obj);
};
