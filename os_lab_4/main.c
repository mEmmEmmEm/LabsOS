#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void usage(const char *p) {
    fprintf(stderr,
        "Usage: %s <mode> <file>\n"
        "Examples:\n"
        "  %s +x file.txt\n"
        "  %s u-r file.txt\n"
        "  %s g+rw file.txt\n"
        "  %s ug+rw file.txt\n"
        "  %s uga+rwx file.txt\n"
        "  %s 766 file.txt\n", p, p, p, p, p, p, p);
}

static int is_octal(const char *s) {
    if (!*s) return 0;
    for (const char *p = s; *p; ++p)
        if (*p < '0' || *p > '7') return 0;
    return 1;
}

static int parse_octal(const char *s, mode_t *out) {
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 8);
    if (errno || end == s || *end != '\0' || v < 0 || v > 07777) return -1;
    *out = (mode_t)v;
    return 0;
}

typedef struct { int u,g,o; } Who;

static void apply_rwx(mode_t *m, Who w, char op, int pr, int pw, int px) {
    mode_t add = 0;
    if (pr) { if (w.u) add |= S_IRUSR; if (w.g) add |= S_IRGRP; if (w.o) add |= S_IROTH; }
    if (pw) { if (w.u) add |= S_IWUSR; if (w.g) add |= S_IWGRP; if (w.o) add |= S_IWOTH; }
    if (px) { if (w.u) add |= S_IXUSR; if (w.g) add |= S_IXGRP; if (w.o) add |= S_IXOTH; }

    if (op == '=') {
        if (w.u) *m &= ~(S_IRUSR|S_IWUSR|S_IXUSR);
        if (w.g) *m &= ~(S_IRGRP|S_IWGRP|S_IXGRP);
        if (w.o) *m &= ~(S_IROTH|S_IWOTH|S_IXOTH);
        *m |= add;
    } else if (op == '+') {
        *m |= add;
    } else if (op == '-') {
        *m &= ~add;
    }
}

static int parse_symbolic_and_apply(const char *expr, mode_t *mode) {
    const char *p = expr;

    while (*p) {
        Who w = {0,0,0}; int saw = 0;
        while (*p=='u' || *p=='g' || *p=='o' || *p=='a') {
            saw = 1;
            if (*p=='u') w.u = 1;
            else if (*p=='g') w.g = 1;
            else if (*p=='o') w.o = 1;
            else w.u = w.g = w.o = 1; 
            ++p;
        }
        if (!saw) w.u = w.g = w.o = 1; 
        if (*p!='+' && *p!='-' && *p!='=') {
            fprintf(stderr, "Invalid operator near: %s\n", p);
            return -1;
        }
        char op = *p++;
        if (*p=='\0') {
            if (op=='=') { 
                if (w.u) *mode &= ~(S_IRUSR|S_IWUSR|S_IXUSR);
                if (w.g) *mode &= ~(S_IRGRP|S_IWGRP|S_IXGRP);
                if (w.o) *mode &= ~(S_IROTH|S_IWOTH|S_IXOTH);
                break;
            }
            fprintf(stderr, "Missing permissions after operator\n");
            return -1;
        }
        int pr=0, pw=0, px=0, any=0;
        while (*p=='r' || *p=='w' || *p=='x') {
            any = 1;
            if (*p=='r') pr = 1;
            else if (*p=='w') pw = 1;
            else px = 1;
            ++p;
        }
        if (!any && op!='=') {
            fprintf(stderr, "Missing permission set in clause\n");
            return -1;
        }

        apply_rwx(mode, w, op, pr, pw, px);
        if (*p == ',') {
            ++p;
            if (*p == '\0') { fprintf(stderr, "Trailing comma\n"); return -1; }
        } else if (*p != '\0') {
            fprintf(stderr, "Garbage near: %s\n", p);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) { usage(argv[0]); return EXIT_FAILURE; }

    const char *mode_str = argv[1];
    const char *path     = argv[2];

    struct stat st;
    if (stat(path, &st) != 0) { perror("stat"); return EXIT_FAILURE; }

    mode_t new_mode = st.st_mode;

    if (is_octal(mode_str)) {
        if (parse_octal(mode_str, &new_mode) != 0) {
            fprintf(stderr, "Invalid octal mode: %s\n", mode_str);
            return EXIT_FAILURE;
        }
    } else {
        if (parse_symbolic_and_apply(mode_str, &new_mode) != 0)
            return EXIT_FAILURE;
    }

    if (chmod(path, new_mode) != 0) { perror("chmod"); return EXIT_FAILURE; }
    return EXIT_SUCCESS;
}
