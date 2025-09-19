/*
 * Socket connection helper
 */


// Protobuf message forward declaration
class PBMessage;

class Connection {
    int m_socket;

    ssize_t send_all(const void* data, size_t len, int flags = 0);
    ssize_t recv_all(void* data, size_t len, int flags = 0);

public:
    Connection(int socket_fd);
    ~Connection();

    int get_socket() const { return m_socket;}
    void force_shutdown();
    std::string get_peer_name() const;

    bool send_protobuf(const PBMessage &message);
    bool recv_protobuf(PBMessage &message);

    // Function to overload operator<<
    friend std::ostream& operator<<(std::ostream& os, const Connection& obj);
};
