/*

        Proyecto: chat

Creado por:

Juan Fernando De Leon Quezada   17822
Maria Jose Castro Lemus         181202
Osmin Josue Sagastume           18173

*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

// * Status
#define ACTIVE_STATUS "ACTIVE"
#define BUSY_STATUS "BUSY"
#define INACTIVE_STATUS "INACTIVE"

// * Global Variable for Client Count
static _Atomic unsigned int cli_count = 0;
static int uid = 10;

// * Client Structure
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid; // Unique for every client
    char name[32];
    char status[32];
    time_t lastsms;
} client_t, *client_t_ptr;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout(){
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf(char* arr, int length){
    for(int i=0; i<length; i++){
        if(arr[i]=='\n'){
            arr[i]='\0';
            break;
        }
    }
}

// * Print IP address of client
void print_ip_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d", addr.sin_addr.s_addr & 0xff, (addr.sin_addr.s_addr & 0xff00) >> 8, (addr.sin_addr.s_addr & 0xff0000) >> 16, (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// * Add clients to queue
// Multithreading Applyied
void queue_add(client_t *cl){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(!clients[i]){
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// * Remove client
void queue_remove(int uid){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid == uid){
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// * Send Msg to all clients, except to sender
void send_message(char *s, int uid){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid != uid){
                if(write(clients[i]-> sockfd, s, strlen(s)) < 0){
                    printf("ERROR: write to descriptor failed.");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// * Send Msg to specific client
void send_message_to_user(char *s, char *name){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
            if(strcmp(clients[i]->name, name) == 0){
                if(write(clients[i]-> sockfd, s, strlen(s)) < 0){
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// * Validate if username exists
bool is_in_users(char *name){
    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
                if(strcmp(clients[i]->name, name) == 0){
                        return true;
                }
        }
    }
	return false;
}

// * Display specific user info
void display_user_info(int sockfd, int uid, char *token){
    pthread_mutex_lock(&clients_mutex);
    
    for(int i=0; i<MAX_CLIENTS; i++){
        if(clients[i]){
            if((strcmp(clients[i]->name, token) == 0)){
                char buffer_out[BUFFER_SZ];
                sprintf(buffer_out, "%d.%d.%d.%d", clients[i]->address.sin_addr.s_addr & 0xff, (clients[i]->address.sin_addr.s_addr & 0xff00) >> 8, (clients[i]->address.sin_addr.s_addr & 0xff0000) >> 16, (clients[i]->address.sin_addr.s_addr & 0xff000000) >> 24);
		        strcat(buffer_out, "\n");
                if(write(sockfd, buffer_out, strlen(buffer_out)) < 0){
                    break;
                }
            }
    	}
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

// * Display users list
void display_users_list(int sockfd, int uid){
    pthread_mutex_lock(&clients_mutex);

    char* title = "Connected users list: \n";
    if(write(sockfd, title, strlen(title)) > 0){
	    for(int i=0; i<MAX_CLIENTS; i++){
        	if(clients[i] && (clients[i]->uid != uid)){
		    char name[32];
		    strcpy(name, clients[i]->name);
		    strcat(name, "\n");
        	    if(write(sockfd, name, strlen(name)) < 0){
                	break;
	            }
        	}
	    }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// * Send message to specific client
void kick_user_out(char *s, int uid){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid == uid){
                if(write(clients[i]-> sockfd, s, strlen(s)) < 0){
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// * Validate if username does not exists yet
bool validate_user_name(client_t *cl){
    for(int i=0; i<MAX_CLIENTS; i++){
        if(clients[i]){
                if(strcmp(clients[i]->name, cl->name) == 0){
                    if(clients[i]->uid < cl->uid){// Print in server that users with provided name already exists
                        printf("Username (%s) already exists.\n", cl->name);
                        return true;
                    }
                }
        }
    }
	return false;
}

// * Main function
void *handle_client(void *arg){
    char buffer_out[BUFFER_SZ];
    char buffer_out_copy[BUFFER_SZ];
    char name[32];
    char *help_commands = {"HELP COMMANDS: \nhelp \t\t\t\t\tThis command is for display chat commands.\nshow-users \t\t\t\tThis command is for displaying all connected users.\nshow-user-info <user-name> \t\tThis command is to display provided user's IP address.\n<user-name> <message> \t\t\tThis is for private messages. Only if provided user is connected, otherwise msg will be sent to all users.\nchange-status <ACTIVE, BUSY, INACTIVE> \tThis command is to change user's status.\nexit \t\t\t\t\tThis command is to leave the chat.\n"};


    // Client is connected or not?
    int leave_flag = 0;

    // Increment Client count
    cli_count++;

    client_t *cli = (client_t*)arg;

    //Name from the client
    if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1){
        printf("ERROR: Please enter a valid name.\n");
        leave_flag = 1;
    } else {
        // Display that a client has joined
        strcpy(cli->name, name);
        // * Validar que no exista un usuario con el mismo nombre
        bool user_name_exists = validate_user_name(cli);
        if(!user_name_exists){// If username does not exists
            sprintf(buffer_out, "%s has joined\n", cli->name);
            printf("%s", buffer_out);
            send_message(buffer_out, cli->uid);
        } else {// If username exists
            sprintf(buffer_out, "Username (%s) already exists.\n", cli->name);
            kick_user_out(buffer_out, cli->uid);
            leave_flag = 1;
        }
    }

    bzero(buffer_out, BUFFER_SZ);

    send_message_to_user(help_commands, cli->name);

    while (1)
    {
        if(leave_flag){
            break;
        }

        // ! TODO: Inactividad de usuario por un cierto tiempo
	if ((time(NULL)-(cli->lastsms))>10){
	    sprintf(buffer_out, "Sesion has timeout.\n");
            kick_user_out(buffer_out, cli->uid);
            leave_flag = 1;
	}

        int receive = recv(cli->sockfd, buffer_out, BUFFER_SZ, 0);

        if (receive > 0)
        {
            // Make copy of buffer
            strcpy(buffer_out_copy, buffer_out);

            char* token = strtok(buffer_out_copy, " ");
            char* show_users_list = "show-users";
            char* show_users_info = "show-user-info";
            token = strtok(NULL, " "); // Second "Parameter"

            // * Remove \n to \0
            str_trim_lf(token, strlen(token));

            if (strlen(buffer_out) > 0)
            {
                 if(strcmp(token, show_users_list) == 0){// Desplegar listado de usuarios
                    // * Display users list
                    display_users_list(cli->sockfd, cli->uid);
                } else if(strcmp(token, show_users_info) == 0){
                    // * Display specific user info
                    token = strtok(NULL, " "); // Third "Parameter" should be the username
                    str_trim_lf(token, strlen(token));
                    display_user_info(cli->sockfd, cli->uid, token);
                } else if(strcmp(token, "exit") == 0){
                    // Send Message that a client has left
                    sprintf(buffer_out, "%s has left\n", cli->name);
                    printf("%s\n", buffer_out);
                    send_message(buffer_out, cli->uid);
                    leave_flag = 1;
                } else if(strcmp(token, "help") == 0){
                    // * Send commands
                    send_message_to_user(help_commands, cli->name);
                } else {
                    // * Validate if message is for specific user 
                    if(is_in_users(token)){
                        send_message_to_user(buffer_out, token);
                    } else {
                        send_message(buffer_out, cli->uid);
                        str_trim_lf(buffer_out, strlen(buffer_out));
                        printf("%s -> %s\n", buffer_out, cli->name, cli->status);
                    }
                }
            }
        } else if (receive == 0){
            // Send Message that a client has left
            sprintf(buffer_out, "%s has left\n", cli->name);
            printf("%s\n", buffer_out);
            send_message(buffer_out, cli->uid);
            leave_flag = 1;
        } else {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        // ! TODO: Desplegar ayuda con el comando <help>

        bzero(buffer_out, BUFFER_SZ);

    }

    // * CLient has left

    // Close Socket connection
    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);

    // Decrease Client count
    cli_count--;

    pthread_detach(pthread_self());

    return NULL;

}

int main(int argc, char **argv){
    if(argc != 2){
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    int option = 1;

	int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;

    pthread_t tid;

    // Socket Settings
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    // Signals
    signal(SIGPIPE, SIG_IGN);
    if(setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
        printf("ERROR: setsockopt\n");
        return EXIT_FAILURE;
    }

    // Bind
    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 ){
        printf("ERROR: bind\n");
        return EXIT_FAILURE;
    }

    // Listen
    if (listen(listenfd, 10) < 0) {
        printf("ERROR: listen\n");
        return EXIT_FAILURE;
    }

    printf("\n\t=== Welcome to Chatroom === \n");


    while(1){
        socklen_t clilen = sizeof(cli_addr);

        // Accept the connection with client
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        // Verify max ammount of connected clients
        if((cli_count + 1) == MAX_CLIENTS){
            printf("Maximun ammount of clients connected. Connection rejected.\n");
            print_ip_addr(cli_addr);
            close(connfd);
            continue;
        }

        // Client Settings
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;
        cli->lastsms = time(NULL);
        //printf("%ld\n",cli->lastsms);

        strcpy(cli->status, ACTIVE_STATUS);

        // Add Client to queue
        queue_add(cli);
        // Create Thread
        pthread_create(&tid, NULL, &handle_client, (void*)cli);

        //Reduce CPU usage
        sleep(1);

    }

    return EXIT_SUCCESS;

}
