#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <limits.h>
#include <stdint.h>

#define CLR_BLUE   "\x1b[34m"
#define CLR_GREEN  "\x1b[32m"
#define CLR_CYAN   "\x1b[36m"
#define CLR_RESET  "\x1b[0m"

typedef struct {
    char *name;
    char *full;
    struct stat st;
    int ok;
} Item;

int opt_a = 0, opt_l = 0, use_color = 0;

char* join_path(const char *dir, const char *name) {
    size_t a = strlen(dir), b = strlen(name);
    int slash = (a>0 && dir[a-1] != '/');
    char *s = malloc(a + slash + b + 1);
    if (!s) return NULL;
    memcpy(s, dir, a);
    if (slash) s[a++] = '/';
    memcpy(s+a, name, b);
    s[a+b] = '\0';
    return s;
}

int cmp_items(const void *pa, const void *pb) {
    const Item *a = pa, *b = pb;
    return strcoll(a->name, b->name);
}

void mode_to_str(mode_t m, char out[11]) {
    out[0] = S_ISDIR(m)?'d': S_ISLNK(m)?'l': S_ISCHR(m)?'c': S_ISBLK(m)?'b': S_ISSOCK(m)?'s': S_ISFIFO(m)?'p':'-';
    out[1] = (m&S_IRUSR)?'r':'-'; out[2] = (m&S_IWUSR)?'w':'-'; out[3] = (m&S_IXUSR)?'x':'-';
    out[4] = (m&S_IRGRP)?'r':'-'; out[5] = (m&S_IWGRP)?'w':'-'; out[6] = (m&S_IXGRP)?'x':'-';
    out[7] = (m&S_IROTH)?'r':'-'; out[8] = (m&S_IWOTH)?'w':'-'; out[9] = (m&S_IXOTH)?'x':'-';
    if (m & S_ISUID) out[3] = (out[3]=='x')?'s':'S';
    if (m & S_ISGID) out[6] = (out[6]=='x')?'s':'S';
    if (m & S_ISVTX) out[9] = (out[9]=='x')?'t':'T';
    out[10] = '\0';
}

void fmt_time(time_t t, char *buf, size_t n) {
    time_t now = time(NULL);
    struct tm tm; localtime_r(&t, &tm);
    if (llabs((long long)(now - t)) > (long long)60*60*24*365/2)
        strftime(buf, n, "%b %e  %Y", &tm);
    else
        strftime(buf, n, "%b %e %H:%M", &tm);
}

const char* pick_color(const struct stat *st) {
    if (!use_color) return "";
    if (S_ISDIR(st->st_mode)) return CLR_BLUE;
    if (S_ISLNK(st->st_mode)) return CLR_CYAN;
    if (S_ISREG(st->st_mode) && (st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))) return CLR_GREEN;
    return "";
}

void print_one_file(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        fprintf(stderr, "myls: cannot access '%s': %s\n", path, strerror(errno));
        return;
    }
    if (!opt_l) {
        const char *c = pick_color(&st);
        const char *r = use_color ? CLR_RESET : "";
        printf("%s%s%s\n", c, path, r);
        return;
    }

    char mode[11]; mode_to_str(st.st_mode, mode);
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    char tbuf[64]; fmt_time(st.st_mtime, tbuf, sizeof tbuf);

    char sizebuf[64];
    if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
        snprintf(sizebuf, sizeof sizebuf, "%u, %u", major(st.st_rdev), minor(st.st_rdev));
    else
        snprintf(sizebuf, sizeof sizebuf, "%jd", (intmax_t)st.st_size);

    printf("%s %2ju %-8s %-8s %8s %s ",
           mode,
           (uintmax_t)st.st_nlink,
           pw?pw->pw_name:"-",
           gr?gr->gr_name:"-",
           sizebuf,
           tbuf);

    const char *c = pick_color(&st);
    const char *r = use_color ? CLR_RESET : "";
    if (S_ISLNK(st.st_mode)) {
        char target[PATH_MAX];
        ssize_t k = readlink(path, target, sizeof(target)-1);
        if (k >= 0) target[k] = '\0'; else strcpy(target, "?");
        printf("%s%s%s -> %s\n", c, path, r, target);
    } else {
        printf("%s%s%s\n", c, path, r);
    }
}

void list_dir(const char *path, int print_header, int multi) {
    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "myls: cannot open directory '%s': %s\n", path, strerror(errno));
        return;
    }

    if (multi && print_header) printf("%s:\n", path);

    size_t cap = 64, n = 0;
    Item *v = malloc(cap * sizeof(*v));
    if (!v) { closedir(d); return; }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!opt_a && de->d_name[0]=='.') continue;
        if (n == cap) {
            cap *= 2;
            Item *tmp = realloc(v, cap * sizeof(*v));
            if (!tmp) { free(v); closedir(d); return; }
            v = tmp;
        }
        v[n].name = strdup(de->d_name);
        v[n].full = join_path(path, de->d_name);
        v[n].ok = 0;
        if (v[n].full && lstat(v[n].full, &v[n].st) == 0) v[n].ok = 1;
        n++;
    }
    closedir(d);

    qsort(v, n, sizeof(*v), cmp_items);

    if (!opt_l) {
        for (size_t i=0;i<n;i++) {
            if (!v[i].ok) { printf("%s\n", v[i].name); continue; }
            const char *c = pick_color(&v[i].st);
            const char *r = (use_color && *c)? CLR_RESET : "";
            printf("%s%s%s\n", c, v[i].name, r);
        }
    } else {
        long long blocks = 0;                 // st_blocks — в 512Б блоках
        for (size_t i=0;i<n;i++) if (v[i].ok) blocks += (long long)v[i].st.st_blocks;
        printf("total %lld\n", blocks / 2);   // печатаем в «килоблоках» как GNU ls

        int w_links=1, w_owner=1, w_group=1, w_size=1;
        for (size_t i=0;i<n;i++) if (v[i].ok) {
            struct stat *st = &v[i].st;
            int t=1; for (unsigned long x=st->st_nlink; x>=10; x/=10) t++; if (t>w_links) w_links=t;
            struct passwd *pw = getpwuid(st->st_uid);
            struct group  *gr = getgrgid(st->st_gid);
            int ow = pw ? (int)strlen(pw->pw_name) : 1;
            int gw = gr ? (int)strlen(gr->gr_name) : 1;
            if (ow>w_owner) w_owner=ow;
            if (gw>w_group) w_group=gw;
            char sb[64];
            if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
                snprintf(sb, sizeof sb, "%u, %u", major(st->st_rdev), minor(st->st_rdev));
            else
                snprintf(sb, sizeof sb, "%jd", (intmax_t)st->st_size);
            int sw = (int)strlen(sb);
            if (sw>w_size) w_size=sw;
        }

        for (size_t i=0;i<n;i++) {
            if (!v[i].ok) { fprintf(stderr, "myls: cannot access '%s'\n", v[i].full); continue; }
            struct stat *st = &v[i].st;
            char mode[11]; mode_to_str(st->st_mode, mode);
            struct passwd *pw = getpwuid(st->st_uid);
            struct group  *gr = getgrgid(st->st_gid);
            char tbuf[64]; fmt_time(st->st_mtime, tbuf, sizeof tbuf);
            char sb[64];
            if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
                snprintf(sb, sizeof sb, "%u, %u", major(st->st_rdev), minor(st->st_rdev));
            else
                snprintf(sb, sizeof sb, "%jd", (intmax_t)st->st_size);

            printf("%s %*ju %-*s %-*s %*s %s ",
                   mode,
                   w_links, (uintmax_t)st->st_nlink,
                   w_owner, pw?pw->pw_name:"-",
                   w_group, gr?gr->gr_name:"-",
                   w_size,  sb,
                   tbuf);

            const char *c = pick_color(st);
            const char *r = use_color ? CLR_RESET : "";
            if (S_ISLNK(st->st_mode)) {
                char target[PATH_MAX];
                ssize_t k = readlink(v[i].full, target, sizeof(target)-1);
                if (k >= 0) target[k]='\0'; else strcpy(target, "?");
                printf("%s%s%s -> %s\n", c, v[i].name, r, target);
            } else {
                printf("%s%s%s\n", c, v[i].name, r);
            }
        }
    }

    for (size_t i=0;i<n;i++) { free(v[i].name); free(v[i].full); }
    free(v);
}

int path_is_dir(const char *p) {
    struct stat st;
    if (lstat(p, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    use_color = isatty(STDOUT_FILENO);

    int c;
    while ((c = getopt(argc, argv, "la")) != -1) {
        if (c=='l') opt_l=1;
        else if (c=='a') opt_a=1;
        else {
            fprintf(stderr, "Usage: %s [-l] [-a] [file...]\n", argv[0]);
            return 1;
        }
    }

    int narg = argc - optind;
    if (narg == 0) { list_dir(".", 0, 0); return 0; }

    int *isdir = calloc(narg, sizeof(int));
    if (!isdir) { perror("calloc"); return 1; }

    for (int i=0;i<narg;i++) isdir[i] = path_is_dir(argv[optind+i]);

    for (int i=0;i<narg;i++) { // сначала файлы/ошибки
        const char *p = argv[optind+i];
        struct stat st;
        if (lstat(p, &st)==0 && !S_ISDIR(st.st_mode)) {
            print_one_file(p);
        } else if (lstat(p, &st)!=0) {
            fprintf(stderr, "myls: cannot access '%s': %s\n", p, strerror(errno));
        }
    }

    for (int i=0;i<narg;i++) { // потом каталоги
        if (isdir[i]) {
            list_dir(argv[optind+i], 1, narg>1);
            if (i != narg-1) printf("\n");
        }
    }

    free(isdir);
    return 0;
}
