#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SHM_KEY   0x1234
#define SEM_KEY   0x5678
#define SHM_SIZE  256

#define SEM_MUTEX 0
#define SEM_SLOCK 1

static volatile sig_atomic_t g_stop = 0;

static int g_shmid = -1;
static int g_semid = -1;
static char* g_shm = (char*)-1;
static int g_creator = 0;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
#if defined(__linux__)
    struct seminfo *__buf;
#endif
};

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void sem_op(int semid, unsigned short semnum, short delta, int flags) {
    struct sembuf op;
    op.sem_num = semnum;
    op.sem_op  = delta;
    op.sem_flg = flags;
    if (semop(semid, &op, 1) == -1) die("semop");
}

static void mutex_lock(int semid)   { sem_op(semid, SEM_MUTEX, -1, 0); }
static void mutex_unlock(int semid) { sem_op(semid, SEM_MUTEX,  1, 0); }

static int try_take_sender_lock(int semid) {
    struct sembuf op = { SEM_SLOCK, -1, IPC_NOWAIT | SEM_UNDO };
    if (semop(semid, &op, 1) == -1) {
        if (errno == EAGAIN) return 0; 
        die("semop(sender_lock)");
    }
    return 1;
}

static void release_sender_lock(int semid) {
    sem_op(semid, SEM_SLOCK, 1, SEM_UNDO);
}

static void cleanup(void) {
    if (g_shm != (char*)-1) {
        shmdt(g_shm);
        g_shm = (char*)-1;
    }

    if (g_creator) {
        if (g_shmid != -1) shmctl(g_shmid, IPC_RMID, NULL);
        if (g_semid != -1) semctl(g_semid, 0, IPC_RMID);
    }
}

int main(void) {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    g_shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (g_shmid == -1) die("shmget");

    g_semid = semget(SEM_KEY, 2, IPC_CREAT | IPC_EXCL | 0666);
    if (g_semid == -1) {
        if (errno != EEXIST) die("semget(create)");
        g_semid = semget(SEM_KEY, 2, 0666);
        if (g_semid == -1) die("semget(open)");
        g_creator = 0;
    } else {
        g_creator = 1;

        union semun arg;
        unsigned short vals[2] = {1, 1};
        arg.array = vals;
        if (semctl(g_semid, 0, SETALL, arg) == -1) die("semctl(SETALL)");
    }

    g_shm = (char*)shmat(g_shmid, NULL, 0);
    if (g_shm == (char*)-1) die("shmat");

    if (!try_take_sender_lock(g_semid)) {
        fprintf(stderr, "Sender уже запущен (нельзя запускать два sender одновременно).\n");
        cleanup();
        return EXIT_FAILURE;
    }
    mutex_lock(g_semid);
    snprintf(g_shm, SHM_SIZE, "Sender pid=%d запущен.\n", getpid());
    mutex_unlock(g_semid);

    while (!g_stop) {
        time_t now = time(NULL);
        char* ts = ctime(&now);

        mutex_lock(g_semid);
        snprintf(g_shm, SHM_SIZE,
                 "Отправитель pid=%d, время=%s",
                 getpid(), ts ? ts : "ctime_error\n");
        mutex_unlock(g_semid);

        sleep(3);
    }

    release_sender_lock(g_semid);
    cleanup();
    return 0;
}
