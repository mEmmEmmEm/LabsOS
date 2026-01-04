#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define NUM_READERS 10
#define BUFFER_SIZE 128

static char shared_array[BUFFER_SIZE] = "Empty";

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv_readers = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cv_writer = PTHREAD_COND_INITIALIZER;

static int generation = 0;
static int readers_seen = 0;
static int data_ready = 0;
static int keep_running = 1;

static void* writer_thread(void* arg) {
    (void)arg;

    while (1) {
        pthread_mutex_lock(&mtx);

        while (keep_running && data_ready) {
            pthread_cond_wait(&cv_writer, &mtx);
        }

        if (!keep_running) {
            pthread_mutex_unlock(&mtx);
            break;
        }

        generation++;
        readers_seen = 0;
        data_ready = 1;

        snprintf(shared_array, BUFFER_SIZE, "Record #%d", generation);

        pthread_cond_broadcast(&cv_readers);
        pthread_mutex_unlock(&mtx);

        printf("[WRITER] wrote: %s\n", shared_array);
        sleep(1);
    }

    return NULL;
}

static void* reader_thread(void* arg) {
    long idx = (long)arg;
    int last_seen = 0;

    while (1) {
        char local[BUFFER_SIZE];

        pthread_mutex_lock(&mtx);

        while (keep_running && (!data_ready || last_seen == generation)) {
            pthread_cond_wait(&cv_readers, &mtx);
        }

        if (!keep_running) {
            pthread_mutex_unlock(&mtx);
            break;
        }

        strncpy(local, shared_array, BUFFER_SIZE);
        local[BUFFER_SIZE - 1] = '\0';

        last_seen = generation;
        readers_seen++;

        if (readers_seen == NUM_READERS) {
            data_ready = 0;
            pthread_cond_signal(&cv_writer);
        }

        pthread_mutex_unlock(&mtx);

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

    pthread_mutex_lock(&mtx);
    keep_running = 0;
    pthread_cond_broadcast(&cv_readers);
    pthread_cond_broadcast(&cv_writer);
    pthread_mutex_unlock(&mtx);

    pthread_join(writer, NULL);
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    pthread_mutex_destroy(&mtx);
    pthread_cond_destroy(&cv_readers);
    pthread_cond_destroy(&cv_writer);

    return 0;
}
