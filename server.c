#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>

#ifdef _OPENMP
#include <omp.h>
#else
#define omp_get_thread_num() 0
#define omp_get_num_threads() 1
#endif

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

// Fonction pour envoyer un ping
void send_ping(int sockfd, const struct sockaddr_in *dest_addr) {
    char packet[512];
    struct icmphdr *icmp_hdr = (struct icmphdr *) packet;
    memset(packet, 0, sizeof(packet));
    
    icmp_hdr->type = ICMP_ECHO;
    icmp_hdr->code = 0;
    icmp_hdr->un.echo.id = htons(getpid());
    icmp_hdr->un.echo.sequence = 1;
    
    size_t packet_size = sizeof(struct icmphdr) + 56;
    memset(packet + sizeof(struct icmphdr), 'a', 56);
    
    icmp_hdr->checksum = 0;
    icmp_hdr->checksum = checksum(packet, packet_size);

    if (sendto(sockfd, packet, packet_size, 0, (struct sockaddr *)dest_addr, sizeof(struct sockaddr_in)) <= 0) {
        perror("sendto");
    }

    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    if (recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&sender, &sender_len) <= 0) {
        perror("Recv failed");
    } else {
        struct iphdr *ip_hdr = (struct iphdr *)packet;
        icmp_hdr = (struct icmphdr *)(packet + (ip_hdr->ihl << 2));  // Skip IP header
        if (icmp_hdr->type == ICMP_ECHOREPLY) {
            printf("Received ICMP Echo Reply from: %s, ICMP Seq=%d\n", inet_ntoa(sender.sin_addr), ntohs(icmp_hdr->un.echo.sequence));
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Adresse IP> <Masque de sous-réseau>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct in_addr ip;
    struct in_addr mask;
    struct in_addr network;
    struct in_addr broadcast;

    inet_pton(AF_INET, argv[1], &ip);
    inet_pton(AF_INET, argv[2], &mask);

    network.s_addr = ip.s_addr & mask.s_addr;
    broadcast.s_addr = network.s_addr | ~mask.s_addr;

    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    

    // Boucle pour envoyer un ping à toutes les adresses du sous-réseau
    #pragma omp parallel for num_threads(20) schedule(dynamic, 5)
    for (unsigned long addr = ntohl(network.s_addr) + 1; addr < ntohl(broadcast.s_addr); addr++) {
        struct sockaddr_in dest;
        dest.sin_family = AF_INET;

        dest.sin_addr.s_addr = htonl(addr);
        send_ping(sockfd, &dest);
        // Sleep to throttle ntohl(broadcast.s_addr)the rate of pings
        usleep(10000); // 10 ms
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
