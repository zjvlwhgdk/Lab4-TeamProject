#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAXLINE 1000
#define NAME_LEN 20

char *EXIT_STRING = "exit";

// 소켓 생성 및 서버 연결, 생성된 소켓 리턴
int tcp_connect(int af, char *servip, unsigned short port);
void errquit(char *mesg) { perror(mesg); exit(1); }

int main(int argc, char *argv[]) {
    char bufmsg[MAXLINE];     // 메시지
    char bufall[MAXLINE + NAME_LEN];
    int maxfdp1;              // 최대 소켓 디스크립터
    int s;                    // 소켓
    fd_set read_fds;

    if (argc != 5) {
        printf("Usage: %s <server_ip> <port> <room_number> <nickname>\n", argv[0]);
        exit(0);
    }

    int room = atoi(argv[3]);  // 방 번호
    char *nickname = argv[4]; // 닉네임

    s = tcp_connect(AF_INET, argv[1], atoi(argv[2]));
    if (s == -1)
        errquit("tcp_connect fail");

    // 서버에 방 번호와 닉네임 전송
    sprintf(bufmsg, "%d %s", room, nickname);
    if (send(s, bufmsg, strlen(bufmsg), 0) < 0) {
        perror("send fail");
        close(s);
        exit(1);
    }

    puts("Connected to the server.");
    maxfdp1 = s + 1;
    FD_ZERO(&read_fds);

    while (1) {
        FD_SET(0, &read_fds);
        FD_SET(s, &read_fds);
        if (select(maxfdp1, &read_fds, NULL, NULL, NULL) < 0)
            errquit("select fail");

        if (FD_ISSET(s, &read_fds)) { // 서버로부터 메시지 수신
            int nbyte;
            if ((nbyte = recv(s, bufmsg, MAXLINE, 0)) > 0) {
                bufmsg[nbyte] = 0;
                write(1, "\033[0G", 4);  // 커서의 X좌표를 0으로 이동
                printf("%s", bufmsg);    // 메시지 출력
                fprintf(stderr, "\033[1;32m"); // 글자색을 녹색으로 변경
                fprintf(stderr, "%s>", nickname); // 내 닉네임 출력
            }
        }
        if (FD_ISSET(0, &read_fds)) { // 키보드 입력 감지
            if (fgets(bufmsg, MAXLINE, stdin)) {
                fprintf(stderr, "\033[1;33m"); // 글자색을 노란색으로 변경
                fprintf(stderr, "\033[1A");   // Y좌표를 현재 위치로부터 -1만큼 이동
                sprintf(bufall, "%s>%s", nickname, bufmsg);
                if (send(s, bufall, strlen(bufall), 0) < 0)
                    puts("Error: Write error on socket.");
                if (strstr(bufmsg, EXIT_STRING) != NULL) {
                    puts("Goodbye.");
                    close(s);
                    exit(0);
                }
            }
        }
    }
}

int tcp_connect(int af, char *servip, unsigned short port) {
    struct sockaddr_in servaddr;
    int s;

    // 소켓 생성
    if ((s = socket(af, SOCK_STREAM, 0)) < 0)
        return -1;

    // 서버 소켓 주소 설정
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = af;
    inet_pton(AF_INET, servip, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    // 서버에 연결 요청
    if (connect(s, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        return -1;

    return s;
}