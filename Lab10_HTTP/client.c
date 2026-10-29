#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    const char *method = "GET";

    if (argc > 1) {
        if (strcmp(argv[1], "GET") == 0 || strcmp(argv[1], "HEAD") == 0) {
            method = argv[1];
        } else {
            fprintf(stderr, "Usage: %s [GET|HEAD]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return EXIT_FAILURE;
    }

    char request[512];
    int request_len = snprintf(request, sizeof(request),
                               "%s / HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n",
                               method);

    if (request_len < 0 || (size_t)request_len >= sizeof(request)) {
        fprintf(stderr, "Request is too large.\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    size_t sent = 0;
    while (sent < (size_t)request_len) {
        ssize_t n = send(sockfd, request + sent, (size_t)request_len - sent, 0);
        if (n < 0) {
            perror("send");
            close(sockfd);
            return EXIT_FAILURE;
        }
        sent += (size_t)n;
    }

    char buffer[BUFFER_SIZE];
    ssize_t n;
    while ((n = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n) {
            perror("fwrite");
            close(sockfd);
            return EXIT_FAILURE;
        }
    }

    if (n < 0) {
        perror("recv");
        close(sockfd);
        return EXIT_FAILURE;
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
