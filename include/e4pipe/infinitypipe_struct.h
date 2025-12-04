#pragma once

#include "e4pipe/infinitypipe.h"

struct infinitypipe
{
    struct infinityseg *head;
    struct infinityseg *tail;
    struct infinitypipe_info stat;
    size_t seg_capacity;
    size_t total_len;
    int flags;
    // maximum allowed size of the pipe
    size_t max_size;
    // callback
    infinitypipe_notify_fn fn;
    void *fn_arg;
    size_t notify_pending;
};