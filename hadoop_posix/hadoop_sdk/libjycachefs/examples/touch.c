#include <fcntl.h>

#include "common.h"

int
main(int argc, char** argv) {
    exact_args(argc, 1);

    uintptr_t instance = jycachefs_create();
    load_cfg_from_environ(instance);

    char* fsname = get_filesystem_name();
    char* mountpoint = get_mountpoint();
    int rc = jycachefs_mount(instance, fsname, mountpoint);
    if (rc != 0) {
        fprintf(stderr, "mount failed: retcode = %d\n", rc);
        return rc;
    }

    int fd = jycachefs_open(instance, argv[1], O_CREAT,  0644);
    if (fd < 0) {
        rc = fd;
        fprintf(stderr, "open failed: retcode = %d\n", rc);
    }
    return rc;
}
