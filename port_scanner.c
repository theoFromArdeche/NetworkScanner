#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>

#define MIN_PORTS 1
#define MAX_PORTS 1024

#ifdef _OPENMP
#include <omp.h>
#else
#define omp_get_thread_num() 0
#define omp_get_num_threads() 1
#endif


// to compile : gcc port_scanner.c -o port_scanner -fopenmp -O3
// to run : ./port_scanner ip_addr min_port max_port
// or : ./port_scanner ip_addr 
// with no min max port given it will by default go from port 1 to port 1024


int main(int argc, char *argv[]) {
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
            printf("Port %d is open\n", port);
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
                    printf("Port %d is open\n", port);
                }
            } else {
                printf("Port %d is filtered\n", port);
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
