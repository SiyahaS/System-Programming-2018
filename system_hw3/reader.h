//
// Created by siyahas on 15.04.2018.
//

#include <unistd.h>
#include "debug.h"

#ifndef SYSTEM_HW3_READER_H
#define SYSTEM_HW3_READER_H

#define READER_BUFF_SIZE 1024

typedef struct reader {
    int fd;
    char buff[READER_BUFF_SIZE];
    off_t offset;
    size_t size;
} *reader_t;

void reader_create(reader_t reader, const int fd);

char reader_getc(reader_t reader);

void reader_destroy(reader_t reader);

#endif //SYSTEM_HW3_READER_H
