#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define NUM_READERS 10
#define BUFFER_SIZE 128

static char shared_array[BUFFER_SIZE] = "Empty";

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv  = PTHREAD_COND_INITIALIZER;

static int generation = 0;
static int keep_running = 1;

static void* writer_thread(void* arg) {
    (void)arg;
    int counter = 0;

    while (1) {
        pthread_mutex_lock(&mtx);

        if (!keep_running) {
            pthread_mutex_unlock(&mtx);
            break;
        }

        counter++;
        generation++;
        snprintf(shared_array, BUFFER_SIZE, "Record #%d", counter);
        pthread_cond_broadcast(&cv);

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
        while (keep_running && last_seen == generation) {
            pthread_cond_wait(&cv, &mtx);
        }

        if (!keep_running) {
            pthread_mutex_unlock(&mtx);
            break;
        }
        strncpy(local, shared_array, BUFFER_SIZE);
        local[BUFFER_SIZE - 1] = '\0';
        last_seen = generation;

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
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);

    pthread_join(writer, NULL);
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    return 0;
}
