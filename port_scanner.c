#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_PORTS 2500

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip_addr = argv[1];
    int port;
    struct sockaddr_in dest;

    for (port = 1; port <= MAX_PORTS; port++) {
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        inet_pton(AF_INET, ip_addr, &dest.sin_addr);
        dest.sin_port = htons(port);

        int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd < 0) {
            perror("Socket creation failed");
            return EXIT_FAILURE;
        }

        int connection_result = connect(sockfd, (struct sockaddr *)&dest, sizeof(dest));
        if (connection_result == 0) {
            printf("Port %d is open\n", port);
        } else if (errno == ECONNREFUSED) {
            // Port is closed
        } else {
            perror("Connect failed");
            return EXIT_FAILURE;
        }

        close(sockfd);
    }

    return EXIT_SUCCESS;
}
