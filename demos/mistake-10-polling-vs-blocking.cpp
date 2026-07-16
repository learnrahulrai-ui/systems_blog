// ============================================================================
// DEMO: Mistake 10 -- Polling vs Blocking
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-10-polling-vs-blocking.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-10-polling-vs-blocking.cpp
//
// RUN (two terminals, in order):
//   ./demo support      -- terminal 1, writes chat.txt
//   ./demo consumer     -- terminal 2, reads chat.txt back
//
// WHAT THIS PROVES:
//   A plain file has no blocking-read primitive. read() on a file returns
//   immediately whether new data exists or not, so the only way to watch
//   a file for changes is a while(true) { read(); } loop that burns 100%
//   of a CPU core. Sockets and pipes are different: read() on those CAN
//   block, letting the OS park the thread and wake it only when data
//   actually arrives.
//
//   consumer, polling:  while(true) -> read() -> returns -1/0 -> loop again -> 100% CPU
//   consumer, blocking: read() -> no data -> OS parks the thread          ->   0% CPU
//                                             | (data arrives)
//                                     OS wakes the thread up
// ============================================================================

#include <stdio.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>

void support() {
    int fd = open("chat.txt", O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        perror("open failed");
        return;
    }

    char buffer[256];
    int len = strlen("hello consumer");
    strcpy(buffer, "hello consumer");
    write(fd, buffer, len);

    close(fd);
}

void consumer() {
    int fd = open("chat.txt", O_RDONLY);
    if (fd == -1) {
        perror("open failed");
        return;
    }

    char buffer[256];
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);

    if (bytes_read == -1) {
        perror("read failed");
        close(fd);
        return;
    }

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::cout << buffer << std::endl;
    }

    close(fd);
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
