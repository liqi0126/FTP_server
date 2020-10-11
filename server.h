#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "client.h"

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
    int control_sockfd;
} Server;

/*------------------------------FUNCTIONS---------------------------------*/
void user(Client *client);
void pass(Client *client);
void retr_port(Client *client);
void retr_pasv(Client *client);
void retr(Client *client);
void stor_port(Client *client);
void stor_pasv(Client *client);
void stor(Client *client);
void quit(Client *client);
void syst(Client *client);
void type(Client *client);
void port(Client *client);
void pasv(Client *client);
void mkd(Client *client);
void cwd(Client *client);
void pwd(Client *client);
void list_port(Client *client);
void list_pasv(Client *client);
void list(Client *client);
void rmd(Client *client);
void rnfr(Client *client);
void rnto(Client *client);
void listen_to_client(Client *client);
#endif