//
// Created by siyahas on 22.05.2018.
//

#ifndef SYSTEM_HW5_DEBUG_H
#define SYSTEM_HW5_DEBUG_H

#ifdef DEBUG
#define DERROR(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##args)
#define DERROR_C(fmt, args...) fprintf(stderr, fmt, ##args)
#define DCALL(stmt) stmt
#else
#define DERROR(fmt, args...)
#define DERROR_C(fmt, args...)
#define DCALL(stmt)
#endif

#endif //SYSTEM_HW5_DEBUG_H
