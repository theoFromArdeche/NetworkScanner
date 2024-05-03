#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define BUFFER_SIZE 4096
#define SERV_PORT 2223

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <Adresse IP>\n", argv[0]);
        return EXIT_FAILURE;
    }


    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erreur lors de la création de la socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("clientTCP: erreur connect\n");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    char message_finished[] = "End of scan\n";
    char message_error[] = "Wrong command\n";
    char message_stop[] = "STOP\n";
    char msg[1024];
    int bytes_received;
    while(1) {
        fgets(msg, sizeof(msg), stdin);
        write(sockfd, msg, strlen(msg));
        if (strcmp(message_stop, msg)==0) break;

        while (1) {
            bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received<=0) continue;
            buffer[bytes_received] = '\0';
            if (strcmp(message_error, buffer)==0) break;
            printf("%s", buffer);
            if (strcmp(message_finished, buffer)==0) break; // STOP
        }
        printf("\n");
        continue;
    }
}