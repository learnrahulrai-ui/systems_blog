// ============================================================================
// DEMO: Mistake 11 -- The TIME_WAIT / EADDRINUSE bind bug
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-11-the-timewait-eaddrinuse-bind-bug.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-11-timewait-eaddrinuse-bind-bug.cpp
//
// RUN (two terminals):
//   ./demo support     -- terminal 1, binds port 8080, accepts one client
//   ./demo consumer    -- terminal 2, connects, reads the greeting
//
// WHAT THIS PROVES:
//   Stop the server and restart it right away on the same port and
//   bind() can fail with EADDRINUSE, even though nothing else is using
//   that port -- the OS is still holding it in TIME_WAIT. SO_REUSEADDR
//   (set below, before bind) tells the OS this process understands the
//   risk and wants to rebind anyway.
//
//   server exits ---> socket enters TIME_WAIT (2-4 minutes)
//   restart server ---> bind() to same port ---> error: EADDRINUSE
//   fix: setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))
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

    // FIX 1: Prevent TIME_WAIT port binding bugs
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("bind failed");
        close(server_fd);
        return;
    }

    listen(server_fd, 100);

    std::cout << "Support is waiting for a connection..." << std::endl;

    int client_fd = accept(server_fd, nullptr, nullptr);

    std::cout << "Accepted client on fd: " << client_fd << std::endl;
    int write_res = write(client_fd, "hello consumer", 14);
    if (write_res == -1)
        std::cout << "accept passed ; but write failed";

    struct linger sl;
    sl.l_onoff = 1;
    sl.l_linger = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_LINGER, (const char*)&sl, sizeof(sl));

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
    } else if (bytes_read == 0) {
        std::cout << "The server cleanly hung up (FIN)." << std::endl;
    } else {
        perror("Read failed (Who killed me?)");
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
