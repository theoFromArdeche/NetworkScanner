#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <errno.h>

// Fonction pour calculer le checksum
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char*)buf;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip_addr = argv[1];
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, ip_addr, &dest.sin_addr);

    char packet[512];
    memset(packet, 0, sizeof(packet));

    struct icmphdr *icmp_hdr = (struct icmphdr *) packet;
    icmp_hdr->type = ICMP_ECHO;
    icmp_hdr->code = 0;
    icmp_hdr->un.echo.id = getpid();
    icmp_hdr->un.echo.sequence = 1;
    icmp_hdr->checksum = 0;
    icmp_hdr->checksum = checksum(packet, sizeof(struct icmphdr) + sizeof(struct iphdr));

    // Ajout d'un peu de données au paquet ICMP pour simuler un vrai ping
    size_t packet_size = sizeof(struct icmphdr) + sizeof(struct iphdr) + 56; // Taille typique du payload de ping
    icmp_hdr->checksum = checksum(packet, packet_size);

    if (sendto(sockfd, packet, packet_size, 0, (struct sockaddr *)&dest, sizeof(dest)) <= 0) {
        perror("Send failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    if (recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&sender, &sender_len) <= 0) {
        perror("Recv failed");
        return EXIT_FAILURE;
    }

    struct iphdr *ip_hdr = (struct iphdr *)packet;
    icmp_hdr = (struct icmphdr *)(packet + (ip_hdr->ihl << 2)); // Skip IP header

    if (icmp_hdr->type == ICMP_ECHOREPLY) {
        printf("Received ICMP Echo Reply from: %s, ICMP Seq=%d\n", inet_ntoa(sender.sin_addr), icmp_hdr->un.echo.sequence);
    } else {
        printf("Received non-echo-reply message\n");
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
