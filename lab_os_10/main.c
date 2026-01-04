#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define NUM_READERS 10
#define BUFFER_SIZE 128

static char shared_array[BUFFER_SIZE] = "Empty";
static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
static volatile int keep_running = 1;
static int record_counter = 0;

static void* writer_thread(void* arg) {
    (void)arg;
    char local[BUFFER_SIZE];

    while (keep_running) {
        pthread_rwlock_wrlock(&rwlock);
        record_counter++;
        snprintf(shared_array, BUFFER_SIZE, "Record #%d", record_counter);
        strncpy(local, shared_array, BUFFER_SIZE);
        local[BUFFER_SIZE - 1] = '\0';
        pthread_rwlock_unlock(&rwlock);

        printf("[WRITER] wrote: %s\n", local);
        sleep(1);
    }
    return NULL;
}

static void* reader_thread(void* arg) {
    long idx = (long)arg;
    char local[BUFFER_SIZE];

    while (keep_running) {
        pthread_rwlock_rdlock(&rwlock);
        strncpy(local, shared_array, BUFFER_SIZE);
        local[BUFFER_SIZE - 1] = '\0';
        pthread_rwlock_unlock(&rwlock);

        printf("[READER %ld tid=%lu] shared_array=\"%s\"\n",
               idx, (unsigned long)pthread_self(), local);

        usleep(200000);
    }
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    pthread_t writer;
    pthread_t readers[NUM_READERS];

    pthread_create(&writer, NULL, writer_thread, NULL);
    for (long i = 0; i < NUM_READERS; i++) {
        pthread_create(&readers[i], NULL, reader_thread, (void*)i);
    }

    printf("Press ENTER to stop...\n");
    getchar();
    keep_running = 0;

    pthread_join(writer, NULL);
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    pthread_rwlock_destroy(&rwlock);
    return 0;
}
