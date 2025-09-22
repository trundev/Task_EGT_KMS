/*
 * Socket connection helper
 */


// Protobuf message forward declaration
class PBMessage;

class Connection {
    int m_socket;

protected:
    virtual int send_all(const void* data, size_t len, int flags = 0);
    virtual int recv_all(void* data, size_t len, int flags = 0);
    bool is_last_error_timeout() const;

public:
    Connection(int socket_fd=0);
    ~Connection();

    int get_socket() const { return m_socket;}
    void force_shutdown();
    std::string get_peer_name() const;

    int accept();
    int set_recv_timeout(int seconds);
    // Wrapper around select() for socket and stdin
    bool wait_recv_or_stdin(bool &had_recv, bool &had_stdin);

    bool send_protobuf(const PBMessage &message);
    bool recv_protobuf(PBMessage &message);

    // Function to overload operator<<
    friend std::ostream& operator<<(std::ostream& os, const Connection& obj);
};

// Needed for Windows builds
bool connection_startup();
void connection_cleanup();

int connect_to_server(const std::string &host, int port);
int create_server_socket(int port, int max_clients);
