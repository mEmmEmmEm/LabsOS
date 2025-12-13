#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SHM_KEY  0x1234
#define SEM_KEY  0x5678
#define SHM_SIZE 256

void sem_lock(int semid) {
    struct sembuf op = {0, -1, 0};
    semop(semid, &op, 1);
}

void sem_unlock(int semid) {
    struct sembuf op = {0, 1, 0};
    semop(semid, &op, 1);
}

int main() {
    int shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    int semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);

    semctl(semid, 0, SETVAL, 1);

    char* shm = (char*)shmat(shmid, NULL, 0);

    while (1) {
        time_t now = time(NULL);

        sem_lock(semid);
        snprintf(shm, SHM_SIZE,
                 "Отправитель pid=%d, время=%s",
                 getpid(), ctime(&now));
        sem_unlock(semid);

        sleep(3);
    }
}
