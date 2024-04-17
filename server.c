#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>

// Fonction pour calculer le checksum
unsigned short checksum(void *b, int len) {    
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char *)buf;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}
 
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP>\n", argv[0]);
        exit(1);
    }

    const char *ip_addr = argv[1];
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    // Destination pour le paquet ICMP
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = 0;
    inet_pton(AF_INET, ip_addr, &dest.sin_addr);

    // Construire l'en-tête ICMP
    char packet[512];
    memset(packet, 0, sizeof(packet));
    struct icmphdr *icmp_hdr = (struct icmphdr *) packet;
    icmp_hdr->type = ICMP_ECHO;
    icmp_hdr->code = 0;
    icmp_hdr->un.echo.id = getpid();
    icmp_hdr->un.echo.sequence = 1;
    icmp_hdr->checksum = 0;
    icmp_hdr->checksum = checksum(packet, sizeof(struct icmphdr));

    // Envoyer le paquet ICMP
    if (sendto(sockfd, packet, sizeof(struct icmphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) <= 0) {
        perror("Send failed");
        return EXIT_FAILURE;
    }

    // Recevoir la réponse
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    if (recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&sender, &sender_len) <= 0) {
        perror("Recv failed");
        return EXIT_FAILURE;
    }

    struct iphdr *ip_hdr = (struct iphdr *) packet;
    icmp_hdr = (struct icmphdr *) (packet + (ip_hdr->ihl * 4)); // ihl is the header length in 32-bit words

    if (icmp_hdr->type == ICMP_ECHOREPLY) {
        printf("Received ICMP Echo Reply from %s: ICMP Seq=%d\n", ip_addr, icmp_hdr->un.echo.sequence);
    } else {
        printf("Received non-echo-reply message\n");
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
