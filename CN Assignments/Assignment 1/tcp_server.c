#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sockfd, newsock;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_size;
    char buffer[1024];

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Server info
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind
    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(1);
    }

    // Listen
    if (listen(sockfd, 1) < 0) {
        perror("Listen failed");
        close(sockfd);
        exit(1);
    }

    printf("Server is waiting...\n");

    addr_size = sizeof(clientAddr);
    newsock = accept(sockfd, (struct sockaddr*)&clientAddr, &addr_size);
    if (newsock < 0) {
        perror("Accept failed");
        exit(1);
    }

    // Read message from client
    recv(newsock, buffer, sizeof(buffer), 0);
    printf("Client says: %s\n", buffer);

    // Reply
    char reply[] = "Hello";
    send(newsock, reply, strlen(reply) + 1, 0);

    close(newsock);
    close(sockfd);

    return 0;
}
