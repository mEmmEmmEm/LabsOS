#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define BUF_SIZE 512

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void format_time_iso(time_t t, char *buf, size_t len) {
    struct tm tm_val;
    if (localtime_r(&t, &tm_val) == NULL) {
        snprintf(buf, len, "unknown");
        return;
    }
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tm_val);
}


static void run_pipe_demo(void) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        die("pipe");
    }

    pid_t pid = fork();
    if (pid < 0) {
        die("fork");
    }

    if (pid == 0) {
        close(pipefd[1]); 

        char buf[BUF_SIZE];
        ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
        if (n < 0) {
            perror("child: read");
            close(pipefd[0]);
            _exit(EXIT_FAILURE);
        }
        if (n == 0) {
            fprintf(stderr, "child: no data received on pipe\n");
            close(pipefd[0]);
            _exit(EXIT_FAILURE);
        }
        buf[n] = '\0';
        close(pipefd[0]);
        sleep(6);

        time_t now = time(NULL);
        char time_str[64];
        format_time_iso(now, time_str, sizeof(time_str));

        printf("=== PIPE DEMO (child) ===\n");
        printf("Child PID: %d\n", (int)getpid());
        printf("Child current time: %s\n", time_str);
        printf("Message from parent: %s\n", buf);
        fflush(stdout);

        _exit(EXIT_SUCCESS);
    } else {
        close(pipefd[0]); 

        time_t now = time(NULL);
        char time_str[64];
        format_time_iso(now, time_str, sizeof(time_str));

        char msg[BUF_SIZE];
        int len = snprintf(
            msg, sizeof(msg),
            "Parent PID: %d; Parent time: %s\n",
            (int)getpid(), time_str
        );
        if (len < 0 || (size_t)len >= sizeof(msg)) {
            fprintf(stderr, "parent: message formatting error\n");
            close(pipefd[1]);
            waitpid(pid, NULL, 0);
            exit(EXIT_FAILURE);
        }

        ssize_t written = write(pipefd[1], msg, (size_t)len);
        if (written < 0) {
            perror("parent: write");
            close(pipefd[1]);
            waitpid(pid, NULL, 0);
            exit(EXIT_FAILURE);
        }
        close(pipefd[1]);

        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
           
        } else {
            fprintf(stderr, "child terminated abnormally\n");
        }
    }
}


static const char *fifo_path(void) {
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/lab6_fifo_%d", (int)getuid());
    return path;
}

static void run_fifo_writer(void) {
    const char *path = fifo_path();
    if (mkfifo(path, 0666) == -1) {
        if (errno != EEXIST) {
            die("mkfifo (writer)");
        }
    }

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        die("open fifo for writing");
    }

    time_t now = time(NULL);
    char time_str[64];
    format_time_iso(now, time_str, sizeof(time_str));

    char msg[BUF_SIZE];
    int len = snprintf(
        msg, sizeof(msg),
        "Writer PID: %d; Writer time: %s\n",
        (int)getpid(), time_str
    );
    if (len < 0 || (size_t)len >= sizeof(msg)) {
        fprintf(stderr, "fifo-writer: message formatting error\n");
        close(fd);
        exit(EXIT_FAILURE);
    }
    ssize_t written = write(fd, msg, (size_t)len);
    if (written < 0) {
        perror("fifo-writer: write");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);

    printf("fifo-writer: message sent and exiting\n");
}

static void run_fifo_reader(void) {
    const char *path = fifo_path();
    if (mkfifo(path, 0666) == -1) {
        if (errno != EEXIST) {
            die("mkfifo (reader)");
        }
    }
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        die("open fifo for reading");
    }
    char buf[BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("fifo-reader: read");
        close(fd);
        exit(EXIT_FAILURE);
    }
    if (n == 0) {
        fprintf(stderr, "fifo-reader: no data received\n");
        close(fd);
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';
    close(fd);

    sleep(12);

    time_t now = time(NULL);
    char time_str[64];
    format_time_iso(now, time_str, sizeof(time_str));

    printf("=== FIFO DEMO (reader) ===\n");
    printf("Reader PID: %d\n", (int)getpid());
    printf("Reader current time: %s\n", time_str);
    printf("Message from writer: %s\n", buf);
    fflush(stdout);
    if (unlink(path) == -1) {
        if (errno != ENOENT) {
            perror("unlink fifo");
        }
    }
}

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s pipe          # demo with unnamed pipe + fork\n"
            "  %s fifo-writer   # fifo writer process\n"
            "  %s fifo-reader   # fifo reader process\n",
            prog, prog, prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "pipe") == 0) {
        run_pipe_demo();
    } else if (strcmp(argv[1], "fifo-writer") == 0) {
        run_fifo_writer();
    } else if (strcmp(argv[1], "fifo-reader") == 0) {
        run_fifo_reader();
    } else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
