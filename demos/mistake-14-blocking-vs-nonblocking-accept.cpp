// ============================================================================
// DEMO: Mistake 14 -- Blocking vs Non-Blocking accept Behavior
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-14-blocking-vs-non-blocking-accept-behavior.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-14-blocking-vs-nonblocking-accept.cpp
//
// RUN (two terminals):
//   ./demo support     -- terminal 1, sleeps 10s, blocking accept()
//   ./demo consumer    -- terminal 2, connects, reads the greeting
//
// WHAT THIS PROVES:
//   A blocking accept() call, with no O_NONBLOCK set on the socket,
//   waits FOREVER for a client -- the thread is parked off the CPU
//   entirely until a connection completes. The only two ways it
//   returns: a real client connects, or the process is killed by hand.
//
//   blocking:                  accept() ---> [no client] ---> thread suspended forever
//   non-blocking (O_NONBLOCK): accept() ---> [no client] ---> returns -1 (EWOULDBLOCK) instantly
// ============================================================================

#include <stdio.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void support() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    int opt = 1;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("bind failed");
        close(server_fd);
        return;
    }

    listen(server_fd, 3);

    std::cout << "Support is waiting for a connection..." << std::endl;
    sleep(10);
    int client_fd = accept(server_fd, nullptr, nullptr);

    write(client_fd, "hello consumer", 14);

    close(client_fd);
    close(server_fd);
}

void consumer() {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (connect(client_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("connect failed");
        return;
    }

    char buffer[256];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::cout << buffer << std::endl;
    }
    close(client_fd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ./demo <support|consumer>\n");
        return 1;
    }

    if (strcmp(argv[1], "support") == 0) {
        support();
    }

    if (strcmp(argv[1], "consumer") == 0) {
        consumer();
    }

    return 0;
}
