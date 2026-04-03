#define NOB_IMPLEMENTATION
#include "thirdparty/nob.h"


#define SRC_DIR "./src_bootstrap/"
#define BUILD_DIR "./bootstrap/"

#define BINARY "zeus"


int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *build_mode = "-d";
    if (argc > 1) build_mode = argv[1];

    if (!mkdir_if_not_exists(BUILD_DIR)) return 1;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "gcc", SRC_DIR"zeus.c", "-o", BUILD_DIR BINARY);
    if (strcmp(build_mode, "-r")) nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb", "-O0");
    else nob_cmd_append(&cmd, "-O3");

    if (!nob_cmd_run_sync(cmd)) return 1;

    return 0;
}

