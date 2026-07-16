// ============================================================================
// DEMO: Mistake 18 -- The Illusion of write Return Values, Data in Flight
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-18-the-illusion-of-write-return-values-data-in-flight.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-18-write-return-illusion.cpp
//
// RUN (two terminals, same loopback IP for both, e.g. 127.0.0.1):
//   ./demo support 127.0.0.1     -- terminal 1, pushes 100MB of random bytes
//   ./demo consumer 127.0.0.1    -- terminal 2, reads once, then SITS ON the
//                                    socket without closing it (see comment
//                                    below) to show data still "in flight"
//
// WHAT THIS PROVES:
//   write(100MB) returning some byte count only means the LOCAL kernel
//   accepted that many bytes into its own send buffer. It says nothing
//   about whether the peer's application ever read them. If the peer
//   aborts while bytes are still between the two kernels, they are
//   gone -- even though write() already reported them as "written".
//
//   write(100MB) ---> returns N (bytes accepted into the local kernel's send buffer)
//                              |
//                 does NOT mean the consumer received them
//                              |
//          if the consumer aborts now, whatever was still in flight is gone
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
#include <cstdlib>
#include <ctime>

void fill_random(char* buf, size_t size) {
    srand(time(NULL));
    for (size_t i = 0; i < size; ++i) {
        buf[i] = (char)(rand() % 256);
    }
}

void support(const char* ip) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 100);

    int client_fd = accept(server_fd, nullptr, nullptr);

    char* buffer = new char[100000000]; // 100MB
    fill_random(buffer, 100000000);

    int written_now = write(client_fd, buffer, 100000000);
    sleep(2);

    std::cout << "Server (IP " << ip << ") successfully pushed " << written_now << " bytes into the network." << std::endl;

    close(client_fd);
    close(server_fd);
    delete[] buffer;
}

void consumer(const char* ip) {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    inet_pton(AF_INET, ip, &address.sin_addr);

    connect(client_fd, (struct sockaddr *)&address, sizeof(address));

    int requested_bytes = 3000000; // 3MB
    char* buffer = new char[requested_bytes];

    sleep(2);

    int bytes_read = read(client_fd, buffer, requested_bytes);
    std::cout << "Consumer read " << bytes_read << " bytes, then holding the socket open..." << std::endl;

    // deliberately NOT closing here -- lets data still in flight sit
    // unread while we sleep, matching the post's "data in flight" claim
    // close(client_fd);
    sleep(13);
    delete[] buffer;
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    if (argc < 3) return 1;
    if (strcmp(argv[1], "support") == 0) support(argv[2]);
    if (strcmp(argv[1], "consumer") == 0) consumer(argv[2]);
    return 0;
}
