#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#include "client.h"
#include "server.h"

/*--------------------------FUNCTIONS---------------------------------*/
// setup server
int setup_server(int argc, char **argv, Server *server);
int parse_port_and_root(int argc, char **argv, Server *server);
void setup_local_addr(struct sockaddr_in *addr, int port);
int setup_listen_socket(int *sockfd, struct sockaddr_in addr);
int setup_addr(struct sockaddr_in *addr, char *ip, int port);
int setup_connect_socket(int *sockfd, struct sockaddr_in addr);
// communicate with client
int receive_request_from_client(Client *client);
void send_msg_to_client(char *buffer, Client *client);
int send_file(int sockfd, FILE *fp, int offset);
int send_file_by_path(int sockfd, char *file_path, int offset);
int receive_file(int sockfd, FILE *fp, int offset);
int receive_file_by_path(int sockfd, char *file_path, int offset);
// ip related
int check_ip_and_port_by_hp(int h1, int h2, int h3, int h4, int p1, int p2);
int check_ip_and_port_by_ip(char *ip, int port);
int parse_ip_and_port(char *param, char *ip, int *port);
int parse_addr(char *param, struct sockaddr_in *addr);
int decorate_ip_and_port(char *param, char *ip, int port);
int decorate_addr(char *param, struct sockaddr_in addr);
int get_host_ip(char *ip);
int gen_random_port();
void parse_ip_and_port_from_addr(struct sockaddr_in addr, char *ip, int *port);
void print_ip_and_port(struct sockaddr_in addr);
// file related
void get_full_path(char *full_path, char *root_path, char *cur_path, char *argu_path);
void path_concat(char *new_path, char *cur_path, char *next_path);
int get_file_size(char *path);
#endif