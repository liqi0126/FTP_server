#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <sys/socket.h>

/*-------------------------------MARCO------------------------------------*/
#define DEFAULT_PORT 21
#define DEFAULT_ROOT_PATH "/tmp/"
#define MAX_CONNECTION 100


/*-------------------------------STRUCT-----------------------------------*/
typedef struct Server {
    // basic config
    int port;
    char root_path[100];
    // addr
    struct sockaddr_in addr;
    socklen_t addrlen;
    int control_socket;      
} Server;


/*------------------------------FUNCTIONS---------------------------------*/
int setup_server(int argc, char ** argv);
int setup_root_and_port(int argc, char ** argv);

#endif