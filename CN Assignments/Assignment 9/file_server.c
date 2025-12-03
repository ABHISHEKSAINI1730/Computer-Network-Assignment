// file_server.c
// Compile: gcc file_server.c -o file_server
// Run: ./file_server <port> <server_directory>
// Example: ./file_server 9090 /home/mininet/server_dir

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define BACKLOG 5
#define BUF 8192

// time diff in seconds (double)
double timediff_sec(struct timespec a, struct timespec b) {
    return (a.tv_sec - b.tv_sec) + (a.tv_nsec - b.tv_nsec) / 1e9;
}

ssize_t full_recv(int fd, void *buf, size_t len) {
    size_t received = 0;
    char *p = buf;
    while (received < len) {
        ssize_t r = recv(fd, p + received, len - received, 0);
        if (r <= 0) return r;
        received += r;
    }
    return received;
}

ssize_t full_send(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    const char *p = buf;
    while (sent < len) {
        ssize_t s = send(fd, p + sent, len - sent, 0);
        if (s <= 0) return s;
        sent += s;
    }
    return sent;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <server_directory>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    const char *server_dir = argv[2];

    // create dir if not exists
    mkdir(server_dir, 0755);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) { perror("bind"); return 1; }
    if (listen(sock, BACKLOG) < 0) { perror("listen"); return 1; }

    printf("File server listening on port %d, dir=%s\n", port, server_dir);

    while (1) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int fd = accept(sock, (struct sockaddr*)&cli, &clilen);
        if (fd < 0) { perror("accept"); continue; }

        char clientip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(cli.sin_addr), clientip, sizeof(clientip));
        printf("Accepted connection from %s:%d\n", clientip, ntohs(cli.sin_port));

        // read a command line (ending with '\n')
        char hdr[512];
        ssize_t r = recv(fd, hdr, sizeof(hdr)-1, 0);
        if (r <= 0) { close(fd); continue; }
        hdr[r] = 0;
        // only consider up to newline
        char *nl = strchr(hdr, '\n');
        if (nl) *nl = 0;

        // parse
        if (strncmp(hdr, "DOWNLOAD|", 9) == 0) {
            char *filename = hdr + 9;
            // build full path
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", server_dir, filename);
            int infd = open(path, O_RDONLY);
            if (infd < 0) {
                char resp[256];
                snprintf(resp, sizeof(resp), "ERR|%s\n", strerror(errno));
                send(fd, resp, strlen(resp), 0);
                close(fd);
                continue;
            }
            struct stat st;
            fstat(infd, &st);
            off_t filesize = st.st_size;
            char resp[256];
            snprintf(resp, sizeof(resp), "OK|%lld\n", (long long)filesize);
            send(fd, resp, strlen(resp), 0);

            // start timer on server side
            struct timespec t_start, t_end;
            clock_gettime(CLOCK_REALTIME, &t_start);

            // send file
            char buf[BUF];
            ssize_t rr;
            while ((rr = read(infd, buf, sizeof(buf))) > 0) {
                if (full_send(fd, buf, rr) <= 0) break;
            }
            clock_gettime(CLOCK_REALTIME, &t_end);
            double secs = timediff_sec(t_end, t_start);
            printf("Sent file %s (%lld bytes) to %s:%d in %.6f s\n", filename, (long long)filesize, clientip, ntohs(cli.sin_port), secs);
            // also send server-side timing info as a final small message? Not necessary — client times too.

            close(infd);
            close(fd);
        }
        else if (strncmp(hdr, "UPLOAD|", 7) == 0) {
            // format: UPLOAD|filename|filesize
            char *p = hdr + 7;
            char *p2 = strchr(p, '|');
            if (!p2) {
                send(fd, "ERR|bad_upload_header\n", 22, 0);
                close(fd); continue;
            }
            *p2 = 0;
            char *filename = p;
            long long filesize = atoll(p2 + 1);
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", server_dir, filename);
            int outfd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
            if (outfd < 0) {
                char resp[256];
                snprintf(resp, sizeof(resp), "ERR|%s\n", strerror(errno));
                send(fd, resp, strlen(resp), 0);
                close(fd); continue;
            }
            send(fd, "OK\n", 3, 0);

            // receive file and measure
            struct timespec t_start, t_end;
            clock_gettime(CLOCK_REALTIME, &t_start);

            long long remaining = filesize;
            char buf[BUF];
            while (remaining > 0) {
                ssize_t toread = remaining > sizeof(buf) ? sizeof(buf) : remaining;
                ssize_t rec = full_recv(fd, buf, toread);
                if (rec <= 0) { break; }
                write(outfd, buf, rec);
                remaining -= rec;
            }

            clock_gettime(CLOCK_REALTIME, &t_end);
            double secs = timediff_sec(t_end, t_start);
            printf("Received file %s (%lld bytes) from %s:%d in %.6f s\n", filename, filesize - remaining, clientip, ntohs(cli.sin_port), secs);

            close(outfd);
            close(fd);
        }
        else {
            send(fd, "ERR|unknown_command\n", 20, 0);
            close(fd);
        }
    }

    close(sock);
    return 0;
}


