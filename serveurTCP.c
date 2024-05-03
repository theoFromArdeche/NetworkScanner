#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>


#define MIN_PORTS 1
#define MAX_PORTS 1024

#ifdef _OPENMP
#include <omp.h>
#else
#define omp_get_thread_num() 0
#define omp_get_num_threads() 1
#endif


#define SERV_PORT 2223
#define BUFFER_SIZE 4096


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
void send_ping(int sockfd, const struct sockaddr_in *dest_addr, int dialogSocket) {
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
            char msg_from_serv[256];
            sprintf(msg_from_serv, "Received ICMP Echo Reply from: %s, ICMP Seq=%d\n", inet_ntoa(sender.sin_addr), ntohs(icmp_hdr->un.echo.sequence));
            send(dialogSocket, msg_from_serv, strlen(msg_from_serv), 0);
        }
    }
}

int ip_scanner(int argc, char *argv[], int dialogSocket) {
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
        send_ping(sockfd, &dest, dialogSocket);
        // Sleep to throttle ntohl(broadcast.s_addr)the rate of pings
        usleep(10000); // 10 ms
    }

    close(sockfd);
    return EXIT_SUCCESS;
}


int port_scanner(int argc, char *argv[], int dialogSocket) {
    if (argc != 2 && argc != 4) {
        fprintf(stderr, "Usage: %s <IP>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int cur_max_port=MAX_PORTS, cur_min_port=MIN_PORTS;
    if (argc==4) {
        cur_min_port=atoi(argv[2]);
        cur_max_port=atoi(argv[3]);
        if (cur_max_port>65535) cur_max_port=65535;
    }

    const char *ip_addr = argv[1];

    #pragma omp parallel for num_threads(200)
    for (int port = cur_min_port; port <= cur_max_port; port++) {
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        inet_pton(AF_INET, ip_addr, &dest.sin_addr);
        dest.sin_port = htons(port);

        int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd < 0) {
            perror("Socket creation failed");
            continue;
            //return EXIT_FAILURE;
        }

        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags < 0) {
            perror("Fcntl failed");
            close(sockfd);
            continue;
            //return EXIT_FAILURE;
        }

        if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
            perror("Fcntl failed");
            close(sockfd);
            continue;
            //return EXIT_FAILURE;
        }

        int connection_result = connect(sockfd, (struct sockaddr *)&dest, sizeof(dest));
        if (connection_result == 0) {
            char msg_from_serv[256];
            sprintf(msg_from_serv, "Port %d is open\n", port);
            send(dialogSocket, msg_from_serv, strlen(msg_from_serv), 0);
        } else if (errno == EINPROGRESS) {
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(sockfd, &write_fds);

            struct timeval timeout = {5, 0}; // 1 second timeout
            int select_result = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);
            if (select_result > 0) {
                int so_error;
                socklen_t len = sizeof(so_error);
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
                if (so_error == 0) {
                    char msg_from_serv[256];
                    sprintf(msg_from_serv, "Port %d is open\n", port);
                    send(dialogSocket, msg_from_serv, strlen(msg_from_serv), 0);
                }
            } else {
                //printf("Port %d is filtered\n", port);
            }
        } else if (errno == ECONNREFUSED) {
            // Port is closed
        } else {
            perror("Connect failed");
            continue;
            //return EXIT_FAILURE;
        }

        close(sockfd);
    }

    return EXIT_SUCCESS;
}





int main() {
    int serverSocket;
    /*
    * Ouvrir socket (socket STREAM)
    */
    if ((serverSocket = socket(PF_INET, SOCK_STREAM, 0))<0) {
        perror("erreur socket");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    /*
    * Lier l'adresse locale à la socket
    */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET ;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_PORT);
    if (bind(serverSocket,(struct sockaddr*)&serv_addr, sizeof(serv_addr))<0) {
        perror("servecho: erreur bind\n");
        exit(1);
    }


    if (listen(serverSocket,SOMAXCONN) <0) {
        perror("servecho: erreur listen\n");
        exit(1);
    }



    int dialogSocket;
    int clilen;
    struct sockaddr_in cli_addr;
    clilen = sizeof(cli_addr);
    dialogSocket = accept(serverSocket,(struct sockaddr *)&cli_addr,(socklen_t *)&clilen);
    if (dialogSocket < 0) {
        perror("servecho : erreur accept\n");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    int bytes_received;
    char message_stop[] = "STOP\n";
    char message_error[]="Wrong command\n";
    char message_end[]="End of scan\n";
    char *token;
    int argc, choice=0;
    char *argv_ip[3];
    char *argv_ports[4];
    while (1) {
        bytes_received = recv(dialogSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received<=0) continue;
        buffer[bytes_received] = '\0';
        printf("Received from client: %s\n", buffer);
        if (strcmp(message_stop, buffer)==0) break; // STOP

        token = strtok(buffer, " ");

        if (strcmp(token, "IP")==0) choice=1;
        else if (strcmp(token, "PORTS")==0) choice=2;
        else {
            send(dialogSocket, message_error, strlen(message_error), 0);
            continue;
        };
        printf("%s scan\n", token);

        argc=0;
        while (token != NULL && argc<5) {
            if (choice==1) argv_ip[argc]=token;
            else argv_ports[argc]=token;
            token = strtok(NULL, " ");
            argc+=1;
        }
        

        if (choice==1) { // Scan IP
            argv_ip[2]=strtok(argv_ip[2], "\n");
            ip_scanner(argc, argv_ip, dialogSocket);
        } else { // Scan Ports
            argv_ports[3]=strtok(argv_ports[3], "\n");
            port_scanner(argc, argv_ports, dialogSocket);
        }
        
        
        send(dialogSocket, message_end, strlen(message_end), 0);
        printf("End of scan\n\n");

        // ip_addr start_port end_port
        // ip_addr netmask

        memset(buffer, 0, BUFFER_SIZE);
    }

    printf("Client disconnected\n");

    close(dialogSocket);
    close(serverSocket);

    return 0;

}