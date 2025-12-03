// udp_calc_client.c
// Compile: gcc udp_calc_client.c -o udp_calc_client
// Run: ./udp_calc_client <server_ip> <server_port>
// Example: ./udp_calc_client 10.0.0.1 8080

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define BUF 1024
#define TIMEOUT_SEC 2
#define MAX_RETRIES 3

// helper: generate a simple unique id (timestamp + counter)
char *make_id(char *buf, size_t n) {
    static int ctr = 0;
    ctr++;
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(buf, n, "%ld%03d", (long)ts.tv_sec, ctr % 1000);
    return buf;
}

int main(int argc, char *argv[]) {
    if (argc < 3) { printf("Usage: %s <server_ip> <server_port>\n", argv[0]); return 1; }
    char *ip = argv[1]; int port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv;
    memset(&serv,0,sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv.sin_addr);

    // set recv timeout
    struct timeval tv; tv.tv_sec = TIMEOUT_SEC; tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    char line[512];
    printf("UDP Calculator client. Type expressions like: sin 1.57   or   3 * 4  or  inv 2\n");
    printf("Type QUIT to exit.\n");

    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        // trim newline
        char *p = strchr(line, '\n'); if (p) *p=0;
        if (strcasecmp(line, "QUIT")==0) break;
        char id[64]; make_id(id, sizeof(id));
        char sendbuf[BUF];
        snprintf(sendbuf, sizeof(sendbuf), "%s|%s", id, line);

        int tries = 0;
        int got = 0;
        while (tries < MAX_RETRIES && !got) {
            ssize_t s = sendto(sock, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&serv, sizeof(serv));
            if (s < 0) { perror("sendto"); break; }
            // wait for reply
            char recvbuf[BUF];
            ssize_t r = recvfrom(sock, recvbuf, sizeof(recvbuf)-1, 0, NULL, NULL);
            if (r < 0) {
                tries++;
                printf("(no reply, retry %d/%d)\n", tries, MAX_RETRIES);
                continue;
            }
            recvbuf[r]=0;
            // expect ID|OK|result  OR ID|ERR|errmsg
            char rid[64]; char status[16]; char payload[512];
            char *pipe1 = strchr(recvbuf, '|');
            if (!pipe1) { printf("Malformed reply: %s\n", recvbuf); got=1; break; }
            int L1 = pipe1 - recvbuf; strncpy(rid, recvbuf, L1); rid[L1]=0;
            char *rest = pipe1 + 1;
            char *pipe2 = strchr(rest, '|');
            if (!pipe2) { printf("Malformed reply: %s\n", recvbuf); got=1; break; }
            int L2 = pipe2 - rest; strncpy(status, rest, L2); status[L2]=0;
            strncpy(payload, pipe2+1, sizeof(payload)-1); payload[sizeof(payload)-1]=0;

            if (strcmp(rid, id) != 0) {
                // reply for different tx (rare), ignore and keep waiting
                printf("(ignored reply for id %s)\n", rid);
                continue;
            }

            if (strcmp(status, "OK")==0) {
                printf("Result: %s\n", payload);
            } else {
                printf("Server error: %s\n", payload);
            }
            got = 1;
            break;
        }
        if (!got) {
            printf("No reply after %d retries — possible packet loss.\n", MAX_RETRIES);
        }
    }

    close(sock);
    return 0;
}

