#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>

#define PORT 8080
#define MAX 1024

typedef struct {
    char name[50];
    int qty;
    char last_sold[30];
} Fruit;

Fruit fruits[10];
int fruit_cnt = 0;
int clients[100], client_cnt = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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
    pthread_mutex_lock(&lock);
    for(int i = 0; i < client_cnt; i++)
        if(clients[i] == id) { pthread_mutex_unlock(&lock); return; }
    clients[client_cnt++] = id;
    pthread_mutex_unlock(&lock);
}

void* handle_client(void* arg) {
    int sock = *(int*)arg;
    free(arg);
    char buf[MAX];
    int id;
    
    int bytes = recv(sock, &id, sizeof(int), 0);
    if(bytes <= 0) {
        close(sock);
        return NULL;
    }
    
    printf("Client %d connected\n", id);
    fflush(stdout);
    add_client(id);
    
    while(1) {
        memset(buf, 0, MAX);
        
        // Send stock
        sprintf(buf, "\n=== Current Stock ===\n");
        for(int i = 0; i < fruit_cnt; i++) {
            char tmp[200];
            sprintf(tmp, "%d. %s: Qty=%d, Last=%s\n", i+1, fruits[i].name, fruits[i].qty, fruits[i].last_sold);
            strcat(buf, tmp);
        }
        
        // Send client list
        pthread_mutex_lock(&lock);
        strcat(buf, "\n=== Customer List ===\n");
        for(int i = 0; i < client_cnt; i++) {
            char tmp[50];
            sprintf(tmp, "Customer ID: %d\n", clients[i]);
            strcat(buf, tmp);
        }
        sprintf(buf + strlen(buf), "Total Customers: %d\n", client_cnt);
        pthread_mutex_unlock(&lock);
        
        strcat(buf, "\nEnter fruit name (or 'exit'): ");
        send(sock, buf, strlen(buf), 0);
        
        memset(buf, 0, MAX);
        bytes = recv(sock, buf, MAX, 0);
        if(bytes <= 0) break;
        buf[strcspn(buf, "\n")] = 0;
        
        if(strcmp(buf, "exit") == 0) {
            strcpy(buf, "Thank you! Goodbye.\n");
            send(sock, buf, strlen(buf), 0);
            break;
        }
        
        char fname[50];
        strcpy(fname, buf);
        
        strcpy(buf, "Enter quantity: ");
        send(sock, buf, strlen(buf), 0);
        
        memset(buf, 0, MAX);
        bytes = recv(sock, buf, MAX, 0);
        if(bytes <= 0) break;
        int qty = atoi(buf);
        
        pthread_mutex_lock(&lock);
        int found = 0;
        for(int i = 0; i < fruit_cnt; i++) {
            if(strcasecmp(fruits[i].name, fname) == 0) {
                found = 1;
                if(fruits[i].qty >= qty) {
                    fruits[i].qty -= qty;
                    time_t now = time(NULL);
                    strftime(fruits[i].last_sold, 30, "%H:%M:%S", localtime(&now));
                    sprintf(buf, "\n✓ SUCCESS: Purchased %d %s(s)\n", qty, fname);
                    printf("Client %d purchased %d %s\n", id, qty, fname);
                } else {
                    sprintf(buf, "\n✗ REGRET: Only %d %s available (requested %d)\n", fruits[i].qty, fname, qty);
                    printf("Client %d: Insufficient stock for %s\n", id, fname);
                }
                fflush(stdout);
                break;
            }
        }
        if(!found) sprintf(buf, "\n✗ ERROR: Fruit '%s' not found\n", fname);
        pthread_mutex_unlock(&lock);
        
        send(sock, buf, strlen(buf), 0);
    }
    
    printf("Client %d disconnected\n", id);
    fflush(stdout);
    close(sock);
    return NULL;
}

int main() {
    int srv, *cli;
    struct sockaddr_in addr;
    
    init_fruits();
    
    srv = socket(AF_INET, SOCK_STREAM, 0);
    if(srv < 0) {
        perror("Socket failed");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(srv, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if(bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    
    if(listen(srv, 10) < 0) {
        perror("Listen failed");
        exit(1);
    }
    
    printf("=================================\n");
    printf("Fruit Store Server Started\n");
    printf("Port: %d\n", PORT);
    printf("Waiting for clients...\n");
    printf("=================================\n");
    fflush(stdout);
    
    while(1) {
        cli = malloc(sizeof(int));
        *cli = accept(srv, NULL, NULL);
        if(*cli < 0) {
            perror("Accept failed");
            free(cli);
            continue;
        }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, cli);
        pthread_detach(tid);
    }
    return 0;
}
