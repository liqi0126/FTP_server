#include "utils.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
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
#include <sys/types.h>
#include <unistd.h>

#include "client.h"
#include "server.h"

/*--------------------------setup server---------------------------------*/
int setup_server(int argc, char **argv, Server *server) {
    if (!parse_port_and_root(argc, argv, server)) {
        return 0;
    }

    setup_local_addr(&server->addr, server->port);
    if (!setup_listen_socket(&server->control_sockfd, server->addr)) {
        return 0;
    }
    return 1;
}

int parse_port_and_root(int argc, char **argv, Server *server) {
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

    // setup root path
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

void setup_local_addr(struct sockaddr_in *addr, int port) {
    // setup ip and port
    memset(addr, '\0', sizeof(&addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(INADDR_ANY);  // IP adress of the machine on which the server is running.
}

int setup_listen_socket(int *sockfd, struct sockaddr_in addr) {
    // create socket
    // AF_INET: Internet domain
    // SOCK_STREAM: stream socket (a continuous stream, for TCP)
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd == -1) {
        printf("Error: fail to setup socket, error msg: %s(%d)\n", strerror(errno), errno);
        return 0;
    }

    // bind socket with ip
    if (bind(*sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        printf("Error: fail to bind socket, error msg: %s(%d)\n", strerror(errno), errno);
        return 0;
    }

    // listen socket
    if (listen(*sockfd, BACKLOG) == -1) {
        printf("Error: fail to listen socket, error msg: %s(%d)\n", strerror(errno), errno);
        return 0;
    }
    return 1;
}

int setup_addr(struct sockaddr_in *addr, char *ip, int port) {
    // setup ip and port
    memset(addr, '\0', sizeof(&addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
        return 0;
    }
    return 1;
}

int setup_connect_socket(int *sockfd, struct sockaddr_in addr) {
    // create socket
    // AF_INET: Internet domain
    // SOCK_STREAM: stream socket (a continuous stream, for TCP)
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd == -1) {
        printf("Error: fail to setup socket, error msg: %s(%d)\n", strerror(errno), errno);
        return 0;
    }

    // connect socket
    if (connect(*sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        printf("Error: fail to connect, error msg: %s(%d)\n", strerror(errno), errno);
        return 0;
    }
    return 1;
}

/*--------------------------communicate with client---------------------------------*/

int receive_request_from_client(Client *client) {
    char buf[BUF_SIZE];
    memset(buf, '\0', sizeof(buf));
    if (recv(client->control_sockfd, buf, BUF_SIZE, 0) < 0) {
        printf("fail to receive msg from client.\n");
        return 0;
    }
    sscanf(buf, "%s %s", client->command, client->argu);
    return 1;
}

void send_msg_to_client(char *buffer, Client *client) {
    int len = strlen(buffer);
    if (buffer[len - 1] != '\n' || buffer[len - 2] != '\r') {
        strcat(buffer, "\r\n");
    }
    if (send(client->control_sockfd, buffer, strlen(buffer), 0) < 0) {
        printf("Error: fail to send msg to client.\n");
    }
}

int send_file(int sockfd, FILE *fp, int offset) {
    if (!fp) {
        printf("Error: cannot open file\n");
        return -1;
    }

    if (offset > 0) {
        fseek(fp, offset, SEEK_SET);
    }

    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    int total = 0;
    int size = fread(buf, 1, BUF_SIZE, fp);
    total += size;
    while (size > 0) {
        send(sockfd, buf, size, 0);
        size = fread(buf, 1, BUF_SIZE, fp);
        total += size;
    }
    return total;
}

int send_file_by_path(int sockfd, char *file_path, int offset) {
    FILE *fp = fopen(file_path, "rb");
    return send_file(sockfd, fp, offset);
}

int receive_file(int sockfd, FILE *fp, int offset) {
    if (!fp) {
        printf("Error: cannot open file\n");
        return -1;
    }

    if (offset > 0) {
        fseek(fp, offset, SEEK_SET);
    }

    char buf[BUF_SIZE];
    int total = 0;
    int size = recv(sockfd, buf, BUF_SIZE, 0);
    total += size;
    while (size > 0) {
        fwrite(buf, 1, size, fp);
        size = recv(sockfd, buf, BUF_SIZE, 0);
        total += size;
    }
    fclose(fp);
    return total;
}

int receive_file_by_path(int sockfd, char *file_path, int offset) {
    FILE *fp = NULL;
    if (offset > 0) {
        fp = fopen(file_path, "r+");
    } else {
        fp = fopen(file_path, "w");
    }
    return receive_file(sockfd, fp, offset);
}

/*---------------------------IP related--------------------------------*/
int check_ip_and_port_by_hp(int h1, int h2, int h3, int h4, int p1, int p2) {
    if ((h1 < 0 || h1 > 255) ||
        (h2 < 0 || h2 > 255) ||
        (h3 < 0 || h3 > 255) ||
        (h4 < 0 || h4 > 255) ||
        (p1 < 0 || p1 > 255) ||
        (p2 < 0 || p2 > 255)) {
        return 0;
    }
    return 1;
}

int check_ip_and_port_by_ip(char *ip, int port) {
    int h1 = -1, h2 = -1, h3 = -1, h4 = -1, p1 = -1, p2 = -1;
    sscanf(ip, "%d.%d.%d.%d", &h1, &h2, &h3, &h4);
    p1 = port / 256;
    p2 = port % 256;
    return check_ip_and_port_by_hp(h1, h2, h3, h4, p1, p2);
}

int parse_ip_and_port(char *param, char *ip, int *port) {
    int h1 = -1, h2 = -1, h3 = -1, h4 = -1, p1 = -1, p2 = -1;
    sscanf(param, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
    if (!check_ip_and_port_by_hp(h1, h2, h3, h4, p1, h2)) {
        return 0;
    } else {
        sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
        *port = (p1 << 8) + p2;
        return 1;
    }
}

int parse_addr(char *param, struct sockaddr_in *addr) {
    char ip[20];
    int port;
    if (!parse_ip_and_port(param, ip, &port)) {
        return 0;
    }
    if (!setup_addr(addr, ip, port)) {
        return 0;
    }
    return 1;
}

int decorate_ip_and_port(char *param, char *ip, int port) {
    int h1 = -1, h2 = -1, h3 = -1, h4 = -1, p1 = -1, p2 = -1;
    sscanf(ip, "%d.%d.%d.%d", &h1, &h2, &h3, &h4);
    p1 = port / 256;
    p2 = port % 256;
    if (!check_ip_and_port_by_hp(h1, h2, h3, h4, p1, h2)) {
        return 0;
    } else {
        sprintf(param, "%d,%d,%d,%d,%d,%d", h1, h2, h3, h4, p1, p2);
        return 1;
    }
}

int decorate_addr(char *param, struct sockaddr_in addr) {
    char ip[20];
    int port;
    parse_ip_and_port_from_addr(addr, ip, &port);
    return decorate_ip_and_port(param, ip, port);
}

int get_host_ip(char *ip) {
    char hostbuffer[256];
    struct hostent *host_entry;
    int hostname;

    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    if (hostname == -1) {
        perror("gethostname");
        return 0;
    }
    host_entry = gethostbyname(hostbuffer);
    if (host_entry == NULL) {
        perror("gethostbyname");
        return 0;
    }
    char *host_ip = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));
    if (!check_ip_and_port_by_ip(host_ip, 0)) {
        return 0;
    }

    strcpy(ip, host_ip);
    return 1;
}

int gen_random_port() {
    // [0, at least 65536 (2^16))
    int port = (rand() << 1) + rand() % 2;
    // [0, 45536)
    port = port % 45536;
    // [20000, 65536)
    port += 20000;
    return port;
}

void parse_ip_and_port_from_addr(struct sockaddr_in addr, char *ip, int *port) {
    char *ipp = inet_ntoa(addr.sin_addr);
    strcpy(ip, ipp);
    *port = htons(addr.sin_port);
}

void print_ip_and_port(struct sockaddr_in addr) {
    printf("(%s:%d) ", inet_ntoa(addr.sin_addr), htons(addr.sin_port));
}
/*---------------------------File related--------------------------------*/

void get_full_path(char *full_path, char *root_path, char *cur_path, char *argu_path) {
    strcpy(full_path, root_path);
    if (full_path[strlen(full_path) - 1] != '/') {
        strcat(full_path, "/");
    }

    for (int i = strlen(argu_path); i >= 0; i--) {
        if (argu_path[i] == '\r' || argu_path[i] == '\n') {
            argu_path[i] = '\0';
        } else {
            break;
        }
    }

    if (strlen(argu_path) > 0 && argu_path[0] == '/') {
        strcat(full_path, argu_path + 1);
        return;
    }

    if (strlen(cur_path) > 0) {
        if (cur_path[0] == '/') {
            strcat(full_path, cur_path + 1);
        } else {
            strcat(full_path, cur_path);
        }
    }

    if (full_path[strlen(full_path) - 1] != '/') {
        strcat(full_path, "/");
    }

    strcat(full_path, argu_path);
}

void path_concat(char *new_path, char *cur_path, char *next_path) {
    strcpy(new_path, cur_path);
    if (new_path[strlen(new_path) - 1] != '/') {
        strcat(new_path, "/");
    }
    for (int i = strlen(next_path); i >= 0; i--) {
        if (next_path[i] == '\r' || next_path[i] == '\n') {
            next_path[i] = '\0';
        } else {
            break;
        }
    }

    if (strlen(next_path) > 0 && next_path[0] == '/') {
        strcat(new_path, next_path + 1);
    } else {
        strcat(new_path, next_path);
    }
}

int get_file_size(char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    fseek(f, 0L, SEEK_END);
    int size = ftell(f);
    fclose(f);
    return size;
}