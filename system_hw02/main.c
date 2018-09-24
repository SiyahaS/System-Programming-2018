#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <memory.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#ifdef DEBUG
#define DERROR(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##args)
#else
#define DERROR(fmt, args...)
#endif

#define strcmpSafe(str1, str2, len1, len2) strncmp(str1, str2, sizeof(char) * (len1 < len2 ? len1 : len2))

void producer(int const numc, int const linec, int const fd, pid_t const child);

void consumer(int const numc, int const fd, pid_t const parent);

void signalHandler(int const sig);

bool systemCallFailed(char *const source, char *const msg, int error);

void teardown(char *const source);

bool readAll(int fd, void *buffer, size_t size);

bool writeAll(int fd, void *buffer, size_t size);

void dft(double *numbers, size_t N, size_t k, double *real, double *imag);

void cleanFP(FILE **fpp);

void cleanDouble(double **dpp);

#define R_SIGINT 1
#define R_SIGUSR2 2
#define R_SIGUSR1 3

volatile sig_atomic_t loop = true;
volatile sig_atomic_t reason = 0;

int main(int argc, char *argv[]) {
    int pid;
    int n = 0;
    int m = 0;
    char *x = NULL;
    int f;
    sigset_t sigbl;
    if (argc < 7) {
        printf("Usage: %s -N n -X x -M m\n", argv[0]);
        printf("    -N n: Number of real numbers per line.\n");
        printf("    -X x: Name of the communication file.\n");
        printf("    -M m: Maximum number of lines in file.\n");
        return 1;
    }

    srand(time(NULL));

    for (int i = 0; i < argc - 1; ++i) {
        int len = strlen(argv[i]);
        if (strcmpSafe(argv[i], "-N", len, 2) == 0) {
            sscanf(argv[i + 1], "%d", &n);
            if (n == 0) {
                printf("-N cannot be: %s\n", argv[i + 1]);
                return 1;
            }
        } else if (strcmpSafe(argv[i], "-X", len, 2) == 0) {
            x = argv[i + 1];
        } else if (strcmpSafe(argv[i], "-M", len, 2) == 0) {
            sscanf(argv[i + 1], "%d", &m);
            if (m == 0) {
                printf("-M cannot be: %s\n", argv[i + 1]);
                return 1;
            }
        }
    }

    int fd = open(x, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (fd == -1) {
        perror("[OPEN]");
        return 1;
    }

    if (unlink(x) == -1) {
        perror("[UNLINK]");
        return 1;
    }

    struct sigaction handler;
    handler.sa_handler = signalHandler;
    handler.sa_flags = SA_RESTART;
    int error = sigemptyset(&handler.sa_mask);

    if (error == -1) {
        DERROR("[INIT] Cannot empty signal mask.\n");
        perror("[INIT] sigemptyset failed");
        return 1;
    }

    error = sigaction(SIGINT, &handler, NULL);

    if (error == -1) {
        DERROR("[INIT] Cannot set sigaction(SIGINT).\n");
        perror("[INIT] sigaction failed(SIGINT)");
        return 1;
    }

    error = sigaction(SIGUSR1, &handler, NULL);

    if (error == -1) {
        DERROR("[INIT] Cannot set sigaction.\n");
        perror("[INIT] sigaction failed");
        return 1;
    }

    error = sigaction(SIGUSR2, &handler, NULL);

    if (error == -1) {
        DERROR("[INIT] Cannot set sigaction.\n");
        perror("[INIT] sigaction failed");
        return 1;
    }

    /* Initialize the sigprocmask sigset */
    error = sigemptyset(&sigbl);
    if (error == -1) {
        perror("[INIT] sigemptyset failed");
        return 1;
    }

    error = sigaddset(&sigbl, SIGINT);
    if (error == -1) {
        perror("[INIT] sigaddset failed(SIGINT)");
        return 1;
    }

    error = sigaddset(&sigbl, SIGUSR1);
    if (error == -1) {
        perror("[INIT] sigaddset failed(SIGUSR1)");
        return 1;
    }

    error = sigaddset(&sigbl, SIGUSR2);
    if (error == -1) {
        perror("[INIT] sigaddset failed(SIGUSR2)");
        return 1;
    }

    /* Block signals to make every system call as safe as possible. */
    DERROR("[INIT] Blocking signals.\n");
    error = sigprocmask(SIG_BLOCK, &sigbl, NULL);
    if (error == -1) {
        perror("[INIT] SIG_BLOCK failed");
        return 1;
    }

    switch (pid = fork()) {
        case 0:
            /* Child process: Consumer */
            DERROR("[CHILD] pid: %d\n", getpid());
            consumer(n, fd, getppid());
            close(fd);
            teardown("[CHILD]");
            DERROR("[CHILD] shutdown reason: %s\n",
                   (reason == R_SIGINT ? "SIGINT" : (reason == R_SIGUSR1 ? "SIGUSR1" : "SIGUSR2")));
            return 0;
        case -1:
            /* Error */
            break;
        default:
            /* Parent process: Producer */
            DERROR("[PARENT] child pid: %d\n", pid);
            producer(n, m, fd, pid);
            close(fd);
            teardown("[PARENT]");
            while ((pid = wait(NULL)) != -1)
                DERROR("[PARENT] child handled: %d\n", pid);
            DERROR("[PARENT] all child are handled.\n");
            DERROR("[PARENT] shutdown reason: %s\n",
                   (reason == R_SIGINT ? "SIGINT" : (reason == R_SIGUSR1 ? "SIGUSR1" : "SIGUSR2")));
            return 0;
    }
}

void producer(int const numc, int const linec, int const fd, pid_t const child) {
    struct flock lock;
    off_t curr = 0;
    int error;
    sigset_t sigsp;
    double number;
    sigset_t sigbl;

    FILE *logfp __attribute__((__cleanup__(cleanFP))) = fopen("producer.log", "w");

    if (logfp == NULL) {
        perror("[PARENT] Logfile");
        teardown("[PARENT]");
        return;
    }

    /* Initialize the sigsuspend sigset */
    error = sigfillset(&sigsp);
    if (systemCallFailed("[PARENT]", "sigfillset failed", error)) {
        fprintf(logfp, "sigfillset failed.\n");
        return;
    }

    error = sigdelset(&sigsp, SIGINT);
    if (systemCallFailed("[PARENT]", "sigdelset failed(SIGINT)", error)) {
        fprintf(logfp, "sigdelset failed(SIGINT).\n");
        return;
    }

    error = sigdelset(&sigsp, SIGUSR1);
    if (systemCallFailed("[PARENT]", "sigdelset failed(SIGUSR1)", error)) {
        fprintf(logfp, "sigdelset failed(SIGUSR1).\n");
        return;
    }

    error = sigdelset(&sigsp, SIGUSR2);
    if (systemCallFailed("[PARENT]", "sigdelset failed(SIGUSR2)", error)) {
        fprintf(logfp, "sigdelset failed(SIGUSR2).\n");
        return;
    }

    /* Initialize the sigprocmask sigset */
    error = sigemptyset(&sigbl);
    if (error == -1) {
        fprintf(logfp, "[PARENT] sigemptyset failed");
        return;
    }

    error = sigaddset(&sigbl, SIGINT);
    if (error == -1) {
        fprintf(logfp, "[PARENT] sigaddset failed(SIGINT)");
        return;
    }

    error = sigaddset(&sigbl, SIGUSR2);
    if (error == -1) {
        fprintf(logfp, "[PARENT] sigaddset failed(SIGUSR2)");
        return;
    }

    memset(&lock, 0, sizeof(lock));

    int f = true;
    /* Infinite Loop */
    while (loop) {
        /* Block the SIGINT & SIGUSR2 while in critical section. */
        DERROR("[PARENT] Blocking signals.\n");
        fprintf(logfp, "Blocking signals.\n");
        error = sigprocmask(SIG_BLOCK, &sigbl, NULL);
        if (systemCallFailed("[PARENT]", "SIG_BLOCK failed", error)) {
            fprintf(logfp, "SIG_BLOCK failed.\n");
            return;
        }

        DERROR("[PARENT] Waiting for lock.\n");
        fprintf(logfp, "Waiting for lock.\n");
        /* Lock file so only producer can access. */
        lock.l_type = F_WRLCK;
        error = fcntl(fd, F_SETLKW, &lock);
        if (systemCallFailed("[PARENT]", "F_SETLCKW failed", error)) {
            fprintf(logfp, "F_SETLCKW failed.\n");
            return;
        }
        DERROR("[PARENT] Lock acquired.\n");
        fprintf(logfp, "Lock acquired.\n");

        /* Seek to end of the file to find file size */
        curr = lseek(fd, 0, SEEK_END);

        if (systemCallFailed("[PARENT]", "Seek failed", curr)) {
            fprintf(logfp, "Seek failed.\n");
            return;
        }

        /* Stop if file is filled. */
        if (curr == numc * linec * sizeof(number)) {
            DERROR("[PARENT] File is filled.\n");
            fprintf(logfp, "File is filled.\n");
            f = false;
            DERROR("[PARENT] Releasing lock.\n");
            fprintf(logfp, "Releasing lock.\n");
            /* Breifly unlock the file so consumer can read. */
            lock.l_type = F_UNLCK;
            error = fcntl(fd, F_SETLKW, &lock);
            if (systemCallFailed("[PARENT]", "F_SETLCKW failed", error)) {
                fprintf(logfp, "F_SETLCKW failed.\n");
                return;
            }
            DERROR("[PARENT] Lock released.\n");
            fprintf(logfp, "Lock released.\n");
            DERROR("[PARENT] Suspending execution till: SIGINT | SIGUSR1 | SIGUSR2\n");
            fprintf(logfp, "Suspending execution till: SIGINT | SIGUSR1 | SIGUSR2\n");
            error = sigsuspend(&sigsp);
            if (systemCallFailed("[PARENT]", "sigsuspend failed", (error != -1 ? -1 : 0))) {
                fprintf(logfp, "sigsuspend failed.\n");
                return;
            }
            DERROR("[PARENT] Signal: %s\n",
                   (reason == R_SIGINT ? "SIGINT" : (reason == R_SIGUSR1 ? "SIGUSR1" : "SIGUSR2")));
            fprintf(logfp, "Signal: %s\n",
                    (reason == R_SIGINT ? "SIGINT" : (reason == R_SIGUSR1 ? "SIGUSR1" : "SIGUSR2")));
        } else {
            fprintf(logfp, "I'm producing a random sequence for line %3lu:", curr / numc / sizeof(number) + 1);
            fprintf(stdout, "I'm producing a random sequence for line %3lu:", curr / numc / sizeof(number) + 1);
            for (int i = 0; i < numc; ++i) {
                number = ((double) (rand() % 100 + 1)) + ((double) (rand() % 100)) / 100;
                DERROR("[PARENT] Number generated: %6.2f\n", number);
                fprintf(logfp, " %6.2f", number);
                fprintf(stdout, " %6.2f", number);
                error = writeAll(fd, &number, sizeof(number));
                if (error) {
                    perror("[PARENT] write failed");
                    return;
                }
            }
            fprintf(logfp, "\n");
            fprintf(stdout, "\n");
            fflush(stdout);
            kill(child, SIGUSR1);
        }

        if (f) {
            DERROR("[PARENT] Releasing lock.\n");
            fprintf(logfp, "Releasing lock.\n");
            /* Breifly unlock the file so consumer can read. */
            lock.l_type = F_UNLCK;
            error = fcntl(fd, F_SETLKW, &lock);
            if (systemCallFailed("[PARENT]", "F_SETLCKW failed", error)) {
                fprintf(logfp, "F_SETLCKW failed.\n");
                return;
            }
            DERROR("[PARENT] Lock released.\n");
            fprintf(logfp, "Lock released.\n");
        }
        f = true;

        /* Unblock signals so we can see if something important happened. */
        DERROR("[PARENT] Unblocking signals.\n");
        fprintf(logfp, "Unblocking signals.\n");
        error = sigprocmask(SIG_UNBLOCK, &sigbl, NULL);
        if (systemCallFailed("[PARENT]", "SIG_UNBLOCK failed", error)) {
            fprintf(logfp, "SIG_UNBLOCK failed.\n");
            return;
        }
    }
}

void consumer(int const numc, int const fd, pid_t const parent) {
    struct flock lock;
    off_t curr = 0;
    int error;
    sigset_t sigsp;
    sigset_t sigbl;
    double *numbers __attribute__((__cleanup__(cleanDouble))) = malloc(sizeof(*numbers) * numc);
    double real, imag;

    FILE *logfp __attribute__((__cleanup__(cleanFP))) = fopen("consumer.log", "w");

    if (logfp == NULL) {
        perror("[OPEN]");
        teardown("[CHILD]");
        return;
    }

    /* Initialize the sigsuspend sigset */
    error = sigfillset(&sigsp);
    if (systemCallFailed("[CHILD]", "sigfillset failed", error)) {
        fprintf(logfp, "sigfillset failed.\n");
        return;
    }

    error = sigdelset(&sigsp, SIGINT);
    if (systemCallFailed("[CHILD]", "sigdelset failed(SIGINT)", error)) {
        fprintf(logfp, "sigdelset failed(SIGINT)\n");
        return;
    }

    error = sigdelset(&sigsp, SIGUSR1);
    if (systemCallFailed("[CHILD]", "sigdelset failed(SIGUSR1)", error)) {
        fprintf(logfp, "sigdelset failed(SIGUSR1)\n");
        return;
    }

    error = sigdelset(&sigsp, SIGUSR2);
    if (systemCallFailed("[CHILD]", "sigdelset failed(SIGUSR2)", error)) {
        fprintf(logfp, "sigdelset failed(SIGUSR2)\n");
        return;
    }

    /* Initialize the sigprocmask sigset */
    error = sigemptyset(&sigbl);
    if (error == -1) {
        fprintf(logfp, "[CHILD] sigemptyset failed");
        return;
    }

    error = sigaddset(&sigbl, SIGINT);
    if (error == -1) {
        fprintf(logfp, "[CHILD] sigaddset failed(SIGINT)");
        return;
    }

    error = sigaddset(&sigbl, SIGUSR2);
    if (error == -1) {
        fprintf(logfp, "[CHILD] sigaddset failed(SIGUSR2)");
        return;
    }

    memset(&lock, 0, sizeof(lock));

    int f = true;
    /* Infinite Loop */
    while (loop) {
        /* Block the SIGINT & SIGUSR2 while in critical section. */
        DERROR("[CHILD] Blocking signals.\n");
        fprintf(logfp, "Blocking signals.\n");
        error = sigprocmask(SIG_BLOCK, &sigbl, NULL);
        if (systemCallFailed("[CHILD]", "SIG_BLOCK failed", error)) {
            fprintf(logfp, "SIG_BLOCK failed.\n");
            return;
        }

        DERROR("[CHILD] Waiting for lock.\n");
        fprintf(logfp, "Waiting for lock.\n");
        /* Lock file so only consumer can access. */
        lock.l_type = F_WRLCK;
        error = fcntl(fd, F_SETLKW, &lock);
        if (systemCallFailed("[CHILD]", "F_SETLCKW failed", error)) {
            fprintf(logfp, "F_SETLCKW failed.\n");
            return;
        }
        DERROR("[CHILD] Lock acquired.\n");
        fprintf(logfp, "Lock acquired.\n");

        /* Seek to end of the file to find file size */
        curr = lseek(fd, 0, SEEK_END);

        if (systemCallFailed("[CHILD]", "Seek failed", curr)) {
            fprintf(logfp, "Seek failed.\n");
            return;
        }

        /* Stop if file is filled. */
        if (curr == 0) {
            DERROR("[CHILD] File is empty.\n");
            fprintf(logfp, "File is empty.\n");
            f = false;
            DERROR("[CHILD] Releasing lock.\n");
            fprintf(logfp, "Releasing lock.\n");
            /* Breifly unlock the file so producer can write. */
            lock.l_type = F_UNLCK;
            error = fcntl(fd, F_SETLKW, &lock);
            if (systemCallFailed("[CHILD]", "F_SETLCKW failed", error)) {
                fprintf(logfp, "F_SETLCKW failed.\n");
                return;
            }
            DERROR("[CHILD] Lock released.\n");
            fprintf(logfp, "Lock released.\n");
            error = sigsuspend(&sigsp);
            if (systemCallFailed("[CHILD]", "sigsuspend failed", (error != -1 ? -1 : 0))) {
                fprintf(logfp, "sigsuspend failed.\n");
                return;
            }
            DERROR("[CHILD] Signal: %s\n",
                   (reason == R_SIGINT ? "SIGINT" : (reason == R_SIGUSR1 ? "SIGUSR1" : "SIGUSR2")));
            fprintf(logfp, "Signal: %s\n",
                    (reason == R_SIGINT ? "SIGINT" : (reason == R_SIGUSR1 ? "SIGUSR1" : "SIGUSR2")));
        } else {
            /* Seek to end of the file to find file size */
            curr = lseek(fd, -numc * sizeof(*numbers), SEEK_END);

            if (systemCallFailed("[CHILD]", "Seek failed", curr)) {
                fprintf(logfp, "Seek failed.\n");
                return;
            }

            fprintf(logfp, "The dft of line %3lu:\n", curr / numc / sizeof(*numbers) + 1);
            fprintf(stdout, "The dft of line %3lu:\n", curr / numc / sizeof(*numbers) + 1);

            error = readAll(fd, numbers, sizeof(*numbers) * numc);
            if (error) {
                perror("[CHILD] readall failed");
                teardown("[CHILD]");
                fprintf(logfp, "Readall failed.\n");
                return;
            }

            for (int i = 0; i < numc; ++i) {
                dft(numbers, numc, i, &real, &imag);
                DERROR("[CHILD] DFT(%6.2f) = %6.2f + i * %6.2f\n", numbers[i], real, imag);
                fprintf(logfp, "    DFT(%6.2f) = %6.2f + i * %6.2f\n", numbers[i], real, imag);
                fprintf(stdout, "    DFT(%6.2f) = %6.2f + i * %6.2f\n", numbers[i], real, imag);
            }
            fflush(stdout);
            ftruncate(fd, curr);
            kill(parent, SIGUSR1);
        }

        if (f) {
            DERROR("[CHILD] Releasing lock.\n");
            fprintf(logfp, "Releasing lock.\n");
            /* Breifly unlock the file so consumer can read. */
            lock.l_type = F_UNLCK;
            error = fcntl(fd, F_SETLKW, &lock);
            if (systemCallFailed("[CHILD]", "F_SETLCKW failed", error)) {
                fprintf(logfp, "F_SETLCKW failed.\n");
                return;
            }
            DERROR("[CHILD] Lock released.\n");
            fprintf(logfp, "Lock released.\n");
        }
        f = true;

        /* Unblock signals so we can see if something important happened. */
        DERROR("[CHILD] Unblocking signals.\n");
        fprintf(logfp, "Unblocking signals.\n");
        error = sigprocmask(SIG_UNBLOCK, &sigbl, NULL);
        if (systemCallFailed("[CHILD]", "SIG_UNBLOCK failed", error)) {
            fprintf(logfp, "SIG_UNBLOCK failed.\n");
            return;
        }
    }
}

void signalHandler(int const sig) {
    switch (sig) {
        case SIGINT:
            DERROR("[%d] SIGINT\n", getpid());
            loop = false;
            reason = R_SIGINT;
            break;
        case SIGUSR1:
            /* Just knowing it came is ok. */
            DERROR("[%d] SIGUSR1\n", getpid());
            if (!(reason < R_SIGUSR1))
                reason = R_SIGUSR1;
            break;
        case SIGUSR2:
            DERROR("[%d] SIGUSR2\n", getpid());
            loop = false;
            if (!(reason < R_SIGUSR2))
                reason = R_SIGUSR2;
            break;
    }
}

void teardown(char *const source) {
    /* Ignore SIGUSR2 and use it to kill child processes. */
    struct sigaction dfl;
    dfl.sa_handler = SIG_IGN;
    dfl.sa_flags = SA_RESTART;
    int error = sigemptyset(&dfl.sa_mask);

    if (error == -1) {
        DERROR("%s sigemptyset failed.\n", source);
        perror("sigemptyset failed");
        kill(0, SIGINT);
        return;
    }

    error = sigaction(SIGUSR2, &dfl, NULL);

    if (error == -1) {
        DERROR("%s sigaction failed.\n", source);
        perror("sigaction failed");
        kill(0, SIGINT);
        return;
    }

    DERROR("[%d] Generating SIGUSR2.\n", getpid());
    kill(0, SIGUSR2); /* Tell the child processes to exit. */
}

bool systemCallFailed(char *const source, char *const msg, int error) {
    /* Handle if the lseek fails. */
    if (error == -1) {
        DERROR("%s %s.\n", source, msg);
        perror(msg);
        teardown(source);
        return 1;
    }
    return 0;
}

bool readAll(int fd, void *buffer, size_t size) {
    size_t total = 0;
    int n;

    do {
        n = read(fd, (uint8_t *) buffer + total, size - total);

        if (n <= 0) {
            return true;
        }

        total += n;
    } while (total != size);

    return false;
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

void dft(double *numbers, size_t N, size_t k, double *real, double *imag) {
    *real = 0;
    *imag = 0;
    for (int n = 0; n < N; ++n) {
        *real += numbers[n] * cos(k * n * 2 * M_PI / N);
        *imag -= numbers[n] * sin(k * n * 2 * M_PI / N);
    }
}

void cleanFP(FILE **fpp){
    fclose(*fpp);
    *fpp = NULL;
}

void cleanDouble(double **dpp){
    free(*dpp);
    *dpp = NULL;
}