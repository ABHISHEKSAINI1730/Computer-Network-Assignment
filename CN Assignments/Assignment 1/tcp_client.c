#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sockfd;
    struct sockaddr_in serverAddr;
    char buffer[1024];

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Server details
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = inet_addr("10.0.0.1"); // Change to server host IP

    // Connect
    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    // Send "Hi"
    char msg[] = "Hi";
    send(sockfd, msg, strlen(msg) + 1, 0);

    // Receive "Hello"
    recv(sockfd, buffer, sizeof(buffer), 0);
    printf("Server says: %s\n", buffer);

    close(sockfd);

    return 0;
}
