#ifndef SERVER_H
#define SERVER_H

/*-------------------------------MARCO------------------------------------*/
#define DEFAULT_PORT 21
#define DEFAULT_ROOT_PATH "/tmp/"


/*-------------------------------STRUCT-----------------------------------*/
typedef struct Server {
    // basic config
    int port;
    char root_path[100];          
} Server;

typedef struct Client {

} Client;

/*------------------------------VARIABLES---------------------------------*/


/*------------------------------FUNCTIONS---------------------------------*/
int setup_server(int argc, char ** argv);
int setup_root_and_port(int argc, char ** argv);




#endif