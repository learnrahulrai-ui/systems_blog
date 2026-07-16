// ============================================================================
// DEMO: Mistake 16 -- TCP TIME_WAIT Port Exhaustion & SO_LINGER, the HTTP/1.0 problem
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-16-tcp-timewait-port-exhaustion-solinger-the-http10-problem.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-16-timewait-port-exhaustion-solinger.cpp
//
// RUN (two terminals):
//   ./demo support     -- terminal 1, accepts one client, pushes 100MB
//   ./demo consumer    -- terminal 2, reads exactly 3,000,000 bytes
//
// WHAT THIS PROVES:
//   A single, un-looped write() of 100MB only pushes whatever fits in
//   the kernel send buffer right now -- the code below deliberately
//   leaves the retry loop commented out so you can see the single-shot
//   write() return value directly. Compare against mistake-7's fix.
//
//   normal close: close(fd) ---> sends FIN ---> enters TIME_WAIT (kernel hoards port/RAM 2-4 min)
//   linger close: SO_LINGER (l_onoff=1, l_linger=0) ---> sends RST ---> vaporizes connection instantly
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

    int flag = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    char* buffer = new char[100000000]; // 100MB
    memset(buffer, 'A', 100000000);

    int total_written = 0;
    char* current_offset = buffer;

    // while (total_written < 100000000) {
        int written_now = write(client_fd, current_offset, 100000000 - total_written);
        // if (written_now <= 0) break;
        current_offset += written_now;
        total_written += written_now;
    // }

    std::cout << "Server successfully pushed " << total_written << " bytes into the network." << std::endl;
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

    // We want 3 Megabytes
    int requested_bytes = 3000000;
    char* buffer = new char[requested_bytes];

    std::cout << "Consumer asking OS to read exactly 3,000,000 bytes..." << std::endl;

    int total_read = 0;
    char* current_offset = buffer;

    while (total_read < requested_bytes) {
        int bytes_read = read(client_fd, current_offset, requested_bytes - total_read);
        if (bytes_read == 0) {
            std::cout << "\n[CONSUMER] read() returned 0! The Server gracefully hung up with a FIN!" << std::endl;
            break;
        }
        if (bytes_read < 0) {
            std::cout << "\n[CONSUMER] read() returned " << bytes_read << "! The Server crashed and fired an RST!" << std::endl;
            break;
        }
        current_offset += bytes_read;
        total_read += bytes_read;
    }

    std::cout << "Consumer actually received: " << total_read << " bytes." << std::endl;

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
