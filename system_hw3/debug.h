//
// Created by siyahas on 16.04.2018.
//

#include <stdio.h>

#ifndef SYSTEM_HW3_DEBUG_H
#define SYSTEM_HW3_DEBUG_H

#ifdef DEBUG
#define DERROR(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##args)
#else
#define DERROR(fmt, args...)
#endif

#endif //SYSTEM_HW3_DEBUG_H
