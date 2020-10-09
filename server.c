#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h> 
#include <netdb.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>
#include <fcntl.h>

#include "client.h"
#include "server.h"
#include "utils.h"

int setup_server(int argc, char ** argv, Server *server) {
	if (!setup_port_and_root(argc, argv, server)) {
		return 0;
	}
	if (!setup_socket(server)) {
		return 0;
	}
	return 1;
}

int setup_port_and_root(int argc, char ** argv, Server *server) {
	// setup port
	server->port = DEFAULT_PORT;
	for (int i = 0; i < argc; i++) {
		if(!strcmp(argv[i], "-port")) {
			if (i + 1 >= argc) {
				printf("Error: parameter for port not found\n");
				return 0;
			}
			int port = atoi(argv[i+1]);
			if (!port) {
				printf("Error: wrong format for port number\n");
				return 0;
			}
			if (port > 65535) {
				printf("Error: port number cannot be larger than 65535");
				return 0;
			}
			server->port = port;
			break;
		}
	}

	// setup root
	strcpy(server->root_path, DEFAULT_ROOT_PATH);
	for (int i = 0; i < argc; i++) {
		if(!strcmp(argv[i], "-root")) {
			if (i + 1 >= argc) {
				printf("Error: parameter for root not found\n");
				return 0;
			}
			strcpy(server->root_path, argv[i+1]);
			DIR* dir = opendir(server->root_path);
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
	// AF_INET for IPv4
	// SOCK_STREAM for TCP
	server->control_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server->control_socket == -1) {
		printf("Error: fail to setup socket, error msg: %s(%d)\n", strerror(errno), errno);
		return 0;
	}

	// setup ip and port
	server->addrlen = sizeof(server->addr);
	memset(&server->addr, 0, server->addrlen);
	server->addr.sin_family = AF_INET;
	server->addr.sin_port = htons(server->port);
	server->addr.sin_addr.s_addr = htonl(INADDR_ANY);	// listen for "127.0.0.1"

	// bind socket with ip
	if (bind(server->control_socket, (struct sockaddr*)&server->addr, server->addrlen) == -1) {
		printf("Error: fail to bind socket, error msg: %s(%d)\n", strerror(errno), errno);
		return 0
	}

	// listen socket
	if (listen(server->control_socket, MAX_CONNECTION) == -1) {
		printf("Error: fail to listen socket, error msg: %s(%d)\n", strerror(errno), errno);
		return 0;
	}
	return 1;
}


void listen_to_client(Client * client) {

	while (1)
	{
		/* code */
	}
}

int main(int argc, char **argv) {
	Server server;
	// setup server
	if(!setup_server(argc, argv, &server)) {
		return -1;
	}
	while (1) {
		Client client;
		// waiting for client socket to connect
		client.control_socket = accept(server.control_socket, &client.addr, &client.addrlen);
		if (client.control_socket == -1) {
			printf("Error: fail to connect client, error msg: %s(%d)\n", strerror(errno), errno);
			return -1;
		}
		pid_t pid = fork();
		if (pid == 0) {
			client.mode = CONNECT;
			// close(server.control_socket); ???
			listen_to_client(&client);
			close(client.control_socket);
		} else if (pid < 0) {
			perror("Error: failed forking processes");
		}
		close(client.control_socket);
	}
	close(server.control_socket);
	return 0;
}

