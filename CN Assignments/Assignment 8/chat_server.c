// Compile: gcc chat_server.c -o chat_server -pthread
// Run: sudo ./chat_server <port>
// Example: sudo ./chat_server 9090

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define MAX_CLIENTS 100
#define BUF 1024
#define LOGFILE "log.txt"

typedef struct {
    int sock;
    struct sockaddr_in addr;
} client_t;

client_t *clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t logfile_lock = PTHREAD_MUTEX_INITIALIZER;

/* Add client to list */
void add_client(client_t *c) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == NULL) {
            clients[i] = c;
            client_count++;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

/* Remove client from list */
void remove_client(client_t *c) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == c) {
            clients[i] = NULL;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

/* Broadcast message to all clients */
void broadcast_message(const char *msg, size_t len) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            ssize_t s = send(clients[i]->sock, msg, len, 0);
            if (s <= 0) {
                // ignore send errors here; client handler will catch disconnect
            }
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

/* Write log to file with timestamp */
void write_log(const char *entry) {
    pthread_mutex_lock(&logfile_lock);
    FILE *f = fopen(LOGFILE, "a");
    if (f) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char timestr[64];
        strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm);
        fprintf(f, "[%s] %s\n", timestr, entry);
        fclose(f);
    } else {
        perror("fopen log");
    }
    pthread_mutex_unlock(&logfile_lock);
}

/* Utility: get client id string IP:port */
void client_id_str(client_t *c, char *buf, size_t n) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(c->addr.sin_addr), ip, sizeof(ip));
    int port = ntohs(c->addr.sin_port);
    snprintf(buf, n, "%s:%d", ip, port);
}

/* Thread function handling a client */
void *client_handler(void *arg) {
    client_t *c = (client_t *)arg;
    char clientid[64];
    client_id_str(c, clientid, sizeof(clientid));

    // Send welcome message
    char welcome[BUF];
    snprintf(welcome, sizeof(welcome), "Welcome to GroupChat! You are %s\n", clientid);
    send(c->sock, welcome, strlen(welcome), 0);

    // Announce join
    char joinmsg[BUF];
    snprintf(joinmsg, sizeof(joinmsg), "%s joined the chat.", clientid);
    write_log(joinmsg);
    broadcast_message(joinmsg, strlen(joinmsg));

    char buffer[BUF];
    while (1) {
        ssize_t r = recv(c->sock, buffer, sizeof(buffer)-1, 0);
        if (r <= 0) {
            // disconnect or error
            if (r == 0) {
                // graceful close
                printf("Client %s disconnected.\n", clientid);
            } else {
                printf("recv error from %s: %s\n", clientid, strerror(errno));
            }
            break;
        }
        buffer[r] = 0;
        // trim newlines
        char *p = buffer;
        while (*p && (*p == '\r' || *p == '\n')) p++;
        // form message: <clientid> : message
        char msg[BUF*2];
        snprintf(msg, sizeof(msg), "%s : %s", clientid, buffer);
        // Log and broadcast
        write_log(msg);
        broadcast_message(msg, strlen(msg));
    }

    // announce leave
    char leave[BUF];
    snprintf(leave, sizeof(leave), "%s left the chat.", clientid);
    write_log(leave);
    broadcast_message(leave, strlen(leave));

    // cleanup
    close(c->sock);
    remove_client(c);
    free(c);
    pthread_exit(NULL);
}

/* Main server */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]); return 1;
    }
    int port = atoi(argv[1]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv, cli;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("bind"); return 1;
    }

    if (listen(server_fd, 10) < 0) { perror("listen"); return 1; }

    printf("Chat server listening on port %d\n", port);

    // Remove old log file or keep it? We'll append. Uncomment to reset:
    // remove(LOGFILE);

    while (1) {
        socklen_t c = sizeof(cli);
        int newfd = accept(server_fd, (struct sockaddr *)&cli, &c);
        if (newfd < 0) {
            perror("accept"); continue;
        }
        client_t *cl = malloc(sizeof(client_t));
        cl->sock = newfd;
        cl->addr = cli;
        add_client(cl);

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, (void*)cl) != 0) {
            perror("pthread_create");
            // cleanup
            remove_client(cl);
            close(newfd);
            free(cl);
        } else {
            pthread_detach(tid);
            char cid[64];
            client_id_str(cl, cid, sizeof(cid));
            printf("Accepted connection from %s\n", cid);
        }
    }

    close(server_fd);
    return 0;
}
