
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>

#define CLIENT_SEND_SIZE 4096
#define MAX_NICK_LEN 16
#define SERVER_SEND_SIZE CLIENT_SEND_SIZE + MAX_NICK_LEN + 2 // +2 for ': '
                                                      
#define BACKLOG 0

struct client_info {
    char *remote_ip;
    char nick[MAX_NICK_LEN];
    struct client_info *next;
    struct client_info *prev;
    int conn_fd;
    uint16_t remote_port;
};

static struct client_info *first_client = NULL;
pthread_mutex_t mutex;

void *client_function(void *data);
void delete_client(struct client_info *client_to_delete);
void send_message(char *send_buf);

pthread_mutex_t mutex; 

/* SERVER */
int
main(int argc, char *argv[])
{
    char *listen_port;
    int listen_fd, conn_fd, rc;
    struct addrinfo hints, *res;
    struct sockaddr_in remote_sa;
    socklen_t addrlen;

    struct client_info *client_data;

    if(argc != 2) {
	printf("usage: %s server_port\n", argv[0]);
	exit(1);
    }

    listen_port = argv[1];

    // create a socket
    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    // bind it to a port
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rc = getaddrinfo(NULL, listen_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    // bind to our socket
    if (bind(listen_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        exit(1);
    }
    
    // start listening 
    if ((listen(listen_fd, BACKLOG)) < 0) {
        perror("listen");
        exit(1);
    };

    printf("Chat server listening on port %s\n", listen_port);
    
    // infinite loop of accepting new connections and handling them
    while(1) {
        // accept a new connection (will block until one appears)
        addrlen = sizeof(remote_sa);
        if ((conn_fd = accept(listen_fd, (struct sockaddr *) &remote_sa, &addrlen)) < 0) {
            perror("accept");
            exit(1);
        }

        if ((client_data = malloc(sizeof(struct client_info))) == NULL) {
            perror("malloc");
            exit(1);
        }

        client_data->conn_fd = conn_fd;

        client_data->remote_ip = inet_ntoa(remote_sa.sin_addr);
        client_data->remote_port = ntohs(remote_sa.sin_port);
        printf("new connection from %s:%d\n", client_data->remote_ip, client_data->remote_port);
        
        //create a thread
        pthread_t client_thread;

        if ((pthread_create(&client_thread, NULL, client_function, ((void *) client_data))) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }
}

void
add_client(struct client_info *c) 
{
    pthread_mutex_lock(&mutex);

    struct client_info *iterator = first_client;

    if (first_client == NULL){
        first_client = c;
        first_client->next = NULL;
        first_client->prev = NULL;
    }
    else {
        while(iterator->next != NULL){
            iterator = iterator->next;
        }
        // reached last client in loop
        iterator->next = c;
        c->prev = iterator;
        c->next = NULL;
    }

    strcpy(c->nick, "unknown");

    pthread_mutex_unlock(&mutex);
}

void *
client_function(void *data)
{ 
    int bytes_received;
    char recv_buf[CLIENT_SEND_SIZE];
    char check_buf[CLIENT_SEND_SIZE];
    char send_buf[SERVER_SEND_SIZE];

    // the +1 accommodates a null byte at the end
    char old_name[MAX_NICK_LEN + 1];
    
    struct client_info *c = data;
    add_client(c);

    while((bytes_received = recv(c->conn_fd, recv_buf, CLIENT_SEND_SIZE, 0)) > 0) {
        // copy message user sent so we can parse it
        strcpy(check_buf, recv_buf);

        // parse user message to look for nickname change
        char *token = strtok(check_buf, " ");

        if (strcmp(token, "/nick") == 0) {
            token = strtok(NULL, " ");
            if (token != NULL) {
                // remove trailing new line from user input
                token[strcspn(token, "\n")] = 0;

                // reset the old_name buffer
                memset(old_name, 0, MAX_NICK_LEN); 

                // save old nickname
                memcpy(old_name, c->nick, strlen(c->nick));

                // save new nickname, MAX_NICK_LEN + 1 to ensure terminating character
                // doesn't cut off last char of nickname
                snprintf(c->nick, MAX_NICK_LEN + 1, "%s", token);

                memset(send_buf, 0, SERVER_SEND_SIZE);

                printf("User %s (%s:%u) is now known as %s.\n", old_name, c->remote_ip, c->remote_port, c->nick);

                // update broadcast message to reflect changed username
                snprintf(send_buf, SERVER_SEND_SIZE, "User %s (%s:%u) is now known as %s.", old_name, c->remote_ip, c->remote_port, c->nick);
            }
            
            //the user typed "/nick" followed by only spaces, so we send that string as a message
            else {
                snprintf(send_buf, SERVER_SEND_SIZE, "%s: %s", c->nick, recv_buf);
            }
        }

        // finish implementing this
        // if client types "/exit", they will disconnect but receive incorrect message
        else if (strcmp(token, "/exit") == 0) {
            break;
        }
        
        // if client is just communicating as normal
        else {
            // send nickname: *their message*
            snprintf(send_buf, SERVER_SEND_SIZE, "%s: %s", c->nick, recv_buf);
        }

        // send message to clients
        send_message(send_buf);

        // reset our buffers
        memset(send_buf, 0, SERVER_SEND_SIZE);
        memset(recv_buf, 0, CLIENT_SEND_SIZE);
    }

    // once client disconnects
    printf("Lost connection from %s\n", c->nick);
    memset(send_buf, 0, SERVER_SEND_SIZE);

    // construct disconnect message
    sprintf(send_buf, "User %s (%s:%u) has disconnected.", c->nick, c->remote_ip, c->remote_port); 
    
    // delete client from linked list
    delete_client(c); 

    // send message to clients
    send_message(send_buf);
    
    return NULL;
}

void
send_message(char *message) {
    pthread_mutex_lock(&mutex);

    struct client_info *iterator = first_client;

    iterator = first_client;
    while(iterator != NULL) {
        // don't need to check return status because it is possible client could have disconnected
        send(iterator->conn_fd, message, strlen(message), 0);
        iterator = iterator->next;
    }
    pthread_mutex_unlock(&mutex);
}

void
delete_client(struct client_info *c) {
    pthread_mutex_lock(&mutex);

    if (first_client == c){
        if (first_client->next == NULL){
            first_client = NULL;
        }
        else {
            first_client = first_client->next;
            first_client->prev = NULL;
        }
    }
    // we're not deleting the first client
    else{
        c->prev->next = c->next;
        if (c->next != NULL){
            c->next->prev = c->prev;
        }
    }
    
    if ((close(c->conn_fd)) < 0) {
        perror("close");
    } 
    free(c);

    pthread_mutex_unlock(&mutex);
}
