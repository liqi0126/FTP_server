#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
/*-------------------------------MARCO------------------------------------*/

enum ClientState {
    DISCONNECT,
    CONNECT,
    USER,
    PASS,
    PORT,
    PASV
};

/*-------------------------------STRUCT-----------------------------------*/
typedef struct Client {
    // basic config
    struct sockaddr_in addr;
    int control_sockfd;
    char cur_path[100];

    // status
    enum ClientState state;

    // command
    char command[100];
    char argu[100];

    // file transfer
    char file_addr[100];
    int file_port;
    int file_sockfd;
    int offset;

    // file rename
    char src_file[100];
    char dst_file[100];

    // statistics
    int sent_bytes;
    int sent_files;
} Client;

#endif