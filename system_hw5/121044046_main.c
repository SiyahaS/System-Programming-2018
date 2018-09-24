#include "debug.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <float.h>

struct florist {
    int c;
    int x;
    int y;
    double speed;
    char *name;
    char **flowers;
};

struct client {
    char *name;
    int x;
    int y;
    char *flower;
};

struct queue {
    struct client c;
    struct queue *next;
};

struct thread_args {
    struct florist *florist;
    struct queue *queue;
    unsigned int seed;
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
};

struct thread_return {
    unsigned int time;
    unsigned int sales;
};

struct dat_file *readDatFile(char *filepath) __attribute_warn_unused_result__;

struct florist *readFlorists(FILE *datfile, size_t *c);

void *floristJob(void *arg);

double distance(double x1, double y1, double x2, double y2);

void cleanupOrder(struct queue **order);

int findClosestFlorist(struct florist *florists, size_t fc, struct client *client);

static volatile int end = 0; /* Represents the end of day */
static volatile int ready = 0; /*  */
static pthread_cond_t b = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t barrier = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: floristApp <DAT_FILE>\n");
        return 1;
    }
    FILE *datfile = fopen(argv[1], "r");

    if (datfile == NULL) {
        printf("Cannot open datfile: %s\n", argv[1]);
        return 2;
    }
    size_t fc = 0;
    struct florist *florists = readFlorists(datfile, &fc);
    pthread_t *threads = calloc(fc, sizeof(pthread_t));
    pthread_cond_t *conds = calloc(fc, sizeof(*conds));
    pthread_mutex_t *mutexes = calloc(fc, sizeof(*mutexes));
    struct thread_args *args = calloc(fc, sizeof(*args));
    struct thread_return **results = calloc(fc, sizeof(*results));
    srand(time(NULL));

    for (int i = 0; i < fc; ++i) {/* initialize the thread arguments and create the thread */
        args[i].cond = conds + i;
        args[i].queue = NULL;
        args[i].mutex = mutexes + i;
        args[i].florist = florists + i;
        args[i].seed = (unsigned int) rand();

        pthread_cond_init(conds + i, NULL);
        pthread_mutex_init(mutexes + i, NULL);
        pthread_create(threads + i, NULL, floristJob, args + i);
    }

    pthread_mutex_lock(&barrier);
    while (ready != fc) {
        DERROR("[COND_CHECK] %d\n", ready);
        pthread_cond_wait(&b, &barrier);
    }
    pthread_mutex_unlock(&barrier);

    /* read clients and send to appropriate queues */
    char *line = NULL;
    size_t len = 0;
    ssize_t rb;
    char *token = NULL;
    char *saveptr = NULL;
#define CLIENT_DELIM " \n(),;:"
    while ((rb = getline(&line, &len, datfile)) != -1 && rb != 1) {
        struct queue *order = malloc(sizeof(*order));
        order->next = NULL;
        token = strtok_r(line, CLIENT_DELIM, &saveptr); /* NAME */
        order->c.name = malloc(sizeof(char) * (strlen(token) + 1));
        memcpy(order->c.name, token, sizeof(char) * strlen(token) + 1);
        DERROR("[CLIENT][NAME] %s ", order->c.name);
        token = strtok_r(NULL, CLIENT_DELIM, &saveptr); /* COORDINATE X */
        sscanf(token, "%d", &(order->c.x));
        DERROR_C("[X] %d ", order->c.x);
        token = strtok_r(NULL, CLIENT_DELIM, &saveptr); /* COORDINATE Y */
        sscanf(token, "%d", &(order->c.y));
        DERROR_C("[Y] %d ", order->c.y);
        token = strtok_r(NULL, CLIENT_DELIM, &saveptr); /* FLOWER */
        order->c.flower = malloc(sizeof(char) * (strlen(token) + 1));
        memcpy(order->c.flower, token, sizeof(char) * strlen(token) + 1);
        DERROR_C("[FLOWER] %s\n", order->c.flower);

        int fi = findClosestFlorist(florists, fc, &(order->c));

        /* turn into a function if possible */
        if (fi == -1) {
            pthread_mutex_lock(&barrier);
            printf("[ERROR] %s wanted a non-sale flower: %s\n", order->c.name, order->c.flower);
            pthread_mutex_unlock(&barrier);
        } else {
            pthread_mutex_lock(mutexes + fi);
            if (args[fi].queue == NULL) {
                args[fi].queue = order;
                pthread_cond_signal(conds + fi);
            } else {
                struct queue *addTo = args[fi].queue;
                while (addTo->next != NULL)
                    addTo = addTo->next;
                addTo->next = order;
            }
            pthread_mutex_unlock(mutexes + fi);
        }
    }
    fclose(datfile);
    free(line);
#undef CLIENT_DELIM

    end = 1;
    for (int i = 0; i < fc; ++i) {
        pthread_cond_signal(conds + i); /* unstuck threads waiting for more job */
    }

    /* cleanup florist threads */
    for (int i = 0; i < fc; ++i) {
        pthread_join(threads[i], (void **) (results + i));
    }

    printf("Sale statistics for today:\n");
    printf("---------------------------------------------------\n");
    printf("Florist          # of Sales       Total Time       \n");
    printf("---------------------------------------------------\n");
    for (int i = 0; i < fc; ++i) {
        printf("%-17s%-17d%-17d\n", florists[i].name, results[i]->sales, results[i]->time);
    }
    printf("---------------------------------------------------\n");

    /* cleanup the mess */
    for (int i = 0; i < fc; ++i) {
        DERROR("[FREE] (%p)results[%d]\n", results + i, i);
        free(results[i]);
        DERROR("[FREE] florist[%d].name\n", i);
        free(florists[i].name);
        for (int j = 0; j < florists[i].c; ++j) {
            DERROR("[FREE] florist[%d].flowers[%d]\n", i, j);
            free(florists[i].flowers[j]);
        }
        DERROR("[FREE] florist[%d].flowers\n", i);
        free(florists[i].flowers);
        DERROR("[COND_DESTROY] conds[%d]\n", i);
        pthread_cond_destroy(conds + i);
        DERROR("[MUTEX_DESTROY] mutexes[%d]\n", i);
        pthread_mutex_destroy(mutexes + i);
    }

    DERROR("[FREE] args\n");
    free(args);
    DERROR("[FREE] conds\n");
    free(conds);
    DERROR("[FREE] threads\n");
    free(threads);
    DERROR("[FREE] results\n");
    free(results);
    DERROR("[FREE] mutexes\n");
    free(mutexes);
    DERROR("[FREE] florists\n");
    free(florists);

    return 0;
}

#define FLORIST_DELIM " \n(),;:"

struct florist *readFlorists(FILE *datfile, size_t *c) {
    struct florist *florists = NULL;
    ssize_t rb;
    char *line = NULL;
    char *token;
    char *saveptr;
    size_t len = 0;

    while ((rb = getline(&line, &len, datfile)) != -1 && rb != 1) {
        struct florist florist = {.flowers = NULL, .c = 0};
        token = strtok_r(line, FLORIST_DELIM, &saveptr); // NAME
        florist.name = malloc(sizeof(char) * (strlen(token) + 1));
        memcpy(florist.name, token, sizeof(char) * strlen(token) + 1);
        DERROR("[FLORIST][NAME] %s ", florist.name);
        token = strtok_r(NULL, FLORIST_DELIM, &saveptr); // COORDINATE X
        sscanf(token, "%d", &(florist.x));
        DERROR_C("[X] %d ", florist.x);
        token = strtok_r(NULL, FLORIST_DELIM, &saveptr); // COORDINATE Y
        sscanf(token, "%d", &(florist.y));
        DERROR_C("[Y] %d ", florist.y);
        token = strtok_r(NULL, FLORIST_DELIM, &saveptr); // SPEED
        sscanf(token, "%lf", &(florist.speed));
        DERROR_C("[SPEED] %.2f ", florist.speed);
        while ((token = strtok_r(NULL, FLORIST_DELIM, &saveptr)) != NULL) { // FLOWERS
            florist.flowers = realloc(florist.flowers, (florist.c + 1) * sizeof(char *));
            florist.flowers[florist.c] = malloc(sizeof(char) * (strlen(token) + 1));
            if (florist.flowers[florist.c] == NULL) {
                perror("Malloc error");
                exit(2);
            }
            memcpy(florist.flowers[florist.c], token, sizeof(char) * strlen(token) + 1);
            DERROR_C("[FLOWER] %s ", florist.flowers[florist.c]);
            florist.c += 1;
        }
        DERROR_C("\n");
        florists = realloc(florists, sizeof(*florists) * (*c + 1));
        memcpy(&(florists[*c]), &florist, sizeof(florist));
        *c += 1;
    }

    free(line);
    return florists;
}

#undef FLORIST_DELIM

void *floristJob(void *arg) {
    struct thread_args *args = arg;
    struct thread_return *retVal = calloc(1, sizeof(*retVal));
    unsigned int seed = args->seed;

    pthread_mutex_lock(&barrier);
    ready += 1;
    pthread_mutex_unlock(&barrier);
    pthread_cond_signal(&b);
    while (!end) {
        pthread_mutex_lock(args->mutex);
        while (args->queue == NULL) {
            pthread_cond_wait(args->cond, args->mutex);
            if (args->queue != NULL) {
                break;
            } else if (end) {
                DERROR("[FLORIST] %s unstuck\n", args->florist->name);
                return retVal;
            }
        }

        struct queue *order __attribute__((cleanup(cleanupOrder))) = args->queue;

        args->queue = args->queue->next; /* Move to next item in queue */
        pthread_mutex_unlock(args->mutex); /* We got what we need they can continue adding */

        unsigned int r = (unsigned int) (rand_r(&seed) % 300 + 10);
        unsigned int prepTime = (unsigned int) (distance(args->florist->x, args->florist->y, order->c.x, order->c.y) /
                                                args->florist->speed);

        usleep((r + prepTime) * 1000);
        pthread_mutex_lock(&barrier);
        printf("Florist %s has delivered a %s to %s in (r)%dms + (p)%dms = %dms\n", args->florist->name,
               order->c.flower, order->c.name,
               r, prepTime, r + prepTime);
        pthread_mutex_unlock(&barrier);
        retVal->sales += 1;
        retVal->time += r + prepTime;
    }

    while (args->queue != NULL) { /* process the remaining orders */
        struct queue *order __attribute__((cleanup(cleanupOrder))) = args->queue;

        pthread_mutex_lock(args->mutex);
        args->queue = args->queue->next;
        pthread_mutex_unlock(args->mutex);

        unsigned int r = (unsigned int) (rand_r(&seed) % 40 + 10);
        unsigned int prepTime = (unsigned int) (distance(args->florist->x, args->florist->y, order->c.x, order->c.y) /
                                                args->florist->speed);

        usleep((r + prepTime) * 1000);
        pthread_mutex_lock(&barrier);
        printf("Florist %s has delivered a %s to %s in (r)%dms + (p)%dms = %dms\n", args->florist->name,
               order->c.flower, order->c.name,
               r, prepTime, r + prepTime);
        pthread_mutex_unlock(&barrier);
        retVal->sales += 1;
        retVal->time += r + prepTime;
    }

    return retVal;
}

double distance(double x1, double y1, double x2, double y2) {
    return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

void cleanupOrder(struct queue **order) {
    DERROR("[FREE] order->c.name\n");
    free((*order)->c.name);
    DERROR("[FREE] order->c.flower\n");
    free((*order)->c.flower);
    DERROR("[FREE] order\n");
    free(*order);
    *order = NULL;
}

int findClosestFlorist(struct florist *florists, size_t fc, struct client *client) {
    int fi = -1;
    double fd = 0;
    for (int i = 0; i < fc; ++i) { /* find the closest florist that has the flower */
        for (int j = 0; j < florists[i].c; ++j) {
            if (strncmp(florists[i].flowers[j], client->flower, strlen(florists[i].flowers[j])) == 0) {
                if (fi == -1) {
                    fd = distance(florists[i].x, florists[i].y, client->x, client->y);
                    fi = i;
                } else {
                    double nfd = distance(florists[i].x, florists[i].y, client->x, client->y);
                    if (nfd < fd) {
                        fd = nfd;
                        fi = i;
                    }
                }
            }
        }
    }

    return fi;
}

