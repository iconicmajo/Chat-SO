/*

        Proyecto: chat

Creado por:

Juan Fernando De Leon Quezada   17822
Maria Jose Castro Lemus         181202
Osmin Josue Sagastume           18173

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define LENGTH 2048

// * Status
#define ACTIVE_STATUS "ACTIVE"
#define BUSY_STATUS "BUSY"
#define INACTIVE_STATUS "INACTIVE"

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];
char status[32];

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

// * Exit function
void catch_ctrl_c_and_exit(int sig){
    flag = 1;
}

// * Send Msg
void send_msg_handler(){
    char message[LENGTH] = {};
    char message_copy[LENGTH] = {};
    char buffer[LENGTH + 32] = {};
    while (1)
    {
        str_overwrite_stdout();
        fgets(message, LENGTH, stdin);
        str_trim_lf(message, LENGTH);

        strcpy(message_copy, message);

        char* token = strtok(message, " ");

        if (strcmp(token, "exit") == 0)
        {// Exit chat
            break;
        } else if(strcmp(token, "change-status") == 0){//Change Status
            token = strtok(NULL, " "); // Second "Parameter"
            if(strcmp(token, ACTIVE_STATUS) == 0 || strcmp(token, BUSY_STATUS) == 0 || strcmp(token, INACTIVE_STATUS) == 0){// Chose one of the available statuses
                strcpy(status, token);// Set status
            }
        } else {
            sprintf(buffer, "[%s::Status(%s)] %s\n", name, status, message_copy);
            send(sockfd, buffer, strlen(buffer), 0);
        }

        bzero(message, LENGTH);
        bzero(buffer, LENGTH + 32);
        
    }

    catch_ctrl_c_and_exit(2);
    
}

// * Receive Msg from server
void recv_msg_handler(){
    char message[LENGTH] = {};
    char error_msg[LENGTH] = "Username already exists.\n";
    while(1){
        int receive = recv(sockfd, message, LENGTH, 0);
        if(receive > 0){// We receive something
            printf("%s ", message);
            if(strcmp(error_msg, message) == 0){
                flag = 1;
                break;
            }
            str_overwrite_stdout();
        } else if(receive == 0){ // Error
            break;
        }
        // bzero(message, LENGTH);
        memset(message, 0, sizeof(message));
    }
}

int main(int argc, char **argv){
    if(argc != 2){
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    signal(SIGINT, catch_ctrl_c_and_exit);

    printf("Enter your name: ");
    fgets(name, 32, stdin);

    str_trim_lf(name, strlen(name));

    if(strlen(name) > 32 || strlen(name) < 2){
        printf("Please enter a valid name.\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;

    // Socket Settings
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    // Status
    strcpy(status, ACTIVE_STATUS);

    // Connect client to server
    int err = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if(err == -1){
        printf("ERROR: connect.\n");
        return EXIT_FAILURE;
    }

    // Send client's Name
    // ! TODO: Send clients name and status with protobuf protocol
    send(sockfd, name, 32, 0);
    
    printf("=== Welcome to Chatroom === \n");

    // Thread for sending msg
    pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, (void*)send_msg_handler, NULL) != 0){
        printf("ERROR: pthread.\n");
        return EXIT_FAILURE;
    }

    // Thread for receiving msg
    pthread_t recv_msg_thread;
    if(pthread_create(&recv_msg_thread, NULL, (void*)recv_msg_handler, NULL) != 0){
        printf("ERROR: pthread.\n");
        return EXIT_FAILURE;
    }

    while (1)
    {
        if(flag){
            printf("\nBye\n");
            break;
        }
    }

    close(sockfd);
    
    return EXIT_SUCCESS;

}
