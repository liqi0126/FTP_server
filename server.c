#include "server.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "client.h"
#include "utils.h"

/*--------------------------setup server---------------------------------*/
int setup_server(int argc, char **argv, Server *server) {
    if (!setup_port_and_root(argc, argv, server)) {
        return 0;
    }
    if (!setup_socket(server)) {
        return 0;
    }
    return 1;
}

int setup_port_and_root(int argc, char **argv, Server *server) {
    // setup port
    server->port = DEFAULT_PORT;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-port")) {
            if (i + 1 >= argc) {
                printf("Error: parameter for port not found\n");
                return 0;
            }
            int port = atoi(argv[i + 1]);
            if (!port) {
                printf("Error: wrong format for port number\n");
                return 0;
            }
            if (port > 65535) {
                printf("Error: port number cannot be larger than 65535\n");
                return 0;
            }
            server->port = port;
            break;
        }
    }

    // setup root
    strcpy(server->root_path, DEFAULT_ROOT_PATH);
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-root")) {
            if (i + 1 >= argc) {
                printf("Error: parameter for root not found\n");
                return 0;
            }
            strcpy(server->root_path, argv[i + 1]);
            DIR *dir = opendir(server->root_path);
            if (dir) {
                closedir(dir);
            } else {
                printf("Error: open directory %s failed\n", server->root_path);
                return 0;
            }
            break;
        }
    }
    return 1;
}

int setup_socket(Server *server) {
    // create socket
    // AF_INET: Internet domain
    // SOCK_STREAM: stream socket (a continuous stream, for TCP)
    server->control_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->control_sockfd == -1) {
        printf("Error: fail to setup socket, error msg: %s(%d)\n", strerror(errno), errno);
        return 0;
    }

    // setup ip and port
    server->addrlen = sizeof(server->addr);
    memset(&server->addr, '\0', server->addrlen);
    server->addr.sin_family = AF_INET;
    server->addr.sin_port = htons(server->port);
    server->addr.sin_addr.s_addr = htonl(INADDR_ANY);  // IP adress of the machine on which the server is running.

    // bind socket with ip
    if (bind(server->control_sockfd, (struct sockaddr *)&server->addr, server->addrlen) == -1) {
        printf("Error: fail to bind socket, error msg: %s(%d)\n", strerror(errno), errno);
        return 0;
    }

    // listen socket
    if (listen(server->control_sockfd, BACKLOG) == -1) {
        printf("Error: fail to listen socket, error msg: %s(%d)\n", strerror(errno), errno);
        return 0;
    }
    return 1;
}

/*--------------------------communicate with client---------------------------------*/
void send_msg_to_client(char *buffer, Client *client) {
    if (send(client->control_sockfd, buffer, strlen(buffer), 0) < 0) {
        print("Error: fail to send msg to client.\n");
    }
}

int receive_request_from_client(Client *client) {
    char buf[BUF_SIZE];
    memset(buf, '\0', sizeof(buf));
    if (recv(client->control_sockfd, buf, BUF_SIZE, 0) < 0) {
        printf("fail to receive msg from client.\n");
        return 0;
    }
    sscanf(buf, "%s %s", client->command, client->argu);  // TODO: check correctness
    return 1;
}

/*--------------------------main logic---------------------------------*/
void user(Client *client) {
    if (client->state == CONNECT) {
        if (!strcmp(client->argu, "anonymous")) {
            client->state = USER;
            send_msg_to_client("331 Guest login ok, send your complete e-mail address as password.", client);
        } else {
            //TODO:
            send_msg_to_client("", client);
        }
    } else {
        //TODO: ?
    }
}

void pass(Client *client) {
    if (client->state == USER) {
        client->state = PASS;
        send_msg_to_client("230 Login successful.", client);
    } else {
        // TODO: ?
    }
}

void listen_to_client(Client *client) {
    client->mode = CONNECT;
    send_msg_to_client("220 Anonymous FTP server ready.\r\n", client);
    while (1) {
        if (!receive_request_from_client(client)) {
            continue;
        }
        if (!strcmp(client->command, "USER")) {
            user(client);
        }
        elif (!strcmp(client->command, "PASS")) {
            pass(client);
        }
        elif (!strcmp(client->command, "SYST")) {
        }
    }
}

int main(int argc, char **argv) {
    Server server;
    // setup server
    if (!setup_server(argc, argv, &server)) {
        return -1;
    }
    while (1) {
        Client client;
        // waiting for client socket to connect
        client.control_sockfd = accept(server.control_sockfd, &client.addr, &client.addrlen);
        if (client.control_sockfd == -1) {
            printf("Error: fail to connect client, error msg: %s(%d)\n", strerror(errno), errno);
            return -1;
        }
        pid_t pid = fork();
        if (pid == 0) {
            close(server.control_sockfd);
            listen_to_client(&client);
            close(client.control_sockfd);
        } else if (pid < 0) {
            perror("Error: failed forking processes\n");
        }
        close(client.control_sockfd);
    }
    close(server.control_sockfd);
    return 0;
}
