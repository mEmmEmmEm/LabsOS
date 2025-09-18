#include <stdio.h>
#include <string.h>
#include "mycat.h"
#include "mygrep.h"

static const char* base_name(const char *p) {
    const char *slash = strrchr(p, '/');
#ifdef _WIN32
    const char *bslash = strrchr(p, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    return slash ? slash + 1 : p;
}

int main(int argc, char *argv[]) {
    const char *prog = base_name(argv[0]);

    if (strstr(prog, "mycat") != NULL) {
        return mycat_run(argc, argv);
    }
    if (strstr(prog, "mygrep") != NULL) {
        return mygrep_run(argc, argv);
    }

    fprintf(stderr,
        "Usage:\n"
        "  mycat [-n] [-b] [-E] [files...]\n"
        "  mygrep pattern [file]\n");
    return 1;
}
