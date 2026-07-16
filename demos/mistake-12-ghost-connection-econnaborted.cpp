// ============================================================================
// DEMO: Mistake 12 -- The Ghost Connection ECONNABORTED accept bug
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-12-the-ghost-connection-econnaborted-accept-bug.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-12-ghost-connection-econnaborted.cpp
//
// RUN (two terminals, consumer must finish BEFORE support wakes up):
//   ./demo support     -- terminal 1, sleeps 10s, then calls accept()
//   ./demo consumer    -- terminal 2, connects then immediately RSTs
//
// WHAT THIS PROVES:
//   The OS completes the TCP handshake before your code ever calls
//   accept(). If the client connects and dies (RST) before accept()
//   runs, the connection is dead sitting in the kernel's queue.
//   accept() still returns, but with -1, not a valid descriptor --
//   ECONNABORTED / ghost connection. Skip the -1 check and the very
//   next line calling write() on that descriptor fails or crashes.
//
//   client connects ---> TCP handshake (OS kernel queue) ---> client Ctrl+C / RST
//                                                                   |
//   server accept() <---------------------------------------- returns -1
//          |
//     write(-1) ---> server dies or fails silently
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
    std::cout << "Sleeping for 10s... While I sleep, the consumer will connect and send a brutal RST." << std::endl;

    sleep(10);

    std::cout << "\nAwake! Calling blocking accept()..." << std::endl;
    int client_fd = accept(server_fd, nullptr, nullptr);

    std::cout << "accept() returned FD: " << client_fd << std::endl;

    if (client_fd != -1) {
        int bytes = write(client_fd, "hello consumer", 14);
        std::cout << "write() returned: " << bytes << std::endl;
        close(client_fd);
    } else {
        perror("accept failed");
    }

    close(server_fd);
    std::cout << "Support exiting." << std::endl;
}

void consumer() {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    std::cout << "Consumer connecting..." << std::endl;
    if (connect(client_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("connect failed");
        return;
    }
    std::cout << "Consumer connected! In queue." << std::endl;

    // TRICK: Force an RST instead of a clean FIN when we close
    struct linger sl;
    sl.l_onoff = 1;
    sl.l_linger = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_LINGER, (const char*)&sl, sizeof(sl));

    std::cout << "Consumer sending brutal RST (Ctrl+C simulation) and dying..." << std::endl;
    close(client_fd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    if (strcmp(argv[1], "support") == 0) support();
    if (strcmp(argv[1], "consumer") == 0) consumer();
    return 0;
}
