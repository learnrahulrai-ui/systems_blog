// ============================================================================
// DEMO: Mistake 7 -- Partial writes
// Blog post: https://learnrahulrai-ui.github.io/systems_blog/posts/mistake-7-partial-writes.html
//
// COMPILE (no Makefile / CMake needed, one command):
//   g++ -std=c++17 -o demo mistake-07-partial-writes.cpp
//
// RUN:
//   ./demo support     -- writes "hello consumer" into chat.txt
//   ./demo consumer     (unused in this demo; kept for interface symmetry)
//
// WHAT THIS PROVES:
//   write() returns the number of bytes it actually wrote, which can be
//   LESS than what you asked for. Correct code loops until
//   bytes_written == total_len. This file writes a short string in one
//   shot to keep the demo runnable end to end, but the diagram below is
//   the general case that breaks on a large payload / a full send buffer.
//
//   [process] --write(100 bytes)--> [kernel send buffer, only 40 bytes free]
//                                          |
//                                 returns 40 (a partial write)
//                                          |
//                    the remaining 60 bytes were never written --
//                    caller must loop: while (bytes_written < total_len)
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
    std::cout << "Wrote \"hello consumer\" to chat.txt" << std::endl;
}

void consumer() {
    std::cout << "hello; this is consumer" << std::endl;
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
