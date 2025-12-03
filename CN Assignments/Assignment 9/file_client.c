// file_client.c
// Compile: gcc file_client.c -o file_client
// Run: ./file_client <server_ip> <port> <client_directory>
// Example: ./file_client 10.0.0.1 9090 /home/mininet/client_dir

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

#define BUF 8192

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

int connect_server(const char *ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    struct sockaddr_in serv;
    memset(&serv,0,sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv.sin_addr);
    if (connect(s, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect"); close(s); return -1;
    }
    return s;
}

void do_download(const char *ip, int port, const char *server_fname, const char *client_dir) {
    int s = connect_server(ip, port);
    if (s < 0) return;

    char header[512];
    snprintf(header, sizeof(header), "DOWNLOAD|%s\n", server_fname);
    send(s, header, strlen(header), 0);

    // read response line
    char resp[256];
    ssize_t r = recv(s, resp, sizeof(resp)-1, 0);
    if (r <= 0) { close(s); return; }
    resp[r] = 0;
    if (strncmp(resp, "OK|", 3) != 0) {
        printf("Server error: %s\n", resp);
        close(s); return;
    }
    long long filesize = atoll(resp + 3);
    printf("Server reports filesize = %lld bytes\n", filesize);

    // prepare to save
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s/%s", client_dir, server_fname);
    int outfd = open(outpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (outfd < 0) { perror("open out"); close(s); return; }

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_REALTIME, &t_start);

    long long remaining = filesize;
    char buf[BUF];
    while (remaining > 0) {
        ssize_t want = remaining > sizeof(buf) ? sizeof(buf) : remaining;
        ssize_t got = full_recv(s, buf, want);
        if (got <= 0) { printf("recv error during download\n"); break; }
        write(outfd, buf, got);
        remaining -= got;
    }

    clock_gettime(CLOCK_REALTIME, &t_end);
    double secs = timediff_sec(t_end, t_start);
    printf("Downloaded %lld bytes in %.6f s\n", filesize - remaining, secs);

    close(outfd);
    close(s);
}

void do_upload(const char *ip, int port, const char *client_fname, const char *client_dir) {
    char inpath[1024];
    snprintf(inpath, sizeof(inpath), "%s/%s", client_dir, client_fname);
    int infd = open(inpath, O_RDONLY);
    if (infd < 0) { perror("open input"); return; }
    struct stat st; fstat(infd, &st);
    long long filesize = st.st_size;

    int s = connect_server(ip, port);
    if (s < 0) { close(infd); return; }

    char header[512];
    snprintf(header, sizeof(header), "UPLOAD|%s|%lld\n", client_fname, (long long)filesize);
    send(s, header, strlen(header), 0);

    char resp[128];
    ssize_t r = recv(s, resp, sizeof(resp)-1, 0);
    if (r <= 0) { close(infd); close(s); return; }
    resp[r] = 0;
    if (strncmp(resp, "OK", 2) != 0) {
        printf("Server error: %s\n", resp);
        close(infd); close(s); return;
    }

    // start timer
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_REALTIME, &t_start);

    char buf[BUF];
    ssize_t rr;
    while ((rr = read(infd, buf, sizeof(buf))) > 0) {
        if (full_send(s, buf, rr) <= 0) {
            printf("send error during upload\n"); break;
        }
    }

    clock_gettime(CLOCK_REALTIME, &t_end);
    double secs = timediff_sec(t_end, t_start);
    printf("Uploaded %lld bytes in %.6f s\n", filesize, secs);

    close(infd);
    close(s);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <client_directory>\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    const char *client_dir = argv[3];
    mkdir(client_dir, 0755);

    // quick interactive menu
    while (1) {
        printf("\nCommands:\n1) download <filename>\n2) upload <filename>\n3) quit\n> ");
        char line[256];
        if (!fgets(line, sizeof(line), stdin)) break;
        char cmd[32], fname[200];
        if (sscanf(line, "%31s %199s", cmd, fname) < 1) continue;
        if (strcmp(cmd, "download") == 0) {
            do_download(ip, port, fname, client_dir);
        } else if (strcmp(cmd, "upload") == 0) {
            do_upload(ip, port, fname, client_dir);
        } else if (strcmp(cmd, "quit") == 0) {
            break;
        } else {
            printf("Unknown cmd\n");
        }
    }
    return 0;
}

