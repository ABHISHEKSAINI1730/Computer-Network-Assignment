#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX 1024

typedef struct {
    int id;
    char fruit_name[50];
    int qty;
    int action; // 1=view_stock, 2=purchase, 3=exit
} Request;

typedef struct {
    char message[MAX];
    int success;
} Response;

int main() {
    int sock;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char ip[20];
    int id;
    Request req;
    Response res;
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    printf("========================================\n");
    printf("    UDP Fruit Store Client\n");
    printf("========================================\n");
    printf("Enter server IP (press Enter for 10.0.0.1): ");
    fflush(stdout);
    
    fgets(ip, 20, stdin);
    ip[strcspn(ip, "\n")] = 0;
    
    if(strlen(ip) == 0) {
        strcpy(ip, "10.0.0.1");
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        exit(1);
    }
    
    printf("Server set to %s:%d\n", ip, PORT);
    
    printf("Enter your Customer ID: ");
    fflush(stdout);
    scanf("%d", &id);
    getchar();
    
    printf("\n*** Welcome Customer #%d ***\n", id);
    printf("Using UDP (connectionless protocol)\n");
    fflush(stdout);
    
    while(1) {
        // Request stock info
        memset(&req, 0, sizeof(req));
        memset(&res, 0, sizeof(res));
        
        req.id = id;
        req.action = 1; // View stock
        
        sendto(sock, &req, sizeof(req), 0, 
               (struct sockaddr*)&server_addr, addr_len);
        
        recvfrom(sock, &res, sizeof(res), 0, NULL, NULL);
        
        printf("%s", res.message);
        printf("\nEnter fruit name (or 'exit'): ");
        fflush(stdout);
        
        char fruit[50];
        fgets(fruit, 50, stdin);
        fruit[strcspn(fruit, "\n")] = 0;
        
        if(strcmp(fruit, "exit") == 0) {
            memset(&req, 0, sizeof(req));
            req.id = id;
            req.action = 3; // Exit
            
            sendto(sock, &req, sizeof(req), 0, 
                   (struct sockaddr*)&server_addr, addr_len);
            
            recvfrom(sock, &res, sizeof(res), 0, NULL, NULL);
            printf("%s", res.message);
            break;
        }
        
        printf("Enter quantity: ");
        fflush(stdout);
        int qty;
        scanf("%d", &qty);
        getchar();
        
        // Purchase request
        memset(&req, 0, sizeof(req));
        memset(&res, 0, sizeof(res));
        
        req.id = id;
        req.action = 2; // Purchase
        strcpy(req.fruit_name, fruit);
        req.qty = qty;
        
        sendto(sock, &req, sizeof(req), 0, 
               (struct sockaddr*)&server_addr, addr_len);
        
        recvfrom(sock, &res, sizeof(res), 0, NULL, NULL);
        
        printf("%s", res.message);
        fflush(stdout);
        
        printf("\nPress Enter to continue...");
        getchar();
    }
    
    close(sock);
    printf("\nDisconnected from server\n");
    return 0;
}
