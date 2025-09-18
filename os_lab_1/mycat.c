#include "mycat.h"
#include <stdio.h>
#include <string.h>

static int print_file(FILE *f, int n_flag, int b_flag, int e_flag) {
    char line[4096];
    int line_num = 1;
    while (fgets(line, sizeof(line), f)) {
        int print_num = 0;

        if (b_flag) {
            if (!(line[0] == '\n' && line[1] == '\0')) print_num = 1;
        } else if (n_flag) {
            print_num = 1;
        }

        if (print_num) {
            printf("%6d\t", line_num++);
        }

        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            if (e_flag) printf("%s$\n", line);
            else        printf("%s\n",  line);
        } else {
            if (e_flag) printf("%s$", line);
            else        printf("%s",  line);
        }
    }
    return 0;
}

int mycat_run(int argc, char *argv[]) {
    int n_flag = 0, b_flag = 0, e_flag = 0;
    int first_file = -1;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (a[0] == '-' && a[1] != '\0') {
            if (strchr(a, 'n')) n_flag = 1;
            if (strchr(a, 'b')) b_flag = 1;
            if (strchr(a, 'E')) e_flag = 1;
        } else {
            first_file = i;
            break;
        }
    }
    if (first_file == -1) {
        return print_file(stdin, n_flag, b_flag, e_flag);
    }
    for (int i = first_file; i < argc; ++i) {
        const char *path = argv[i];
        FILE *f = fopen(path, "r");
        if (!f) {
            perror(path);
            continue; 
        }
        print_file(f, n_flag, b_flag, e_flag);
        fclose(f);
    }
    return 0;
}
