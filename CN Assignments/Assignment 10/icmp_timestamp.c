#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <time.h>

unsigned short checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    while (nwords--) sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage: %s <target_ip>\n", argv[0]);
        return 1;
    }

    char *dst = argv[1];

    int s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (s < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, dst, &addr.sin_addr);

    char packet[64];
    memset(packet, 0, sizeof(packet));

    struct icmphdr *icmph = (struct icmphdr*)packet;

    icmph->type = 13;   // Timestamp request
    icmph->code = 0;
    icmph->un.echo.id = htons(0xABCD);
    icmph->un.echo.sequence = htons(1);

    // create timestamp payload (originate, receive, transmit)
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm gm;
    time_t sec = ts.tv_sec;
    gmtime_r(&sec, &gm);
    uint32_t ms_since_midnight =
        gm.tm_hour*3600*1000 +
        gm.tm_min*60*1000 +
        gm.tm_sec*1000 +
        (ts.tv_nsec / 1000000);

    uint32_t *body = (uint32_t*)(packet + sizeof(struct icmphdr));
    body[0] = htonl(ms_since_midnight);
    body[1] = 0;
    body[2] = 0;

    icmph->checksum = checksum((unsigned short*)packet, (sizeof(struct icmphdr) + 12)/2);

    if (sendto(s, packet, sizeof(struct icmphdr)+12, 0,
        (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        close(s);
        return 1;
    }

    printf("Sent ICMP Timestamp Request to %s\n", dst);

    close(s);
    return 0;
}
