#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PSEUDO_HDR_SIZE 12
#define BUF_SIZE 4096

// -------- YOUR ROLL NUMBER HERE --------
#define ROLL_NUMBER "CSM24062"
// ----------------------------------------

// Pseudo header for TCP checksum
struct pseudo_header {
    uint32_t saddr;
    uint32_t daddr;
    uint8_t zero;
    uint8_t proto;
    uint16_t tcplength;
};

unsigned short checksum(unsigned short *ptr,int nbytes) {
    long sum = 0;
    unsigned short oddbyte;
    unsigned short res;

    while(nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if(nbytes == 1) {
        oddbyte = 0;
        *((unsigned char*)&oddbyte) = *(unsigned char*)ptr;
        sum += oddbyte;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    res = (unsigned short)~sum;
    return res;
}

int main(int argc, char *argv[]) {

    if(argc < 3) {
        printf("Usage: %s <target_ip> <target_port>\n", argv[0]);
        return 1;
    }

    char *dst_ip = argv[1];
    int dst_port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if(sock < 0){
        perror("socket");
        return 1;
    }

    int one = 1;
    if(setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0){
        perror("setsockopt");
        close(sock);
        return 1;
    }

    char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);

    struct iphdr *iph = (struct iphdr *)buffer;
    struct tcphdr *tcph = (struct tcphdr *)(buffer + sizeof(struct iphdr));
    char *data = buffer + sizeof(struct iphdr) + sizeof(struct tcphdr);

    // insert roll number into TCP payload
    int data_len = snprintf(data, 128, "%s", ROLL_NUMBER);

    // IP header
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr) + data_len);
    iph->id = htons(54321);
    iph->frag_off = 0;
    iph->ttl = 64;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;

    // SET YOUR SOURCE IP ACCORDING TO SENDING HOST
    // Example: sender is h1 = 10.0.0.1
    char src_ip[] = "10.0.0.1"; 
    inet_pton(AF_INET, src_ip, &iph->saddr);
    inet_pton(AF_INET, dst_ip, &iph->daddr);

    iph->check = checksum((unsigned short *)iph, sizeof(struct iphdr));

    // TCP header
    tcph->source = htons(54321);
    tcph->dest = htons(dst_port);
    tcph->seq = htonl(0);
    tcph->ack_seq = 0;
    tcph->doff = 5;
    tcph->psh = 1;
    tcph->ack = 1;
    tcph->window = htons(65535);
    tcph->urg_ptr = 0;
    tcph->check = 0;

    // TCP checksum (pseudo header)
    struct pseudo_header psh;
    psh.saddr = iph->saddr;
    psh.daddr = iph->daddr;
    psh.zero = 0;
    psh.proto = IPPROTO_TCP;
    psh.tcplength = htons(sizeof(struct tcphdr) + data_len);

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + data_len;
    char *pseudogram = malloc(psize);

    memcpy(pseudogram, &psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));
    memcpy(pseudogram + sizeof(struct pseudo_header) + sizeof(struct tcphdr), data, data_len);

    tcph->check = checksum((unsigned short*)pseudogram, psize);
    free(pseudogram);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = tcph->dest;
    inet_pton(AF_INET, dst_ip, &sin.sin_addr);

    if (sendto(sock, buffer, sizeof(struct iphdr) + sizeof(struct tcphdr) + data_len, 0,
               (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("sendto");
    } else {
        printf("Sent TCP packet to %s:%d with payload \"%s\"\n", dst_ip, dst_port, ROLL_NUMBER);
    }

    close(sock);
    return 0;
}
