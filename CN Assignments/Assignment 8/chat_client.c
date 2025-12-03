// Compile: gcc chat_client.c -o chat_client -pthread
// Run: ./chat_client <server_ip> <port>
// Example: ./chat_client 10.0.0.1 9090

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define BUF 1024

int sockfd;

void *recv_thread(void *arg) {
    char buf[BUF];
    while (1) {
        ssize_t r = recv(sockfd, buf, sizeof(buf)-1, 0);
        if (r <= 0) {
            printf("Server disconnected or error.\n");
            exit(0);
        }
        buf[r] = 0;
        printf("%s\n", buf);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]); return 1;
    }
    char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in serv;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect"); return 1;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, NULL);
    pthread_detach(tid);

    char line[BUF];
    while (1) {
        if (!fgets(line, sizeof(line), stdin)) break;
        // send line as-is
        if (send(sockfd, line, strlen(line), 0) <= 0) {
            printf("Send failed\n");
            break;
        }
    }

    close(sockfd);
    return 0;
}
