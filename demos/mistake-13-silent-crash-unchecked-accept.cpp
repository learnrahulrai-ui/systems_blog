// ============================================================================
// DEMO: Mistake 13 -- Silent program crash from unchecked accept return
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-13-silent-program-crash-from-unchecked-accept-return.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-13-silent-crash-unchecked-accept.cpp
//
// RUN (two terminals -- hit Ctrl+C on the consumer when it says to):
//   ./demo support     -- terminal 1, sleeps 10s, accepts, writes twice
//   ./demo consumer    -- terminal 2, connects, then gets Ctrl+C'd
//
// WHAT THIS PROVES:
//   accept() can return -1. Skip the check and the very next write()
//   call runs on an invalid descriptor. The first write after a dead
//   peer often buffers silently with no error; it's the SECOND write
//   that raises SIGPIPE and kills the process with no error printed
//   anywhere -- the server just goes silent.
//
//   accept() fails ---> returns -1 (client_fd)
//   code writes: write(-1, buffer, len) ---> OS returns EBADF (-1 is invalid)
//   unchecked, this can trigger SIGPIPE and kill the server with no error printed
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
    listen(server_fd, 10);

    std::cout << "Support (BLOCKING) started." << std::endl;
    std::cout << "Sleeping for 10s... Quick, run consumer and hit Ctrl+C to abort it!" << std::endl;

    sleep(10);

    std::cout << "\nAwake! Calling blocking accept()..." << std::endl;
    int client_fd = accept(server_fd, nullptr, nullptr);

    std::cout << "accept() returned fake valid FD: " << client_fd << std::endl;

    if (client_fd != -1) {
        std::cout << "Attempting 1st write... (This just buffers into RAM silently)" << std::endl;
        write(client_fd, "hello", 5);

        std::cout << "Waiting 1 sec for OS to realize consumer sent an RST..." << std::endl;
        sleep(1);

        std::cout << "Attempting 2nd write... THIS WILL TRIGGER SIGPIPE AND KILL THE SERVER!" << std::endl;
        write(client_fd, "world", 5);

        std::cout << "YOU WILL NEVER SEE THIS TEXT BECAUSE THE SERVER IS DEAD." << std::endl;
        close(client_fd);
    }

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

    std::cout << "Consumer connected! HIT Ctrl+C NOW to abort the connection!" << std::endl;

    char buffer[256];
    read(client_fd, buffer, sizeof(buffer) - 1);
    close(client_fd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    if (strcmp(argv[1], "support") == 0) support();
    if (strcmp(argv[1], "consumer") == 0) consumer();
    return 0;
}
