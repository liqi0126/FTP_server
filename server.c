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

#include "server.h"
#include "utils.h"

int setup_server(int argc, char ** argv, Server server) {
	if(!setup_port_and_root(argc, argv, server)) {
		return 0;
	}

	return 1;
}

int setup_port_and_root(int argc, char ** argv, Server server) {
	// setup port
	server.port = DEFAULT_PORT;
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
			server.port = port;
			break;
		}
	}

	// setup root
	strcpy(server.root_path, DEFAULT_ROOT_PATH);
	for (int i = 0; i < argc; i++) {
		if(!strcmp(argv[i], "-root")) {
			if (i + 1 >= argc) {
				printf("Error: parameter for root not found\n");
				return 0;
			}
			strcpy(server.root_path, argv[i+1]);
			DIR* dir = opendir(server.root_path);
			if (dir) {
				closedir(dir);
			} else {
				printf("Error: open directory %s failed\n", server.root_path);
				return 0;
			}
			break;
		}
	}
}


int main(int argc, char **argv) {
	Server server;
	setup_server(argc, argv, server);
}

