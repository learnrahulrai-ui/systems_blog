// ============================================================================
// DEMO: Mistake 17 -- Partial Reads and Kernel Buffers, the 212KB Wall
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-17-partial-reads-and-kernel-buffers-the-212kb-wall.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-17-partial-reads-212kb-wall.cpp
//
// RUN (two terminals):
//   ./demo support     -- terminal 1, single write() of 100MB, no loop
//   ./demo consumer    -- terminal 2, single read() asking for 3MB, no loop
//
// WHAT THIS PROVES:
//   read() does not wait around collecting everything you asked for.
//   It hands back whatever is currently sitting in the kernel's receive
//   buffer and returns immediately -- on this machine, that number
//   lands near 212KB, far short of the 3,000,000 bytes requested below.
//
//   client requests 3MB via read() ---> kernel receive buffer has only ~212KB available
//                                                |
//                                     returns ~212KB instantly, call already done
//                                                |
//                          the remaining ~2.8MB requires calling read() again, and again
// ============================================================================

#include <stdio.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netinet/tcp.h>

void support() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 100);

    int client_fd = accept(server_fd, nullptr, nullptr);

    char* buffer = new char[100000000]; // 100MB
    memset(buffer, 'A', 100000000);

    // ==========================================
    // single write, no loop -- write 100MB in one go
    // ==========================================
    int written_now = write(client_fd, buffer, 100000000);
    sleep(2);

    std::cout << "Server successfully pushed " << written_now << " bytes into the network." << std::endl;

    close(client_fd);
    close(server_fd);
    delete[] buffer;
}

void consumer() {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    connect(client_fd, (struct sockaddr *)&address, sizeof(address));

    int requested_bytes = 3000000; // 3MB
    char* buffer = new char[requested_bytes];

    std::cout << "Consumer asking OS to read exactly 3,000,000 bytes..." << std::endl;
    sleep(2);

    // ==========================================
    // single read, no loop -- ask for all 3MB in one go
    // ==========================================
    int bytes_read = read(client_fd, buffer, requested_bytes);

    std::cout << "Consumer actually received: " << bytes_read << " bytes." << std::endl;

    close(client_fd);
    delete[] buffer;
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    if (argc < 2) return 1;
    if (strcmp(argv[1], "support") == 0) support();
    if (strcmp(argv[1], "consumer") == 0) consumer();
    return 0;
}
