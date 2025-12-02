#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 8080
#define MAX 1024

typedef struct {
    char name[50];
    int qty;
    char last_sold[30];
} Fruit;

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

Fruit fruits[10];
int fruit_cnt = 0;
int clients[100], client_cnt = 0;

void init_fruits() {
    strcpy(fruits[0].name, "Apple"); fruits[0].qty = 100;
    strcpy(fruits[1].name, "Banana"); fruits[1].qty = 150;
    strcpy(fruits[2].name, "Orange"); fruits[2].qty = 80;
    strcpy(fruits[3].name, "Mango"); fruits[3].qty = 60;
    strcpy(fruits[4].name, "Grapes"); fruits[4].qty = 120;
    fruit_cnt = 5;
    for(int i = 0; i < 5; i++) strcpy(fruits[i].last_sold, "Never");
}

void add_client(int id) {
    for(int i = 0; i < client_cnt; i++)
        if(clients[i] == id) return;
    clients[client_cnt++] = id;
}

void get_stock_info(char *buf) {
    sprintf(buf, "\n=== Current Stock ===\n");
    for(int i = 0; i < fruit_cnt; i++) {
        char tmp[200];
        sprintf(tmp, "%d. %s: Qty=%d, Last=%s\n", i+1, fruits[i].name, fruits[i].qty, fruits[i].last_sold);
        strcat(buf, tmp);
    }
    
    strcat(buf, "\n=== Customer List ===\n");
    for(int i = 0; i < client_cnt; i++) {
        char tmp[50];
        sprintf(tmp, "Customer ID: %d\n", clients[i]);
        strcat(buf, tmp);
    }
    sprintf(buf + strlen(buf), "Total Customers: %d\n", client_cnt);
}

int main() {
    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    Request req;
    Response res;
    
    init_fruits();
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        perror("Socket failed");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if(bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    
    printf("=================================\n");
    printf("UDP Fruit Store Server Started\n");
    printf("Port: %d\n", PORT);
    printf("Waiting for requests...\n");
    printf("=================================\n");
    fflush(stdout);
    
    while(1) {
        memset(&req, 0, sizeof(req));
        memset(&res, 0, sizeof(res));
        
        int bytes = recvfrom(sock, &req, sizeof(req), 0, 
                            (struct sockaddr*)&client_addr, &addr_len);
        
        if(bytes <= 0) continue;
        
        add_client(req.id);
        
        if(req.action == 1) {
            // View stock
            get_stock_info(res.message);
            res.success = 1;
            printf("Client %d requested stock info\n", req.id);
            
        } else if(req.action == 2) {
            // Purchase
            int found = 0;
            for(int i = 0; i < fruit_cnt; i++) {
                if(strcasecmp(fruits[i].name, req.fruit_name) == 0) {
                    found = 1;
                    if(fruits[i].qty >= req.qty) {
                        fruits[i].qty -= req.qty;
                        time_t now = time(NULL);
                        strftime(fruits[i].last_sold, 30, "%H:%M:%S", localtime(&now));
                        sprintf(res.message, "\n✓ SUCCESS: Purchased %d %s(s)\n", 
                                req.qty, req.fruit_name);
                        res.success = 1;
                        printf("Client %d purchased %d %s\n", req.id, req.qty, req.fruit_name);
                    } else {
                        sprintf(res.message, "\n✗ REGRET: Only %d %s available (requested %d)\n", 
                                fruits[i].qty, req.fruit_name, req.qty);
                        res.success = 0;
                        printf("Client %d: Insufficient stock for %s\n", req.id, req.fruit_name);
                    }
                    break;
                }
            }
            if(!found) {
                sprintf(res.message, "\n✗ ERROR: Fruit '%s' not found\n", req.fruit_name);
                res.success = 0;
            }
            
        } else if(req.action == 3) {
            // Exit
            sprintf(res.message, "Thank you! Goodbye.\n");
            res.success = 1;
            printf("Client %d disconnected\n", req.id);
        }
        
        fflush(stdout);
        
        sendto(sock, &res, sizeof(res), 0, 
               (struct sockaddr*)&client_addr, addr_len);
    }
    
    close(sock);
    return 0;
}
