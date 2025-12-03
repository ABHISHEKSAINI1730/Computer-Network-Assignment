#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sockfd;
    unsigned char buffer[65536];

    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (sockfd < 0) {
        perror("Socket error");
        return 1;
    }

    printf("Listening for TCP packets...\n");

    while (1) {
        ssize_t data_size = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        struct ethhdr *eth = (struct ethhdr *)buffer;

        struct iphdr *ip = (struct iphdr *)(buffer + sizeof(struct ethhdr));

        if (ip->protocol == 6) {  // TCP = 6
            struct tcphdr *tcp = (struct tcphdr *)(buffer + sizeof(struct ethhdr) + ip->ihl*4);

            printf("\n--- TCP Packet ---\n");
            printf("Source IP      : %s\n", inet_ntoa(*(struct in_addr *)&ip->saddr));
            printf("Destination IP : %s\n", inet_ntoa(*(struct in_addr *)&ip->daddr));
            printf("Source Port    : %u\n", ntohs(tcp->source));
            printf("Dest Port      : %u\n", ntohs(tcp->dest));
            printf("Seq Number     : %u\n", ntohl(tcp->seq));
            printf("Ack Number     : %u\n", ntohl(tcp->ack_seq));
        }
    }

    close(sockfd);
    return 0;
}
