
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
#include <pthread.h>
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

void retr_job(Client *client) {
    if (client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 require PORT/PASV mode.\r\n", client);
        return;
    }

    int sockfd = -1;
    if (!build_file_conn_socket(&sockfd, client)) {
        send_msg_to_client("530 fail to set up connect socket.\r\n", client);
        return;
    }

    char file_path[BUF_SIZE];
    get_full_path(file_path, client->root_path, client->cur_path, client->argu);
    int size = get_file_size(file_path);

    char buf[BUF_SIZE];
    if (size < 0) {
        sprintf(buf, "500 file %s not found.\r\n", client->argu);
        send_msg_to_client(buf, client);
        close(sockfd);
        return;
    }

    sprintf(buf, "150 Opening BINARY mode data connection for %s (%d)\r\n", client->argu, size);
    send_msg_to_client(buf, client);

    int total = send_file_by_path(sockfd, file_path, client->offset);
    if (total < 0) {
        sprintf(buf, "500 fail to send %s\r\n", client->argu);
        send_msg_to_client(buf, client);
        close(sockfd);
        return;
    }

    send_msg_to_client("226 Transfer complete.\r\n", client);
    close(sockfd);

    client->sent_bytes += total;
    client->sent_files += 1;
    client->offset = 0;
    client->state = PASS;
}

void retr(Client *client) {
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, (void *)&retr_job, client);
    if (rc != 0) {
        printf("Error: fail to create thread %d\n", rc);
    }
}

void stor_job(Client *client) {
    if (client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 require PORT/PASV mode.\r\n", client);
        return;
    }

    int sockfd = -1;
    if (!build_file_conn_socket(&sockfd, client)) {
        send_msg_to_client("500 fail to set up connect socket.\r\n", client);
        return;
    }

    char file_path[BUF_SIZE];
    get_full_path(file_path, client->root_path, client->cur_path, client->argu);

    char buf[BUF_SIZE];
    sprintf(buf, "150 Opening BINARY mode data connection for %s.\r\n", client->argu);
    send_msg_to_client(buf, client);

    int total = receive_file_by_path(sockfd, file_path, client->offset);
    if (total < 0) {
        sprintf(buf, "500 fail to receive %s\r\n", client->argu);
        send_msg_to_client(buf, client);
        close(sockfd);
        return;
    }
    close(sockfd);
    send_msg_to_client("226 Transfer complete.\r\n", client);

    client->offset = 0;
    client->state = PASS;
}

void stor(Client *client) {
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, (void *)&stor_job, client);
    if (rc != 0) {
        printf("Error: fail to create thread %d\n", rc);
    }
}

void appe_job(Client *client) {
    if (client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 require PORT/PASV mode.\r\n", client);
        return;
    }

    int sockfd = -1;
    if (!build_file_conn_socket(&sockfd, client)) {
        send_msg_to_client("500 fail to set up connect socket.\r\n", client);
        return;
    }

    char file_path[BUF_SIZE];
    get_full_path(file_path, client->root_path, client->cur_path, client->argu);

    char buf[BUF_SIZE];
    sprintf(buf, "150 Opening BINARY mode data connection for %s.\r\n", client->argu);
    send_msg_to_client(buf, client);

    int total = append_file_by_path(sockfd, file_path);
    if (total < 0) {
        sprintf(buf, "500 fail to receive %s\r\n", client->argu);
        send_msg_to_client(buf, client);
        close(sockfd);
        return;
    }
    close(sockfd);
    send_msg_to_client("226 Transfer complete.\r\n", client);

    client->offset = 0;
    client->state = PASS;
}

void appe(Client *client) {
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, (void *)&appe_job, client);
    if (rc != 0) {
        printf("Error: fail to create thread %d\n", rc);
    }
}

void quit(Client *client) {
    char buf[BUF_SIZE];
    sprintf(buf, "221-You have transferred %d bytes in %d files.\r\n", client->sent_bytes, client->sent_files);
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
        send_msg_to_client("500 Only Type I is supported\r\n", client);
    }
}

void port(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    if (parse_addr(client->argu, &client->file_addr)) {
        client->state = PORT;

#ifdef DEBUG
        print_ip_and_port(client->addr);
        char ip[20];
        int port;
        parse_ip_and_port_from_addr(client->file_addr, ip, &port);
        printf("enter port mode, listen address: %s:%d\n", ip, port);
#endif

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
    char host_ip[20];
    if (!get_host_ip(host_ip)) {
        send_msg_to_client("500 fail to get host ip.\r\n", client);
        return;
    }

    struct sockaddr_in addr;
    setup_local_addr(&addr, 0);  // port 0 to connect a usable port
    if (!setup_listen_socket(&client->file_sockfd, addr)) {
        close(client->file_sockfd);
        printf("fail to setup listen socket.\n");
        return;
    }

    // get socket port
    struct sockaddr_in sin;
    socklen_t len;
    if (getsockname(client->file_sockfd, (struct sockaddr *)&sin, &len) == -1) {
        printf("fail to check socket bind port.\n");
        return;
    }
    int file_port = ntohs(sin.sin_port);
    setup_addr(&client->file_addr, host_ip, file_port);

#ifdef DEBUG
    print_ip_and_port(client->addr);
    printf("enter pasv mode, listen address: %s:%u\n", host_ip, file_port);
#endif

    client->state = PASV;
    char param[100];
    decorate_addr(param, client->file_addr);
    char buf[BUF_SIZE];
    sprintf(buf, "227 (%s)\r\n", param);
    send_msg_to_client(buf, client);
}

void rest(Client *client) {
    client->offset = atoi(client->argu);
    char buf[BUF_SIZE];
    sprintf(buf, "350 restart offset is set to %d\r\n", client->offset);
    send_msg_to_client(buf, client);
}

void mkd(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    char new_dir[BUF_SIZE];
    get_full_path(new_dir, client->root_path, client->cur_path, client->argu);
    char buf[BUF_SIZE];
    if (mkdir(new_dir, 0700) == 0) {
        sprintf(buf, "257 \"%s\" created.\r\n", new_dir);
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
    char new_path[BUF_SIZE];
    get_full_path(new_path, client->root_path, client->cur_path, client->argu);

    DIR *dir = opendir(new_path);
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

#ifdef DEBUG
        print_ip_and_port(client->addr);
        printf("cur_path: %s\n", client->cur_path);
#endif

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
    sprintf(buf, "257 \"%s\"\r\n", client->cur_path);
    send_msg_to_client(buf, client);
}

void list(Client *client) {
    if (client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 require PORT/PASV mode.\r\n", client);
        return;
    }

    int sockfd = -1;
    if (!build_file_conn_socket(&sockfd, client)) {
        send_msg_to_client("500 fail to set up connect socket.\r\n", client);
        return;
    }

    char command[BUF_SIZE];
    char cur_path[BUF_SIZE];
    get_full_path(cur_path, client->root_path, client->cur_path, "");
    sprintf(command, "cd %s; ls -l | tail +1", cur_path);
    FILE *fp = popen(command, "r");

    send_msg_to_client("150 Opening BINARY mode data connection.\r\n", client);

    int total = send_file(sockfd, fp, 0);
    if (total < 0) {
        send_msg_to_client("500 fail to send file lists.\r\n", client);
        close(sockfd);
        return;
    }
    send_msg_to_client("226 Transfer complete.\r\n", client);
    close(sockfd);

    client->state = PASS;
}

void size(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    char path[BUF_SIZE];
    get_full_path(path, client->root_path, client->cur_path, client->argu);
    FILE *fp = fopen(path, "rb");
    char buf[BUF_SIZE];
    if (!fp) {
        sprintf(buf, "500 cannot find file %s", client->argu);
        send_msg_to_client(buf, client);
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fclose(fp);

    sprintf(buf, "231 %ld", file_size);
    send_msg_to_client(buf, client);
}

void dele(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    char path[BUF_SIZE];
    get_full_path(path, client->root_path, client->cur_path, client->argu);

    char buf[BUF_SIZE];
    if (remove(path) == 0) {
        sprintf(buf, "250 %s removed.\r\n", path);
    } else {
        sprintf(buf, "500 remove %s failed, error msg: %s(%d)\r\n", path, strerror(errno), errno);
    }
    send_msg_to_client(buf, client);
}

void rmd(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    char path[BUF_SIZE];
    get_full_path(path, client->root_path, client->cur_path, client->argu);

    char buf[BUF_SIZE];
    if (rmdir(path) == 0) {
        sprintf(buf, "250 %s removed.\r\n", path);
    } else {
        sprintf(buf, "500 remove %s failed, error msg: %s(%d)\r\n", path, strerror(errno), errno);
    }
    send_msg_to_client(buf, client);
}

void rnfr(Client *client) {
    if (client->state != PASS && client->state != PORT && client->state != PASV) {
        send_msg_to_client("530 you haven't login in.\r\n", client);
        return;
    }
    get_full_path(client->src_file, client->root_path, client->cur_path, client->argu);
    // path_concat(client->src_file, client->cur_path, client->argu);
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
    get_full_path(client->dst_file, client->root_path, client->cur_path, client->argu);

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
        if (!receive_request_from_client(client)) {
            continue;
        }

#ifdef DEBUG
        print_ip_and_port(client->addr);
        printf("from client: %s %s", client->command, client->argu);
        printf("\tcurrent state: ");
        switch (client->state) {
            case DISCONNECT:
                printf("DISCONNECT");
                break;
            case CONNECT:
                printf("CONNECT");
                break;
            case USER:
                printf("USER");
                break;
            case PASS:
                printf("PASS");
                break;
            case PORT:
                printf("PORT");
                break;
            case PASV:
                printf("PASV");
                break;
            default:
                break;
        }
        printf("\n");
#endif

        if (!strcmp(client->command, "USER")) {
            user(client);
        } else if (!strcmp(client->command, "PASS")) {
            pass(client);
        } else if (!strcmp(client->command, "RETR")) {
            retr(client);
        } else if (!strcmp(client->command, "STOR")) {
            stor(client);
        } else if (!strcmp(client->command, "APPE")) {
            appe(client);
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
        } else if (!strcmp(client->command, "REST")) {
            rest(client);
        } else if (!strcmp(client->command, "MKD")) {
            mkd(client);
        } else if (!strcmp(client->command, "CWD")) {
            cwd(client);
        } else if (!strcmp(client->command, "PWD")) {
            pwd(client);
        } else if (!strcmp(client->command, "LIST")) {
            list(client);
        } else if (!strcmp(client->command, "SIZE")) {
            size(client);
        } else if (!strcmp(client->command, "RMD")) {
            rmd(client);
        } else if (!strcmp(client->command, "DELE")) {
            dele(client);
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
    printf("server sockfd: %d\n", server.control_sockfd);
#endif

    while (1) {
        Client client;
        // waiting for client socket to connect
        // to get the addr of the connected client.
        // but this can cause error when bind to 21 port. (unsolved bug.)
        /* memset(&client.addr, 0, sizeof(client.addr)); */
        /* socklen_t addrlen; */
        /* client.control_sockfd = accept(server.control_sockfd, (struct sockaddr *)&client.addr, &addrlen); */
        client.control_sockfd = accept(server.control_sockfd, NULL, NULL);
        if (client.control_sockfd == -1) {
            printf("Error: fail to connect client, error msg: %s(%d)\n", strerror(errno), errno);
            return -1;
        }
        pid_t pid = fork();
        if (pid == 0) {
            // set up client
            memset(client.src_file, 0, strlen(client.src_file));
            memset(client.dst_file, 0, strlen(client.dst_file));
            strcpy(client.root_path, server.root_path);
            strcpy(client.cur_path, "/");
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
