//
// Created by siyahas on 15.04.2018.
//

#include <stddef.h>
#include "debug.h"

#ifndef SYSTEM_HW3_DYNAMIC_H
#define SYSTEM_HW3_DYNAMIC_H

typedef struct dynamic {
    size_t cap;
    size_t used;
    void *data;
} *dynamic_t;

void dynamic_create(dynamic_t dynamic);

void dynamic_add(dynamic_t dynamic, void *data, size_t size);

void dynamic_get(dynamic_t dynamic, void **data, size_t *size);

void dynamic_clear(dynamic_t dynamic);

void dynamic_destroy(dynamic_t dynamic);

#endif //SYSTEM_HW3_DYNAMIC_H
