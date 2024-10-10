#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#define CLIENT_SEND_SIZE 4096
#define MAX_NICK_LEN 64
#define TIME_STR_LEN 16
#define SERVER_SEND_SIZE CLIENT_SEND_SIZE + MAX_NICK_LEN + 2 // + 2 for the length of ": " which goes between nickname and message

/* CLIENT */
void * send_function(void* conn_fd_ptr);

int main(int argc, char *argv[])
{
    char *dest_hostname, *dest_port;
    char recv_buf[SERVER_SEND_SIZE];
    int conn_fd, rc, num_received;

    pid_t pid;

    time_t current_time;
    struct tm *time_struct;
    char time_str[TIME_STR_LEN];
    pthread_t sending_thread;
    
    struct addrinfo hints, *res;

    if(argc != 3) {
	printf("usage: %s server_hostname server_port\n", argv[0]);
	exit(1);
    }

    dest_hostname = argv[1];
    dest_port = argv[2];

    //create a socket
    if ((conn_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    };

    //find the IP of the server
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((rc = getaddrinfo(dest_hostname, dest_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    //connect to the server
    if (connect(conn_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        exit(2);
    }

    printf("Connected\n");

    pid = getpid();
    
    if ((pthread_create(&sending_thread, NULL, send_function, &conn_fd)) != 0) {
        perror("pthread_create");
        exit(1);
    };

    // infinite receiving loop
    while(1) {
        // read from server file into our buffer
        if ((num_received = recv(conn_fd, recv_buf, SERVER_SEND_SIZE, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        else if (num_received == 0) {
            printf("Connection closed by remote host\n");
            kill(pid, SIGINT);
        }

	    if (time(&current_time) < 0) {
		    perror("time");
            exit(1);
	    }

        if ((time_struct = localtime(&current_time)) == NULL) {
            perror("localtime");
            exit(1);
        }

        strftime(time_str, sizeof(time_str),"%H:%M:%S", time_struct);
        printf("%s: ", time_str);

        // write whatever server has to client stdout
        puts(recv_buf);

        // "erase" the buffer after after it is read to client
        memset(recv_buf, 0, SERVER_SEND_SIZE);
    }

    if ((close(conn_fd)) < 0) {
        perror("close");
        exit(1);
    };
}

void *
send_function(void* conn_fd_ptr)
{
    int conn_fd, num_read;
    char send_buf[CLIENT_SEND_SIZE];

    conn_fd = *(int *)conn_fd_ptr;

    // read from user stdin to the send buffer 
    while((num_read = read(0, send_buf, CLIENT_SEND_SIZE)) > 0) {
        if (send(conn_fd, send_buf, num_read - 1, 0) < 0) {
            perror("send");
            exit(1);
        }
        // 'erase' buffer after we send it
        memset(send_buf, 0, CLIENT_SEND_SIZE);
    }

    pid_t pid = getpid();
    printf("Exiting\n");

    if ((kill(pid, SIGINT)) < 0) {
        perror("kill");
        exit(1);
    };
    return NULL;
}

