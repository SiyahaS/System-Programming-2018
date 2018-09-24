//
// Created by siyahas on 15.04.2018.
//

#include <stdlib.h>
#include <memory.h>
#include "dynamic.h"

#define SIZE 1024

void dynamic_create(dynamic_t dynamic) {
    dynamic->cap = SIZE;
    dynamic->used = 0;
    dynamic->data = malloc(sizeof(char) * SIZE);
}

void dynamic_add(dynamic_t dynamic, void *data, size_t size) {
    if (dynamic->used + size >= dynamic->cap) {
        dynamic->cap += SIZE;

        void *old = dynamic->data;
        dynamic->data = malloc(sizeof(char) * dynamic->cap);
        memcpy(dynamic->data, old, sizeof(dynamic->used));
        free(old);
    }
    memcpy(&(((char *)dynamic->data)[dynamic->used]), data, size);

    dynamic->used += size;
}

void dynamic_get(dynamic_t dynamic, void **data, size_t *size) {
    *data = dynamic->data;
    *size = dynamic->used;
}

void dynamic_clear(dynamic_t dynamic) {
    dynamic->used = 0;
}

void dynamic_destroy(dynamic_t dynamic) {
    free(dynamic->data);
    dynamic->used = 0;
    dynamic->cap = 0;
}