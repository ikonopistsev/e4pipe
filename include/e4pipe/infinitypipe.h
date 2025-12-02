#pragma once

#include "e4pipe/infinityseg.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INFINITYPIPE_MAX_SPLICE_AT_ONCE
#define INFINITYPIPE_MAX_SPLICE_AT_ONCE	(1<<30)
#endif

// структура изменения буфера
struct infinitypipe_info
{
    size_t orig_size;
    size_t n_added;
    size_t n_deleted;
};

typedef void (*infinitypipe_notify_fn)(void *arg);

#define IP_NONBLOCK O_NONBLOCK
#define IP_CLOEXEC O_CLOEXEC

struct infinitypipe
{
    struct infinityseg *head;
    struct infinityseg *tail;
    struct infinitypipe_info stat;
    size_t seg_capacity;
    size_t total_len;
    int flags;
    /* callback */
    infinitypipe_notify_fn fn;
    void *fn_arg;
    int notify_pending;
};

struct infinitypipe_mark
{
    struct infinityseg *last_before;
};

static inline size_t infinitypipe_get_length(const struct infinitypipe *ip)
{
    return ip->total_len;
}

static inline void infinitypipe_mark(struct infinitypipe *ip,
    struct infinitypipe_mark *m)
{
    m->last_before = ip->tail;
}

// init/free
int infinitypipe_init(struct infinitypipe *ip, size_t seg_capacity, int flags);
void infinitypipe_free(struct infinitypipe *ip);

// callbacks setup
void infinitypipe_setcb(struct infinitypipe *ip, infinitypipe_notify_fn fn, void *fn_arg);
int infinitypipe_get_stat(struct infinitypipe *ip, struct infinitypipe_info *stat);

// splice/move ops
ssize_t infinitypipe_splice_in(struct infinitypipe *ip, int in_fd, size_t max_bytes);
ssize_t infinitypipe_splice_out(struct infinitypipe *ip, int out_fd, size_t max_bytes);
ssize_t infinitypipe_move(struct infinitypipe *dst, struct infinitypipe *src, size_t max_bytes);
ssize_t infinitypipe_discard(struct infinitypipe *ip, size_t max_bytes);

ssize_t infinitypipe_tee_since(struct infinitypipe *ip,
    const struct infinitypipe_mark *m, int pipe_fd);

#ifdef __cplusplus
}
#endif
