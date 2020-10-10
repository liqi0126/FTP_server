#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>

/*-------------------------------MARCO------------------------------------*/

enum ClientState {
    DISCONNECT,
    CONNECT,
    USER,
    PASS,
};

enum ClientMode {
    PORT;
    PASV;
};


/*-------------------------------STRUCT-----------------------------------*/
typedef struct Client {
    // basic config
    struct sockaddr_in addr;
    socklen_t addrlen;
    int control_sockfd;

    // status
    enum ClientState state;
    enum ClientMode mode;

    // command
    char command[100];
    char argu[100];
} Client;


#endif