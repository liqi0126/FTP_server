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
int setup_server(int argc, char **argv);
int setup_root_and_port(int argc, char **argv);

#endif