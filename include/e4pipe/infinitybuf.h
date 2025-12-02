#pragma once

#include "e4pipe/infinitypipe.h"
#include <event2/buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

// write some of the contents of an infinitypipe to a evbuffer.
ssize_t infinitypipe_write(struct infinitypipe *ip,
    struct evbuffer *out, size_t max_bytes);

// read some of the contents of an evbuffer to a infinitypipe.
ssize_t infinitypipe_read(struct infinitypipe *ip,
    struct evbuffer *from, size_t max_bytes);

#ifdef __cplusplus
}
#endif
