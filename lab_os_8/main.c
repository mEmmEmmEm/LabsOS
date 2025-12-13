#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

#define READER_COUNT 10
#define DATA_SIZE 256
#define WAIT_TIME 1500000

char common_data[DATA_SIZE];
int operation_number = 0;
pthread_mutex_t data_lock;

void* data_writer(void* param) {
    (void)param;

    while (1) {
        usleep(WAIT_TIME);

        pthread_mutex_lock(&data_lock);

        operation_number++;
        snprintf(common_data, DATA_SIZE, "Данные #%d", operation_number);

        printf("\033[0;31m[ПИСАТЕЛЬ]\033[0m Записано в массив: %s\n", common_data);

        pthread_mutex_unlock(&data_lock);
    }
    return 0;
}

void* data_reader(void* param) {
    long reader_num = (long)(intptr_t)param;
    pthread_t thread_id = pthread_self();

    while (1) {
        usleep(WAIT_TIME);

        pthread_mutex_lock(&data_lock);

        if (operation_number == 0) {
            printf("\033[0;36m[ЧИТАТЕЛЬ %ld]\033[0m (id: %lu) Массив пустой\n",
                   reader_num, (unsigned long)thread_id);
        } else {
            printf("\033[0;36m[ЧИТАТЕЛЬ %02ld]\033[0m (id: %lu) Содержимое: %s\n",
                   reader_num, (unsigned long)thread_id, common_data);
        }

        pthread_mutex_unlock(&data_lock);
    }
    return 0;
}

int execute_program() {
    pthread_t writer_thread;
    pthread_t reader_threads[READER_COUNT];
    int counter;

    if (pthread_mutex_init(&data_lock, NULL) != 0) {
        perror("Не удалось создать мьютекс");
        return 1;
    }

    strcpy(common_data, "Нет информации");

    printf("=== Программа запущена ===\n");
    printf("=== Потоков для чтения: %d ===\n", READER_COUNT);
    printf("=== Задержка между операциями: 1.5 секунды ===\n\n");

    if (pthread_create(&writer_thread, NULL, data_writer, NULL) != 0) {
        perror("Не удалось создать поток писателя");
        return 1;
    }

    for (counter = 0; counter < READER_COUNT; counter++) {
        if (pthread_create(&reader_threads[counter], NULL, data_reader,
                          (void*)(intptr_t)(counter + 1)) != 0) {
            perror("Не удалось создать поток читателя");
            return 1;
        }
    }

    pthread_join(writer_thread, NULL);
    for (counter = 0; counter < READER_COUNT; counter++) {
        pthread_join(reader_threads[counter], NULL);
    }

    pthread_mutex_destroy(&data_lock);
    return 0;
}

int main() {
    return execute_program();
}