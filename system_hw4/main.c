#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#define NUM_OF_COMBINATIONS 7

#ifdef DEBUG
#define DERROR2(fmt, args...) fprintf(stderr, fmt, __FILE__, __LINE__, __func__, ##args)
#define DERROR(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##args)
#else
#define DERROR(fmt, args...)
#endif

#define SHM_KEY "/system_hw04_shm"

enum sem_indexes {
    EGG_FLOUR, EGG_BUTTER, EGG_SUGAR, FLOUR_BUTTER, FLOUR_SUGAR, BUTTER_SUGAR, PRODUCER
};

static volatile sig_atomic_t flag = 0;

void baker(const char *wm, const char *tm, sem_t *sem, sem_t *producer);
void signalHandler(int const sig);

int main() {
    int err;
    sem_t *sems;
    struct sigaction handler;
    int shm_fd;

    handler.sa_handler = signalHandler;
    handler.sa_flags = 0;

    err = sigemptyset(&handler.sa_mask);
    if (err == -1) {
        DERROR("[%d]sigemptyset failed\n", getpid());
        perror("sigemptyset");
        return 1;
    }
    DERROR("[%d]sigemptyset success\n", getpid());

    err = sigaction(SIGINT, &handler, NULL);

    if (err == -1) {
        DERROR("[%d]sigaction failed\n", getpid());
        perror("sigaction");
        return 1;
    }
    DERROR("[%d]sigaction success\n", getpid());

    shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

    if (shm_fd == -1) {
        DERROR("[%d]shm_open failed\n", getpid());
        perror("shm_open");
        return -1;
    }
    DERROR("[%d]shm_open success\n", getpid());

    err = shm_unlink(SHM_KEY);

    if(err == -1){
        DERROR("[%d]shm_unlink failed\n", getpid());
        perror("shm_unlink");
        return -5;
    }
    DERROR("[%d]shm_unlink success\n", getpid());

    err = ftruncate(shm_fd, sizeof(sem_t) * NUM_OF_COMBINATIONS);

    if (err == -1) {
        DERROR("[%d]ftruncate failed\n", getpid());
        perror("ftruncate");
        return -2;
    }
    DERROR("[%d]ftruncate success\n", getpid());

    sems = mmap(NULL, sizeof(sem_t) * NUM_OF_COMBINATIONS, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if(sems == MAP_FAILED){
        DERROR("[%d]mmap failed\n", getpid());
        perror("mmap");
        return -3;
    }
    DERROR("[%d]mmap success\n", getpid());

    for(int i = 0 ; i < 6 ; ++i){
        err = sem_init(&sems[i], 1, (unsigned int)0);
        if(err == -1){
            DERROR("[%d]sem_init[%d] failed\n", getpid(), i);
            perror("sem_init");
            return -4;
        }
        DERROR("[%d]sem_init[%d] success\n", getpid(), i);
    }

    err = sem_init(&sems[6], 1, (unsigned int)1);
    if(err == -1){
        DERROR("[%d]sem_init failed\n", getpid());
        perror("sem_init");
        return -4;
    }
    DERROR("[%d]sem_init success\n", getpid());

    char *waitMessage;
    char *tookMessage;
    for(int i = 0 ; i < 6 ; ++i){
        switch (fork()){
            case 0:
                // EGG_FLOUR, EGG_BUTTER, EGG_SUGAR, FLOUR_BUTTER, FLOUR_SUGAR, BUTTER_SUGAR, PRODUCER
                DERROR("[%d]Chef%d born.\n", getpid(), i);
                switch (i){
                    case 0:
                        waitMessage = "Chef1 is waiting for FLOUR and EGG";
                        tookMessage = "Chef1 has taken FLOUR and EGG";
                        break;
                    case 1:
                        waitMessage = "Chef2 is waiting for EGG and BUTTER";
                        tookMessage = "Chef2 has taken EGG and BUTTER";
                        break;
                    case 2:
                        waitMessage = "Chef3 is waiting for EGG and SUGAR";
                        tookMessage = "Chef3 has taken EGG and SUGAR";
                        break;
                    case 3:
                        waitMessage = "Chef4 is waiting for FLOUR and BUTTER";
                        tookMessage = "Chef4 has taken FLOUR and BUTTER";
                        break;
                    case 4:
                        waitMessage = "Chef5 is waiting for FLOUR and SUGAR";
                        tookMessage = "Chef5 has taken FLOUR and SUGAR";
                        break;
                    case 5:
                        waitMessage = "Chef6 is waiting for BUTTER and SUGAR";
                        tookMessage = "Chef6 has taken BUTTER and SUGAR";
                        break;
                }
                baker(waitMessage, tookMessage, &sems[i], &sems[6]);
                //sem_close(&sems[i]);
                err = munmap(sems, sizeof(sem_t) * 7);
                if(err == -1){
                    DERROR("[%d]munmap failed\n", getpid());
                    perror("munmap");
                    return -1;
                }
                DERROR("[%d]munmap success\n", getpid());
                printf("Chef%d exits gracefully.\n", i + 1);
                fflush(stdout);
                exit(0);
            case -1:
                DERROR("[%d]Chef%d aborted.\n", getpid(), i);
                perror("fork");
                err = kill(-getpid(), SIGINT);

                if(err == -1){
                    DERROR("[%d]fork kill failed\n", getpid());
                    perror("fork kill");
                    return -1;
                }
                DERROR("[%d]fork kill success\n", getpid());
                break;
            default:
                // Parent
                break;
        }
    }


    srand(0);
    int random;
    while(!flag){
        err = sem_wait(&sems[6]);

        if(err == -1){
            DERROR("[%d]sem_wait producer failed\n", getpid());
            if(errno != EINTR)
                perror("sem_wait producer");
            break;
        }

        DERROR("[%d]sem_wait producer success\n", getpid());

        random = rand() % 6;
        DERROR("[%d]random[%d]: \n", getpid(), random);

        // EGG_FLOUR, EGG_BUTTER, EGG_SUGAR, FLOUR_BUTTER, FLOUR_SUGAR, BUTTER_SUGAR, PRODUCER
        switch (random){
            case 0:
                DERROR("[%d]EGG and FLOUR\n", getpid());
                printf("Wholesaler delivers EGG and FLOUR\n");
                break;
            case 1:
                DERROR("[%d]EGG and BUTTER\n", getpid());
                printf("Wholesaler delivers EGG and BUTTER\n");
                break;
            case 2:
                DERROR("[%d]EGG and SUGAR\n", getpid());
                printf("Wholesaler delivers EGG and SUGAR\n");
                break;
            case 3:
                DERROR("[%d]FLOUR and BUTTER\n", getpid());
                printf("Wholesaler delivers FLOUR and BUTTER\n");
                break;
            case 4:
                DERROR("[%d]FLOUR and SUGAR\n", getpid());
                printf("Wholesaler delivers FLOUR and SUGAR\n");
                break;
            case 5:
                DERROR("[%d]BUTTER and SUGAR\n", getpid());
                printf("Wholesaler delivers BUTTER and SUGAR\n");
                break;
        }
        fflush(stdout);
        sem_post(&sems[random]);
    }

    for(int i = 0 ; i < 7 ; ++i){
        err = sem_destroy(&sems[i]);
        if(err == -1){
            perror("sem_destroy");
        }
    }
    err = munmap(sems, sizeof(sem_t) * 7);
    if(err == -1){
        DERROR("[%d]munmap failed\n", getpid());
        perror("munmap");
        return -1;
    }
    DERROR("[%d]munmap success\n", getpid());

    kill(-getpid(), SIGINT);
    int chld=0;
    while((chld = wait(NULL)) != -1)
        printf("chef process cleaned up\n");

    printf("Wholesaler exits gracefully.\n");

    return 0;
}

void baker(const char *wm, const char *tm, sem_t *sem, sem_t *producer){
    while (!flag){
        printf("%s\n", wm);
        fflush(stdout);
        if(!flag && sem_wait(sem) == -1){
            if(errno != EINTR)
                perror("sem_wait");
            return;
        }
        printf("%s\n", tm);
        fflush(stdout);
        if(!flag && sem_post(producer) == -1){
            if(errno != EINTR)
                perror("sem_post");
            return;
        }

    }
}

void signalHandler(int const sig){
    if(sig == SIGINT) {
        flag = 1;
        DERROR("[%d]sigint recv\n", getpid());
    }
}
