#include "mygrep.h"
#include <stdio.h>
#include <string.h>

int mygrep_run(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: mygrep pattern [file]\n");
        return 1;
    }

    const char *pattern = argv[1];
    FILE *f = stdin;

    if (argc > 2) {
        f = fopen(argv[2], "r");
        if (!f) {
            perror(argv[2]);
            return 1;
        }
    }

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, pattern)) {
            fputs(line, stdout);
        }
    }

    if (f != stdin) fclose(f);
    return 0;
}
