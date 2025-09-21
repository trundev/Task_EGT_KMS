/*
 * Common server/client declarations
 */
#define SERVER_SOCKET_FAMILY    AF_INET
#define SERVER_SOCKET_TYPE      SOCK_STREAM
#define SERVER_PORT 8080

#define MAX_CLIENTS 10
#define MAX_MESSAGE_SIZE 1024

#define CLIENT_DISCONNECT_TIMEOUT 10*60

// Select logger file by rounding timestaps
#define LOGFILE_TIME_ROUND  std::chrono::hours
