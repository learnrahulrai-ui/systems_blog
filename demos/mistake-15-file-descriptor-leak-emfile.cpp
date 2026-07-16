// ============================================================================
// DEMO: Mistake 15 -- File Descriptor Leak EMFILE
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-15-file-descriptor-leak-emfile.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-15-file-descriptor-leak-emfile.cpp
//
// RUN (two terminals):
//   ./demo support     -- terminal 1, accepts forever, NEVER closes client_fd
//   ./demo consumer    -- terminal 2, fires 5000 clean connect+close cycles
//
// WHAT THIS PROVES:
//   The server never calls close(client_fd). Even though the consumer
//   is perfectly clean on its own end, the server's socket sits in
//   CLOSE_WAIT and permanently holds a file descriptor slot. Repeat
//   ~2048 times (the default MSVC CRT limit per process) and accept()
//   starts failing with EMFILE / WSAENOBUFS.
//
//   accept() client ---> returns fd N ---> handle client ---> (no close(fd)) ---> leaks fd N
//   repeat 2048 times ---> hits the limit ---> accept() returns -1 with EMFILE ---> server breaks
// ============================================================================

#include <stdio.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void support() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));

    listen(server_fd, 2048); // Massive backlog so the OS doesn't reject them

    std::cout << "Support started. Accepting connections nonstop!" << std::endl;

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);

        if (client_fd == -1) {
            std::cout << "\nBOOM! The OS refused to give us another file descriptor!" << std::endl;
            perror("accept failed");
            break; // End the loop
        }

        std::cout << "Accepted client on fd: " << client_fd << std::endl;

        // WE ARE NOT CLOSING IT! WE ARE LEAKING EVERY SINGLE FD!
        // close(client_fd);
    }
    close(server_fd);
}

void consumer() {
    std::cout << "Consumer launching a massive connection flood..." << std::endl;

    for (int i = 0; i < 5000; i++) {
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);

        if (client_fd == -1) {
            std::cout << "\nBOOM! Consumer ran out of file descriptors!" << std::endl;
            perror("socket failed");
            break;
        }

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

        if (connect(client_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
            perror("connect failed");
            break;
        }

        // THIS CONSUMER IS PERFECTLY CLEAN!
        // It does its job and properly closes its socket.
        close(client_fd);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    if (strcmp(argv[1], "support") == 0) support();
    if (strcmp(argv[1], "consumer") == 0) consumer();
    return 0;
}
