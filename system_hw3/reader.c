//
// Created by siyahas on 15.04.2018.
//

#include <unistd.h>
#include <stdio.h>
#include "reader.h"

#define SIZE READER_BUFF_SIZE

void reader_create(reader_t reader, const int fd) {
    reader->fd = fd;
    reader->size = 0;
    reader->offset = 0;
}

char reader_getc(reader_t reader) {
    char c;
    if (reader->size - reader->offset == 0) {
        reader->size = read(reader->fd, reader->buff, sizeof(reader->buff));
        if (reader->size == -1) {
            DERROR("[READER][GETC] Cannot read file");
            perror("");
        }
    }
    c = (reader->buff)[reader->offset];
    reader->offset += 1;
    return c;
}

void reader_destroy(reader_t reader) {
    reader->size = 0;
}