#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX 1024

int main() {
    int sock;
    struct sockaddr_in addr;
    char buf[MAX], ip[20];
    int id;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    printf("========================================\n");
    printf("    TCP Fruit Store Client\n");
    printf("========================================\n");
    printf("Enter server IP (press Enter for 10.0.0.1): ");
    fflush(stdout);
    
    fgets(ip, 20, stdin);
    ip[strcspn(ip, "\n")] = 0;
    
    if(strlen(ip) == 0) {
        strcpy(ip, "10.0.0.1");  // Default Mininet h1 IP
    }
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        exit(1);
    }
    
    printf("Connecting to %s:%d...\n", ip, PORT);
    fflush(stdout);
    
    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Connection failed");
        printf("Make sure server is running on %s\n", ip);
        exit(1);
    }
    
    printf("Connected successfully!\n\n");
    
    printf("Enter your Customer ID: ");
    fflush(stdout);
    scanf("%d", &id);
    getchar();
    
    send(sock, &id, sizeof(int), 0);
    printf("\n*** Welcome Customer #%d ***\n", id);
    fflush(stdout);
    
    while(1) {
        memset(buf, 0, MAX);
        int bytes = recv(sock, buf, MAX, 0);
        if(bytes <= 0) {
            printf("\nServer disconnected\n");
            break;
        }
        printf("%s", buf);
        fflush(stdout);
        
        fgets(buf, MAX, stdin);
        buf[strcspn(buf, "\n")] = 0;
        send(sock, buf, strlen(buf), 0);
        
        if(strcmp(buf, "exit") == 0) {
            memset(buf, 0, MAX);
            recv(sock, buf, MAX, 0);
            printf("%s", buf);
            break;
        }
        
        memset(buf, 0, MAX);
        bytes = recv(sock, buf, MAX, 0);
        if(bytes <= 0) break;
        printf("%s", buf);
        fflush(stdout);
        
        fgets(buf, MAX, stdin);
        buf[strcspn(buf, "\n")] = 0;
        send(sock, buf, strlen(buf), 0);
        
        memset(buf, 0, MAX);
        bytes = recv(sock, buf, MAX, 0);
        if(bytes <= 0) break;
        printf("%s", buf);
        fflush(stdout);
        
        printf("\nPress Enter to continue...");
        getchar();
    }
    
    close(sock);
    printf("\nDisconnected from server\n");
    return 0;
}
