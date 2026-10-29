#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BACKLOG 10
#define REQUEST_SIZE 4096
#define WWW_ROOT "www"
#define INDEX_FILE "www/index.html"

static void http_date(time_t value, char *out, size_t out_size) {
    struct tm tm_value;
    gmtime_r(&value, &tm_value);
    strftime(out, out_size, "%a, %d %b %Y %H:%M:%S GMT", &tm_value);
}

static int send_all(int fd, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = send(fd, data + sent, length - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static void send_error_response(int client_fd, int status, const char *reason,
                                const char *message, const char *method) {
    char date[64], header[1024];
    time_t now = time(NULL);
    http_date(now, date, sizeof(date));

    int body_len = (int)strlen(message);
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.0 %d %s\r\n"
        "Date: %s\r\n"
        "Server: Lab10-HTTP/1.0\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        status, reason, date, body_len);

    if (header_len > 0 && (size_t)header_len < sizeof(header)) {
        send_all(client_fd, header, (size_t)header_len);
        if (strcmp(method, "HEAD") != 0) {
            send_all(client_fd, message, (size_t)body_len);
        }
    }
}

static void handle_client(int client_fd) {
    char request[REQUEST_SIZE];
    size_t used = 0;

    while (used < sizeof(request) - 1) {
        ssize_t n = recv(client_fd, request + used, sizeof(request) - 1 - used, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            return;
        }
        if (n == 0) break;
        used += (size_t)n;
        request[used] = '\0';
        if (strstr(request, "\r\n\r\n") != NULL) break;
    }

    if (used == 0) return;
    request[used] = '\0';

    char method[16] = {0}, path[1024] = {0}, version[16] = {0};
    if (sscanf(request, "%15s %1023s %15s", method, path, version) != 3) {
        send_error_response(client_fd, 400, "Bad Request",
                            "400 Bad Request\n", "");
        return;
    }

    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        send_error_response(client_fd, 405, "Method Not Allowed",
                            "405 Method Not Allowed\n", method);
        return;
    }

    if (strcmp(path, "/") != 0 && strcmp(path, "/index.html") != 0) {
        send_error_response(client_fd, 404, "Not Found",
                            "404 Not Found\n", method);
        return;
    }

    FILE *file = fopen(INDEX_FILE, "rb");
    if (file == NULL) {
        send_error_response(client_fd, 404, "Not Found",
                            "404 Not Found\n", method);
        return;
    }

    struct stat st;
    if (stat(INDEX_FILE, &st) != 0) {
        fclose(file);
        send_error_response(client_fd, 404, "Not Found",
                            "404 Not Found\n", method);
        return;
    }

    char date[64], modified[64], header[1024];
    http_date(time(NULL), date, sizeof(date));
    http_date(st.st_mtime, modified, sizeof(modified));

    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.0 200 OK\r\n"
        "Date: %s\r\n"
        "Server: Lab10-HTTP/1.0\r\n"
        "Last-Modified: %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n\r\n",
        date, modified, (long long)st.st_size);

    if (header_len > 0 && (size_t)header_len < sizeof(header)) {
        if (send_all(client_fd, header, (size_t)header_len) == 0 &&
            strcmp(method, "HEAD") != 0) {
            char buffer[REQUEST_SIZE];
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                if (send_all(client_fd, buffer, n) < 0) break;
            }
        }
    }

    fclose(file);
}

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("HTTP server listening on port %d\n", PORT);
    printf("Serving %s (paths / and /index.html)\n", INDEX_FILE);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        printf("Connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
