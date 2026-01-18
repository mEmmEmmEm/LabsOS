#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SHM_KEY   0x1234
#define SEM_KEY   0x5678
#define SHM_SIZE  256

#define SEM_MUTEX 0

static volatile sig_atomic_t g_stop = 0;
static char* g_shm = (char*)-1;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void mutex_lock(int semid) {
    struct sembuf op = { SEM_MUTEX, -1, 0 };
    if (semop(semid, &op, 1) == -1) die("semop(lock)");
}

static void mutex_unlock(int semid) {
    struct sembuf op = { SEM_MUTEX, 1, 0 };
    if (semop(semid, &op, 1) == -1) die("semop(unlock)");
}

int main(void) {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    int shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
    if (shmid == -1) die("shmget");

    int semid = semget(SEM_KEY, 2, 0666);
    if (semid == -1) die("semget");

    g_shm = (char*)shmat(shmid, NULL, 0);
    if (g_shm == (char*)-1) die("shmat");

    while (!g_stop) {
        time_t now = time(NULL);
        char* ts = ctime(&now);

        mutex_lock(semid);
        printf("Приёмник pid=%d, время=%sПринято: %s\n",
               getpid(),
               ts ? ts : "ctime_error\n",
               g_shm);
        mutex_unlock(semid);

        sleep(3);
    }

    shmdt(g_shm);
    g_shm = (char*)-1;
    return 0;
}
