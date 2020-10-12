
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

#define DEBUG

void user(Client *client) {
    if (client->state == CONNECT || client->state == USER) {
        if (!strcmp(client->argu, "anonymous")) {
            client->state = USER;
            send_msg_to_client("331 Guest login ok, send your complete e-mail address as password.\r\n", client);
        } else {
            send_msg_to_client("530 Only support anonymous user.\r\n", client);
        }
    } else if (client->state == DISCONNECT) {
        send_msg_to_client("500 You are not connected.\r\n", client);
    } else {
        send_msg_to_client("500 You are already logged in.\r\n", client);
    }
}

void pass(Client *client) {
    if (client->state == USER) {
        client->state = PASS;
        send_msg_to_client("230 Login successful.\r\n", client);
    } else {
        send_msg_to_client("500 PASS command should follow USER command.\r\n", client);
    }
}

void retr_port(Client *client) {
    struct sockaddr_in addr;
    if (!setup_addr(&addr, client->file_addr, client->file_port)) {
        return;
    }
    if (!setup_connect_socket(&client->file_sockfd, addr)) {
        return;
    }

    char file_path[BUF_SIZE];
    path_concat(file_path, client->cur_path, client->argu);
    int size = get_file_size(file_path);

    char buf[BUF_SIZE];
    if (size <= 0) {
        sprintf(buf, "500 file %s not found.\r\n", client->argu);
        send_msg_to_client(buf, client);
        return;
    }

    sprintf(buf, "150 Opening BINARY mode data connection for %s (%d)\r\n", client->argu, size);
    send_msg_to_client(buf, client);

#ifdef DEBUG
    printf("begin send data\n");
#endif

    int total = send_file(client->file_sockfd, file_path, client->offset);
    if (total <= 0) {
        sprintf(buf, "500 fail to send %s\r\n", client->argu);
        send_msg_to_client(buf, client);
        return;
    }

#ifdef DEBUG
    printf("data sending finish.\n");
#endif

    client->sent_bytes += total;
    client->sent_files += 1;

    send_msg_to_client("226 Transfer complete.\r\n", client);
}

void retr_pasv(Client *client) {
    int sockfd = accept(client->file_sockfd, NULL, NULL);
    if (sockfd == -1) {
        send_msg_to_client("500 fail to connect client socket.\r\n", client);
        return;
    }
    char file_path[BUF_SIZE];
    path_concat(file_path, client->cur_path, client->argu);

    int size = get_file_size(file_path);

    char buf[BUF_SIZE];
    if (size <= 0) {
        sprintf(buf, "500 file %s not found.\r\n", client->argu);
        send_msg_to_client(buf, client);
        close(sockfd);
        return;
    }

    sprintf(buf, "150 Opening BINARY mode data connection for %s (%d)\r\n", client->argu, size);
    send_msg_to_client(buf, client);

    int total = send_file(sockfd, file_path, client->offset);
    if (total <= 0) {
        sprintf(buf, "500 fail to send %s\r\n", client->argu);
        send_msg_to_client(buf, client);
        close(sockfd);
        return;
    }
    client->sent_bytes += total;
    client->sent_files += 1;

    send_msg_to_client("226 Transfer complete.\r\n", client);
    close(sockfd);
}

void retr(Client *client) {
    if (client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 require PORT/PASV mode.\r\n", client);
        return;
    }
    if (client->state == PORT) {
        retr_port(client);
    } else {
        retr_pasv(client);
    }
    close(client->file_sockfd);
    client->state = PASS;
}

void stor_port(Client *client) {
    struct sockaddr_in addr;
    if (!setup_addr(&addr, client->file_addr, client->file_port)) {
        return;
    }
    if (!setup_connect_socket(&client->file_sockfd, addr)) {
        return;
    }

    char file_path[BUF_SIZE];
    path_concat(file_path, client->cur_path, client->argu);

    char buf[BUF_SIZE];
    sprintf(buf, "150 Opening BINARY mode data connection for %s\r\n", client->argu);
    send_msg_to_client(buf, client);

    int total = receive_file(client->file_sockfd, file_path);
    if (total <= 0) {
        sprintf(buf, "500 fail to receive %s\r\n", client->argu);
        send_msg_to_client(buf, client);
        return;
    }

    send_msg_to_client("226 Transfer complete.\r\n", client);
}

void stor_pasv(Client *client) {
    int sockfd = accept(client->file_sockfd, NULL, NULL);
    if (sockfd == -1) {
        send_msg_to_client("500 fail to connect client socket.\r\n", client);
    }

    char file_path[BUF_SIZE];
    path_concat(file_path, client->cur_path, client->argu);

    char buf[BUF_SIZE];
    sprintf(buf, "150 Opening BINARY mode data connection for %s.\r\n", client->argu);
    send_msg_to_client(buf, client);
    int total = receive_file(sockfd, file_path);
    if (total <= 0) {
        sprintf(buf, "500 fail to receive %s\r\n", client->argu);
        send_msg_to_client(buf, client);
        close(sockfd);
        return;
    }

    close(sockfd);
    send_msg_to_client("226 Transfer complete.\r\n", client);
}

void stor(Client *client) {
    if (client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 require PORT/PASV mode.\r\n", client);
        return;
    }
    if (client->state == PORT) {
        stor_port(client);
    } else {
        stor_pasv(client);
    }
    close(client->file_sockfd);
    client->state = PASS;
}

void quit(Client *client) {
    char buf[BUF_SIZE];
    sprintf(buf, "221-You have transferred %d bytes in %d files.", client->sent_bytes, client->sent_files);
    strcat(buf, "221-Thank you for using the FTP service.\r\n");
    strcat(buf, "221 Goodbye.\r\n");
    send_msg_to_client(buf, client);
}

void syst(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    send_msg_to_client("215 UNIX Type: L8\r\n", client);
}

void type(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    if (!strcmp(client->argu, "I")) {
        send_msg_to_client("200 Type set to I.\r\n", client);
    } else {
        send_msg_to_client("500 Only Type I is supported", client);
    }
}

void port(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    if (parse_addr_and_port(client->argu, client->file_addr, &client->file_port)) {
        client->state = PORT;
        send_msg_to_client("200 PORT command succesful.\r\n", client);
    } else {
        send_msg_to_client("504 unsupported parameter.\r\n", client);
    }
}

void pasv(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    if (!get_host_ip(client->file_addr)) {
        send_msg_to_client("500 fail to get host ip.\r\n", client);
        return;
    }
    client->file_port = gen_random_port();
    struct sockaddr_in addr;
    setup_local_addr(&addr, client->file_port);
    if (!setup_listen_socket(&client->file_sockfd, addr)) {
        send_msg_to_client("500 fail to setup file socket.\r\n", client);
        return;
    }
    char param[100];
    decorate_addr_and_port(param, client->file_addr, client->file_port);
    char buf[BUF_SIZE];
    sprintf(buf, "227 Entering Passive mode(%s)", param);
    send_msg_to_client(buf, client);
}

void mkd(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    char new_dir[BUF_SIZE];
    path_concat(new_dir, client->cur_path, client->argu);
    char buf[BUF_SIZE];
    if (mkdir(new_dir, 0700) == 0) {
        sprintf(buf, "257 %s created.\r\n", new_dir);
    } else {
        sprintf(buf, "550 %s create failed.\r\n", new_dir);
    }
    send_msg_to_client(buf, client);
}

void cwd(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    DIR *dir = opendir(client->argu);
    if (dir) {
        closedir(dir);
        if (client->argu[0] == '/') {
            strcpy(client->cur_path, client->argu);
        } else {
            char cur_path[BUF_SIZE];
            strcpy(cur_path, client->cur_path);
            path_concat(client->cur_path, cur_path, client->argu);
        }
        char buf[BUF_SIZE];
        sprintf(buf, "250 go to %s\r\n", client->cur_path);
        send_msg_to_client(buf, client);
    } else {
        char buf[BUF_SIZE];
        sprintf(buf, "550 %s: No such file or directory.\r\n", client->argu);
        send_msg_to_client(buf, client);
    }
}

void pwd(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    char buf[BUF_SIZE];
    sprintf(buf, "257 %s\r\n", client->cur_path);
    send_msg_to_client(buf, client);
}

void list_port(Client *client) {
    struct sockaddr_in addr;
    if (!setup_addr(&addr, client->file_addr, client->file_port)) {
        return;
    }
    if (!setup_connect_socket(&client->file_sockfd, addr)) {
        return;
    }

    system("ls -l | tail +1 > ls.txt");
    char ls_path[BUF_SIZE];
    path_concat(ls_path, client->cur_path, "ls.txt");

    send_msg_to_client("150 Opening BINARY mode data connection.\r\n", client);

    int total = send_file(client->file_sockfd, ls_path, 0);
    if (total <= 0) {
        send_msg_to_client("500 fail to send file lists.\r\n", client);
        return;
    }
    send_msg_to_client("226 Transfer complete.\r\n", client);
}

void list_pasv(Client *client) {
    int sockfd = accept(client->file_sockfd, NULL, NULL);
    if (sockfd == -1) {
        send_msg_to_client("500 fail to connect client socket.\r\n", client);
        return;
    }

    system("ls -l | tail +1 > ls.txt");
    char ls_path[BUF_SIZE];
    path_concat(ls_path, client->cur_path, "ls.txt");

    send_msg_to_client("150 Opening BINARY mode data connection.\r\n", client);

    int total = send_file(sockfd, ls_path, 0);
    if (total <= 0) {
        send_msg_to_client("500 fail to send file lists.\r\n", client);
        close(sockfd);
        return;
    }
    send_msg_to_client("226 Transfer complete.\r\n", client);
    close(sockfd);
}

void list(Client *client) {
    if (client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 require PORT/PASV mode.\r\n", client);
        return;
    }

    if (client->state == PORT) {
        list_port(client);
    } else {
        list_pasv(client);
    }
    close(client->file_sockfd);
    client->state = PASS;
}

void rmd(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    char path[BUF_SIZE];
    path_concat(path, client->cur_path, client->argu);

    char buf[BUF_SIZE];
    if (rmdir(path) == 0) {
        sprintf(buf, "250 %s directory removed.\r\n", path);
    } else {
        sprintf(buf, "500 remove %s failed.\r\n", path);
    }
    send_msg_to_client(buf, client);
}

void rnfr(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    path_concat(client->src_file, client->cur_path, client->argu);
    if (access(client->src_file, F_OK) == -1) {
        char buf[BUF_SIZE];
        sprintf(buf, "500 no such a file.\r\n");
        send_msg_to_client(buf, client);
        return;
    }
    send_msg_to_client("350 please specify the destination file name.\r\n", client);
}

void rnto(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    if (strlen(client->argu) == 0) {
        send_msg_to_client("501 file name cannot be empty.\r\n", client);
        return;
    }
    if (strlen(client->src_file) == 0) {
        send_msg_to_client("500 you need to specify the source file name first.\r\n", client);
        return;
    }
    path_concat(client->dst_file, client->cur_path, client->argu);

    char buf[BUF_SIZE];
    if (rename(client->src_file, client->dst_file) != 0) {
        sprintf(buf, "502 fail to rename file from %s to %s.\r\n", client->src_file, client->dst_file);
        send_msg_to_client(buf, client);
        return;
    }

    sprintf(buf, "250 file rename from %s to %s.\r\n", client->src_file, client->dst_file);
    send_msg_to_client(buf, client);
    memset(client->src_file, 0, strlen(client->src_file));
    memset(client->dst_file, 0, strlen(client->dst_file));
}

void listen_to_client(Client *client) {
    send_msg_to_client("220 Anonymous FTP server ready.\r\n", client);

    while (1) {
#ifdef DEBUG
        print_ip_and_port(client->addr);
        printf("state: ");
        switch (client->state) {
            case DISCONNECT:
                printf("DISCONNECT\n");
                break;
            case CONNECT:
                printf("CONNECT\n");
                break;
            case USER:
                printf("USER\n");
                break;
            case PASS:
                printf("PASS\n");
                break;
            case PORT:
                printf("PORT\n");
                break;
            case PASV:
                printf("PASV\n");
                break;
            default:
                break;
        }
#endif
        if (!receive_request_from_client(client)) {
            continue;
        }
        if (!strcmp(client->command, "USER")) {
            user(client);
        } else if (!strcmp(client->command, "PASS")) {
            pass(client);
        } else if (!strcmp(client->command, "RETR")) {
            retr(client);
        } else if (!strcmp(client->command, "STOR")) {
            stor(client);
        } else if (!strcmp(client->command, "QUIT")) {
            quit(client);
            break;
        } else if (!strcmp(client->command, "ABOR")) {
            quit(client);
            break;
        } else if (!strcmp(client->command, "SYST")) {
            syst(client);
        } else if (!strcmp(client->command, "TYPE")) {
            type(client);
        } else if (!strcmp(client->command, "PORT")) {
            port(client);
        } else if (!strcmp(client->command, "PASV")) {
            pasv(client);
        } else if (!strcmp(client->command, "MKD")) {
            mkd(client);
        } else if (!strcmp(client->command, "CWD")) {
            cwd(client);
        } else if (!strcmp(client->command, "PWD")) {
            pwd(client);
        } else if (!strcmp(client->command, "LIST")) {
            list(client);
        } else if (!strcmp(client->command, "RMD")) {
            rmd(client);
        } else if (!strcmp(client->command, "RNFR")) {
            rnfr(client);
        } else if (!strcmp(client->command, "RNTO")) {
            rnto(client);
        } else {
            send_msg_to_client("502 unsupported parameters.\r\n", client);
        }
    }
}

int main(int argc, char **argv) {
    Server server;
    // setup server
    if (!setup_server(argc, argv, &server)) {
        return -1;
    }

#ifdef DEBUG
    printf("server port: %d\n", server.port);
    printf("server root: %s\n", server.root_path);
#endif

    while (1) {
        Client client;
        // waiting for client socket to connect
        unsigned int addrlen;
        client.control_sockfd = accept(server.control_sockfd, (struct sockaddr *)&client.addr, &addrlen);
        if (client.control_sockfd == -1) {
            printf("Error: fail to connect client, error msg: %s(%d)\n", strerror(errno), errno);
            return -1;
        }
        pid_t pid = fork();
        if (pid == 0) {
            // set up client
            memset(client.src_file, 0, strlen(client.src_file));
            memset(client.dst_file, 0, strlen(client.dst_file));
            strcpy(client.cur_path, server.root_path);
            client.offset = 0;
            client.sent_bytes = 0;
            client.sent_files = 0;
            client.state = CONNECT;

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
