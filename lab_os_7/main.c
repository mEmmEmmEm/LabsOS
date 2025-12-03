#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SHM_KEY        0x12347ABC   
#define LOCKFILE_PATH  "/tmp/lab7_writer.lock"
#define MSG_LEN        256

struct shared_block {
    pid_t  writer_pid;
    time_t writer_time;
    char   text[MSG_LEN];
};

static int   g_shmid   = -1;
static struct shared_block *g_shm = NULL;
static int   g_lock_fd = -1;
static bool  g_is_writer = false;

static void format_time(time_t t, char *buf, size_t buflen) {
    struct tm tm_val;
    if (localtime_r(&t, &tm_val) == NULL) {
        snprintf(buf, buflen, "unknown");
        return;
    }
    strftime(buf, buflen, "%Y-%m-%d %H:%M:%S", &tm_val);
}

static void clean_up_and_exit(int code);

static void signal_handler(int sig) {
    (void)sig;
    if (g_is_writer) {
        printf("\nWriter: caught signal, cleaning up...\n");
    } else {
        printf("\nReader: caught signal, detaching...\n");
    }
    clean_up_and_exit(0);
}

static void setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}


static int acquire_writer_lock(void) {
    g_lock_fd = open(LOCKFILE_PATH, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (g_lock_fd < 0) {
        if (errno == EEXIST) {
            fprintf(stderr, "Writer: another writer process is already running (lock file exists: %s)\n",
                    LOCKFILE_PATH);
        } else {
            perror("Writer: open lock file");
        }
        return -1;
    }

    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    if (len > 0) {
        if (write(g_lock_fd, buf, (size_t)len) < 0) {
            perror("Writer: cannot write to lock file");
        }
        fsync(g_lock_fd);
    }
    return 0;
}

static int create_or_attach_shm_writer(void) {
    g_shmid = shmget(SHM_KEY, sizeof(struct shared_block), IPC_CREAT | 0666);
    if (g_shmid < 0) {
        perror("Writer: shmget");
        return -1;
    }

    g_shm = shmat(g_shmid, NULL, 0);
    if (g_shm == (void*)-1) {
        perror("Writer: shmat");
        g_shm = NULL;
        return -1;
    }
    g_shm->writer_pid   = getpid();
    g_shm->writer_time  = 0;
    g_shm->text[0]      = '\0';

    return 0;
}
static int attach_shm_reader(void) {
    int attempts = 0;
    while (1) {
        g_shmid = shmget(SHM_KEY, sizeof(struct shared_block), 0666);
        if (g_shmid >= 0) break;

        if (errno == ENOENT) {
            if (attempts == 0) {
                fprintf(stderr, "Reader: shared memory not created yet, waiting for writer...\n");
            }
            attempts++;
            usleep(200000); 
            continue;
        } else {
            perror("Reader: shmget");
            return -1;
        }
    }

    g_shm = shmat(g_shmid, NULL, 0);
    if (g_shm == (void*)-1) {
        perror("Reader: shmat");
        g_shm = NULL;
        return -1;
    }

    return 0;
}

static void clean_up_and_exit(int code) {
    if (g_shm != NULL && g_shm != (void*)-1) {
        shmdt(g_shm);
        g_shm = NULL;
    }

    if (g_is_writer && g_shmid >= 0) {
        if (shmctl(g_shmid, IPC_RMID, NULL) < 0 && errno != EIDRM) {
            perror("Writer: shmctl IPC_RMID");
        } else {
            printf("Writer: shared memory segment removed.\n");
        }
    }

    if (g_lock_fd >= 0) {
        close(g_lock_fd);
        g_lock_fd = -1;
        if (g_is_writer) {
            if (unlink(LOCKFILE_PATH) < 0 && errno != ENOENT) {
                perror("Writer: unlink lock file");
            }
        }
    }

    exit(code);
}

static int run_writer(void) {
    g_is_writer = true;

    if (acquire_writer_lock() != 0) {
        return 1;
    }

    if (create_or_attach_shm_writer() != 0) {
        clean_up_and_exit(2);
    }
    setup_signals();

    printf("Writer started (pid=%d).\n", (int)getpid());
    printf("Shared memory key: 0x%X\n", SHM_KEY);
    printf("Lock file: %s\n", LOCKFILE_PATH);
    printf("Press Ctrl-C to stop writer.\n");
    while (1) {
        time_t now = time(NULL);

        char time_str[64];
        format_time(now, time_str, sizeof(time_str));

        g_shm->writer_pid  = getpid();
        g_shm->writer_time = now;

        char msg[MSG_LEN];
        int written = snprintf(
            msg, sizeof(msg),
            "From writer pid=%d at %s",
            (int)getpid(), time_str
        );
        if (written < 0) {
            strncpy(msg, "format-error", sizeof(msg));
            msg[sizeof(msg) - 1] = '\0';
        }

        strncpy(g_shm->text, msg, MSG_LEN - 1);
        g_shm->text[MSG_LEN - 1] = '\0';

        printf("Writer: updated message: %s\n", g_shm->text);
        fflush(stdout);

        sleep(1);
    }

    return 0;
}
static int run_reader(void) {
    g_is_writer = false;

    if (attach_shm_reader() != 0) {
        return 1;
    }

    setup_signals();

    printf("Reader started (pid=%d).\n", (int)getpid());
    printf("Attached to shared memory key: 0x%X\n", SHM_KEY);
    printf("Press Ctrl-C to stop reader.\n");

    while (1) {
        time_t local_now = time(NULL);

        char local_time_str[64];
        char writer_time_str[64];

        format_time(local_now, local_time_str, sizeof(local_time_str));
        format_time(g_shm->writer_time, writer_time_str, sizeof(writer_time_str));

        printf("Reader (pid=%d) local time: %s\n",
               (int)getpid(), local_time_str);
        printf("  Received from pid=%d at %s:\n",
               (int)g_shm->writer_pid, writer_time_str);
        printf("  \"%s\"\n", g_shm->text);
        fflush(stdout);

        sleep(1);
    }

    return 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s writer   # start single writer process\n"
            "  %s reader   # start reader process (can run multiple times)\n",
            prog, prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "writer") == 0) {
        return run_writer();
    } else if (strcmp(argv[1], "reader") == 0) {
        return run_reader();
    } else {
        print_usage(argv[0]);
        return 1;
    }
}
