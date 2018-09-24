//
// Created by siyahas on 25.04.2018.
//

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#define BUFF_SIZE 1024

bool writeAll(int fd, void *buffer, size_t size);

int main(int argc, char *argv[]){
    if(argc == 1){
        printf("Usage: %s <filepath>", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    char buff[BUFF_SIZE];
    int fd = open(filename, O_RDONLY);
    size_t rb = 0, wb = 0;

    if(fd == -1){
        perror("Could not open the file");
        return 2;
    }

    while((rb = read(fd, &buff, sizeof(buff) - 1)) > 0){
        if(writeAll(STDOUT_FILENO, &buff, rb)){
            perror("Cannot write to stdout");
            return 3;
        }
    }

    if(rb == -1){
        perror("Cannot read file");
        return 4;
    }

    return 0;
}

bool writeAll(int fd, void *buffer, size_t size) {
    size_t total = 0;
    int n;

    do {
        n = write(fd, (uint8_t *) buffer + total, size - total);

        if (n <= 0) {
            return true;
        }

        total += n;
    } while (total != size);

    return false;
}
