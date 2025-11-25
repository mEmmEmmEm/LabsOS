#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <utime.h>
#include <getopt.h>
#include <errno.h>

#define NAME_LIMIT 256
#define COPY_BUF 4096
#define MAX_EXTRACT_SIZE (1024LL * 1024LL * 1024LL)

struct fileInput {
    char  name[NAME_LIMIT];
    mode_t mode;
    uid_t uid;
    gid_t gid;
    off_t size;
    time_t atime;
    time_t mtime;
    char deleted;
};

static void print_usage(const char *prog) {
    printf("Usage: %s <archive> [option] [file]\n", prog);
    printf("Options:\n");
    printf("  -i, --input <file>    Add file to archive\n");
    printf("  -e, --extract <file>  Extract file from archive (and remove it)\n");
    printf("  -s, --stat            Show archive contents\n");
    printf("  -h, --help            Show this help message\n");
}

static int full_write(int fd, const void *buf, size_t len, const char *msg) {
    const char *p = buf;
    size_t left = len;

    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            perror(msg);
            return -1;
        }
        if (w == 0) {
            fprintf(stderr, "%s: wrote 0 bytes\n", msg);
            return -1;
        }
        p += w;
        left -= (size_t)w;
    }
    return 0;
}

static int skip_bytes(int fd, off_t size) {
    char buf[COPY_BUF];

    if (size <= 0) return 0;

    if (lseek(fd, size, SEEK_CUR) != (off_t)-1) {
        return 0;
    }

    off_t left = size;
    while (left > 0) {
        ssize_t chunk = (left > (off_t)sizeof(buf)) ? sizeof(buf) : left;
        ssize_t r = read(fd, buf, chunk);
        if (r < 0) {
            perror("skip_bytes: read");
            return -1;
        }
        if (r == 0) {
            fprintf(stderr, "skip_bytes: unexpected EOF\n");
            return -1;
        }
        left -= r;
    }
    return 0;
}

static int cmd_add(const char *archive_name, const char *file_name) {
    int arch_fd = open(archive_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (arch_fd < 0) {
        perror("Failed to open/create archive");
        return -1;
    }

    int in_fd = open(file_name, O_RDONLY);
    if (in_fd < 0) {
        perror("Failed to open input file");
        close(arch_fd);
        return -1;
    }

    struct stat st;
    if (fstat(in_fd, &st) < 0) {
        perror("Failed to stat input file");
        close(in_fd);
        close(arch_fd);
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a regular file\n", file_name);
        close(in_fd);
        close(arch_fd);
        return -1;
    }
    struct fileInput hdr = {0};
    if (strlen(file_name) >= NAME_LIMIT) {
        fprintf(stderr, "Error: file name '%s' is too long\n", file_name);
        close(in_fd);
        close(arch_fd);
        return -1;
    }
    strncpy(hdr.name, file_name, NAME_LIMIT - 1);
    hdr.mode = st.st_mode;
    hdr.uid = st.st_uid;
    hdr.gid = st.st_gid;
    hdr.size = st.st_size;
    hdr.atime = st.st_atime;
    hdr.mtime = st.st_mtime;
    hdr.deleted = 0;
    if (full_write(arch_fd, &hdr, sizeof(hdr), "Failed to write header") < 0) {
        close(in_fd);
        close(arch_fd);
        return -1;
    }

    char buf[COPY_BUF];
    ssize_t r;
    while ((r = read(in_fd, buf, sizeof(buf))) > 0) {
        if (full_write(arch_fd, buf, r, "Failed to write data") < 0) {
            close(in_fd);
            close(arch_fd);
            return -1;
        }
    }

    if (r < 0) {
        perror("Failed to read input file");
        close(in_fd);
        close(arch_fd);
        return -1;
    }

    printf("File '%s' added to archive '%s'.\n", file_name, archive_name);

    close(in_fd);
    close(arch_fd);
    return 0;
}
static int cmd_compact(const char *archive_name) {
    int in_fd = open(archive_name, O_RDONLY);
    if (in_fd < 0) {
        perror("compact: cannot open archive");
        return -1;
    }

    char tmp_name[512];
    snprintf(tmp_name, sizeof(tmp_name), "%s.tmpXXXXXX", archive_name);

    int tmp_fd = mkstemp(tmp_name);
    if (tmp_fd < 0) {
        perror("compact: cannot create temp file");
        close(in_fd);
        return -1;
    }

    struct stat st;
    if (fstat(in_fd, &st) == 0) fchmod(tmp_fd, st.st_mode);

    struct fileInput hdr;
    char buf[COPY_BUF];

    while (1) {
        ssize_t r = read(in_fd, &hdr, sizeof(hdr));
        if (r == 0) break;
        if (r < 0) {
            perror("compact: read error");
            goto fail;
        }
        if (r != sizeof(hdr)) {
            fprintf(stderr, "compact: partial header — broken archive\n");
            goto fail;
        }

        if (hdr.deleted) {
            if (skip_bytes(in_fd, hdr.size) < 0) goto fail;
            continue;
        }

        if (full_write(tmp_fd, &hdr, sizeof(hdr), "compact: write header") < 0)
            goto fail;

        off_t left = hdr.size;
        while (left > 0) {
            ssize_t want = left > sizeof(buf) ? sizeof(buf) : left;
            ssize_t br = read(in_fd, buf, want);
            if (br <= 0) {
                perror("compact: read data error");
                goto fail;
            }
            if (full_write(tmp_fd, buf, br, "compact: write data") < 0)
                goto fail;
            left -= br;
        }
    }
    fsync(tmp_fd);
    close(tmp_fd);
    close(in_fd);

    if (rename(tmp_name, archive_name) < 0) {
        perror("compact: rename failed");
        unlink(tmp_name);
        return -1;
    }
    return 0;

fail:
    close(tmp_fd);
    close(in_fd);
    unlink(tmp_name);
    return -1;
}

static int cmd_extract(const char *archive_name, const char *file_name) {
    int arch_fd = open(archive_name, O_RDWR);
    if (arch_fd < 0) {
        perror("Failed to open archive");
        return -1;
    }
    struct fileInput hdr;
    int found = 0;

    while (1) {
        off_t hdr_pos = lseek(arch_fd, 0, SEEK_CUR);
        if (hdr_pos == (off_t)-1) {
            perror("lseek failed");
            close(arch_fd);
            return -1;
        }

        ssize_t r = read(arch_fd, &hdr, sizeof(hdr));
        if (r == 0) break;
        if (r < 0) {
            perror("Failed to read header");
            close(arch_fd);
            return -1;
        }
        if (r != sizeof(hdr)) {
            fprintf(stderr, "Broken archive: partial header\n");
            close(arch_fd);
            return -1;
        }

        if (!hdr.deleted && strcmp(hdr.name, file_name) == 0) {
            found = 1;

            if (hdr.size > MAX_EXTRACT_SIZE) {
                fprintf(stderr, "File too large to extract: %lld bytes\n",
                        (long long)hdr.size);
                skip_bytes(arch_fd, hdr.size);
                break;
            }

            int out_fd = open(hdr.name, O_WRONLY | O_CREAT | O_TRUNC, hdr.mode);
            if (out_fd < 0) {
                perror("Failed to create output file");
                skip_bytes(arch_fd, hdr.size);
                break;
            }
            char buf[COPY_BUF];
            off_t left = hdr.size;
            while (left > 0) {
                ssize_t want = (left > sizeof(buf)) ? sizeof(buf) : left;
                ssize_t br = read(arch_fd, buf, want);
                if (br <= 0) {
                    perror("Error reading archived data");
                    close(out_fd);
                    close(arch_fd);
                    return -1;
                }
                if (full_write(out_fd, buf, br, "Error writing extracted file") < 0) {
                    close(out_fd);
                    close(arch_fd);
                    return -1;
                }
                left -= br;
            }

            close(out_fd);

            chmod(hdr.name, hdr.mode);
            chown(hdr.name, hdr.uid, hdr.gid);
            struct utimbuf times = {hdr.atime, hdr.mtime};
            utime(hdr.name, &times);
            hdr.deleted = 1;
            lseek(arch_fd, hdr_pos, SEEK_SET);
            full_write(arch_fd, &hdr, sizeof(hdr), "Failed to mark deleted");

            close(arch_fd);

            if (cmd_compact(archive_name) < 0) {
                fprintf(stderr, "Warning: archive compaction failed.\n");
            }

            printf("Extracted '%s'.\n", file_name);
            return 0;
        } else {
            if (skip_bytes(arch_fd, hdr.size) < 0) {
                close(arch_fd);
                return -1;
            }
        }
    }

    if (!found) {
        printf("File '%s' not found in archive.\n", file_name);
    }

    close(arch_fd);
    return found ? 0 : 1;
}

static int cmd_stat(const char *archive_name) {
    int arch_fd = open(archive_name, O_RDONLY);
    if (arch_fd < 0) {
        perror("Failed to open archive");
        return -1;
    }
    printf("Archive '%s' contents:\n", archive_name);
    printf("----------------------------------------------\n");
    printf("%-30s %-12s %-20s\n", "File name", "Size (B)", "Modified");
    printf("----------------------------------------------\n");
    struct fileInput hdr;

    while (1) {
        ssize_t r = read(arch_fd, &hdr, sizeof(hdr));
        if (r == 0) break;
        if (r < 0) {
            perror("read error");
            close(arch_fd);
            return -1;
        }
        if (r != sizeof(hdr)) {
            fprintf(stderr, "Broken archive: partial header\n");
            close(arch_fd);
            return -1;
        }

        if (!hdr.deleted) {
            char tbuf[64];
            struct tm *tm = localtime(&hdr.mtime);
            if (tm) strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
            else strcpy(tbuf, "unknown");
            printf("%-30s %-12lld %-20s\n",
                   hdr.name, (long long)hdr.size, tbuf);
        }

        if (skip_bytes(arch_fd, hdr.size) < 0) {
            close(arch_fd);
            return -1;
        }
    }

    close(arch_fd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *archive = argv[1];

    static struct option long_opts[] = {
        {"input", required_argument, 0, 'i'},
        {"extract", required_argument, 0, 'e'},
        {"stat", no_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    optind = 2;
    int opt, idx;

    opt = getopt_long(argc, argv, "i:e:sh", long_opts, &idx);

    switch (opt) {
        case 'i': return cmd_add(archive, optarg);
        case 'e': return cmd_extract(archive, optarg);
        case 's': return cmd_stat(archive);
        case 'h': print_usage(argv[0]); return 0;
        default:  print_usage(argv[0]); return 1;
    }
}
