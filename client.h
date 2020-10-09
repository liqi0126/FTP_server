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
    struct sockaddr_in addr;
    socklen_t addrlen;
    int control_socket;

    enum ClientState state;
    enum ClientMode mode;
} Client;


#endif