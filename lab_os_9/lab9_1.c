#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

#define BUF_SIZE 128

char buffer[BUF_SIZE];
sem_t sem;

void* writer_thread(void* arg) {
    (void)arg;
    int counter = 1;

    while (1) {
        sem_wait(&sem);

        snprintf(buffer, BUF_SIZE, "Запись номер %d", counter++);
        sem_post(&sem);

        sleep(1);
    }
    return NULL;
}

void* reader_thread(void* arg) {
    (void)arg;
    pthread_t tid = pthread_self();

    while (1) {
        sem_wait(&sem);

        printf("[Читатель tid=%lu] buffer = \"%s\"\n",
               (unsigned long)tid, buffer);

        sem_post(&sem);

        sleep(1);
    }
    return NULL;
}

int main() {
    pthread_t writer, reader;

    sem_init(&sem, 0, 1);
    strcpy(buffer, "Инициализация");

    pthread_create(&writer, NULL, writer_thread, NULL);
    pthread_create(&reader, NULL, reader_thread, NULL);

    pthread_join(writer, NULL);
    pthread_join(reader, NULL);

    sem_destroy(&sem);
    return 0;
}
