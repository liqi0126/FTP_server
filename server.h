#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <sys/types.h>

/*-------------------------------MARCO------------------------------------*/
#define DEFAULT_PORT 21
#define DEFAULT_ROOT_PATH "/tmp/"
#define BACKLOG 5
#define BUF_SIZE 8192

/*-------------------------------STRUCT-----------------------------------*/
typedef struct Server {
    // basic config
    int port;
    char root_path[100];
    struct sockaddr_in addr;
    socklen_t addrlen;
    int control_sockfd;
} Server;

/*------------------------------FUNCTIONS---------------------------------*/
// setup server
int setup_server(int argc, char **argv, Server *server);
int setup_port_and_root(int argc, char **argv, Server *server);
int setup_socket(Server *server);
// communicate with client
void send_msg_to_client(char *buffer, Client *client);
int receive_request_from_client(Client *client);

#endif