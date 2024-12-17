#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAXLINE  511
#define MAX_SOCK 1024
#define MAX_ROOMS 10 

char *EXIT_STRING = "exit";
char *START_STRING = "Connected to chat_server\n";
char *FILE_UPLOAD_STRING = "FILE_UPLOAD"; 

typedef struct {
    int sock;
    int room;
    char nickname[20];
} Client;

Client clients[MAX_SOCK];
int num_user = 0;
int listen_sock;

void addClient(int s, struct sockaddr_in *newcliaddr);
void removeClient(int s);
int getmax();
int tcp_listen(int host, int port, int backlog);
void errquit(char *mesg) { perror(mesg); exit(1); }

int main(int argc, char *argv[]) {
    struct sockaddr_in cliaddr;
    char buf[MAXLINE + 1];
    int i, j, nbyte, accp_sock, addrlen = sizeof(struct sockaddr_in);
    fd_set read_fds;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listen_sock = tcp_listen(INADDR_ANY, atoi(argv[1]), 5);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(listen_sock, &read_fds);
        for (i = 0; i < num_user; i++)
            FD_SET(clients[i].sock, &read_fds);

        int maxfdp1 = getmax() + 1;
        if (select(maxfdp1, &read_fds, NULL, NULL, NULL) < 0)
            errquit("select fail");

        if (FD_ISSET(listen_sock, &read_fds)) {
            accp_sock = accept(listen_sock, (struct sockaddr*)&cliaddr, &addrlen);
            if (accp_sock == -1) errquit("accept fail");
            addClient(accp_sock, &cliaddr);
            send(accp_sock, START_STRING, strlen(START_STRING), 0);
            printf("User added. Current users: %d\n", num_user);
        }

        for (i = 0; i < num_user; i++) {
            if (FD_ISSET(clients[i].sock, &read_fds)) {
                nbyte = recv(clients[i].sock, buf, MAXLINE, 0);
                if (nbyte <= 0) {
                    removeClient(i);
                    continue;
                }
                buf[nbyte] = 0;
                
                if (strstr(buf, FILE_UPLOAD_STRING) != NULL) {

                    char file_content[MAXLINE];

                    sprintf(file_content, "[Room %d]: %s", clients[i].room, buf + strlen(FILE_UPLOAD_STRING));


                    for (j = 0; j < num_user; j++) {
                        if (clients[j].room == clients[i].room) {
                            send(clients[j].sock, file_content, strlen(file_content), 0);
                        }
                    }

                    printf("[Room %d] File received: %s\n", clients[i].room, buf + strlen(FILE_UPLOAD_STRING));
                }
                else if (strstr(buf, EXIT_STRING) != NULL) {
                    removeClient(i);
                    continue;
                }
                else {
                    for (j = 0; j < num_user; j++) {
                        if (clients[j].room == clients[i].room) {
                            send(clients[j].sock, buf, nbyte, 0);
                        }
                    }
                    printf("[Room %d]: %s\n", clients[i].room, buf);
                }
            }
        }
    }
    return 0;
}

int tcp_listen(int host, int port, int backlog) {
    int sd;
    struct sockaddr_in servaddr;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) errquit("socket fail");

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(host);
    servaddr.sin_port = htons(port);

    if (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) errquit("bind fail");
    listen(sd, backlog);
    return sd;
}


int getmax() {
    int max = listen_sock;
    for (int i = 0; i < num_user; i++) {
        if (clients[i].sock > max) {
            max = clients[i].sock;
        }
    }
    return max;
}

void removeClient(int index) {

    char leave_message[MAXLINE];
    sprintf(leave_message, "[Server]: %s has left the chat room %d!\n", clients[index].nickname, clients[index].room);

    for (int i = 0; i < num_user; i++) {
        if (clients[i].room == clients[index].room && i != index) {
            send(clients[i].sock, leave_message, strlen(leave_message), 0);
        }
    }


    close(clients[index].sock);

    if (index != num_user - 1) {
        clients[index] = clients[num_user - 1];
    }
    num_user--;

    printf("\nUser left. Current users: %d\n", num_user);
}


void addClient(int s, struct sockaddr_in *newcliaddr) {
    char buf[50];
    recv(s, buf, sizeof(buf), 0);

    int room;
    char nickname[20];
    sscanf(buf, "%d %s", &room, nickname);

    clients[num_user].sock = s;
    clients[num_user].room = room;
    strcpy(clients[num_user].nickname, nickname);

    num_user++;

    char welcome_message[MAXLINE];
    sprintf(welcome_message, "[Server]: %s has joined the chat room %d!\n", nickname, room);
    
    for (int i = 0; i < num_user; i++) {
        if (clients[i].room == room) {
            send(clients[i].sock, welcome_message, strlen(welcome_message), 0);
        }
    }

    printf("New client in Room %d: %s\n", room, nickname);
}