libjycachefs
===

SDK C/C++ library for JYCacheFS.

Example
===

```c
#include "libjycachefs.h"

int instance = jycachefs_create();
jycachefs_conf_set(instance, "s3.ak", "xxx")
jycachefs_conf_set(instance, "s3.sk", "xxx")

...

int rc = jycachefs_mount(instance, "fsname", "/);
if (rc != 0) {
    // mount failed
}

rc = jycachefs_mkdir(instance_ptr, "/mydir")
if (rc != 0) {
    // mkdir failed
}
```

See [examples](examples) for more examples.
