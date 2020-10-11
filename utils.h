#ifndef UTILS_H
#define UTILS_H

#include "client.h"
#include "server.h"

/*--------------------------FUNCTIONS---------------------------------*/
// setup server
int setup_server(int argc, char **argv, Server *server);
int parse_port_and_root(int argc, char **argv, Server *server);
void setup_local_addr(struct sockaddr_in *addr, int port);
int setup_listen_socket(int *sockfd, struct sockaddr_in *addr);
int setup_addr(struct sockaddr_in *addr, char *ip, int port);
int setup_connect_socket(int *sockfd, struct sockaddr_in *addr);
// communicate with client
int receive_request_from_client(Client *client);
void send_msg_to_client(char *buffer, Client *client);
int send_file(int sockfd, char *file_path, int offset);
int receive_file(int sockfd, char *file_path);
// ip related
int check_addr_and_port_by_hp(int h1, int h2, int h3, int h4, int p1, int p2);
int check_addr_and_port_by_ip(char *ip, int port);
int parse_addr_and_port(char *param, char *addr, int *port);
int decorate_addr_and_port(char *param, char *addr, int port);
int get_host_ip(char *ip);
int gen_random_port();
// file related
void path_concat(char *new_path, char *cur_path, char *next_path);
int get_file_size(char *path);
#endif