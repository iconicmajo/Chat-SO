/*

        Proyecto: chat

Creado por:

Juan Fernando De Leon Quezada   17822
Maria Jose Castro Lemus         181202

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

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define NAME_LEN 32

// * Global Variable for Client Count
static _Atomic unsigned int cli_count;
static int uid = 10;

// * Client Structure
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid; // Unique for every client
    char name[NAME_LEN];
} client_t;

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

// * Add clients to queue
// Multithreading Applyied
void queue_add(client_t *cl){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; i++){
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

    for(int i=0; i<MAX_CLIENTS; i++){
        if(!clients[i]){
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

    for(int i=0; i<MAX_CLIENTS; i++){
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

// * Print IP address of client
void print_ip_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d", addr.sin_addr.s_addr & 0xff, (addr.sin_addr.s_addr & 0xff00) >> 8, (addr.sin_addr.s_addr & 0xff0000) >> 16, (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// * Main function
void *handle_client(void *arg){
    char buffer[BUFFER_SZ];
    char name[NAME_LEN];

    // Client is connected or not?
    int leave_flag = 0;
    // Increment Client count
    cli_count++;

    client_t *cli = (client_t*)arg;

    //Name from the client
    if(recv(cli->sockfd, name, NAME_LEN, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LEN - 1){
        printf("ERROR: Pkease enter a valid name.\n");
        leave_flag = 1;
    } else {
        // Display that a client has joined
        strcpy(cli->name, name);
        sprintf(buffer, "%s has joined\n", cli->name);
        printf("%s", buffer);
        send_message(buffer, cli->uid);
    }

    bzero(buffer, BUFFER_SZ);

    while (1)
    {
        if(leave_flag){
            break;
        }

        int receive = recv(cli->sockfd, buffer, BUFFER_SZ, 0);

        if (receive > 0)
        {
            if (strlen(buffer) > 0)
            {
                send_message(buffer, cli->uid);
                str_trim_lf(buffer, strlen(buffer));
                printf("%s -> %s", buffer, cli->name);
            } else if (receive == 0 || strcmp(buffer, "exit") == 0)
            {
                // Send Message that a client has left
                sprintf(buffer, "%s has left\n", cli->name);
                printf("%s", buffer);
                send_message(buffer, cli->uid);
                leave_flag = 1;
            } else {
                printf("ERROR: -l\n");
                leave_flag = 1;
            }

            bzero(buffer, BUFFER_SZ);

        }
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

    printf("=== Welcome to Chatroom === \n");

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

        // Add Client to queue
        queue_add(cli);
        // Create Thread
        pthread_create(&tid, NULL, &handle_client, (void*)cli);

        //Reduce CPU usage
        sleep(1);



    }

    return EXIT_SUCCESS;

}