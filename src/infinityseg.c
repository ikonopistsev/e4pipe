#define _GNU_SOURCE

#include "e4pipe/infinityseg.h"

#define _FILE_OFFSET_BITS 64
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#ifdef __linux__
#ifndef SPLICE_F_MOVE
#include <linux/splice.h>
#endif
#endif

int x_pipe2(int p[2], int flags)
{
#ifdef __linux__
    return pipe2(p, flags);
#else
    (void)flags;
    return pipe(p);
#endif
}

#ifdef __linux__
static size_t get_pipe_sz(int fd)
{
#ifdef F_GETPIPE_SZ
    int v = fcntl(fd, F_GETPIPE_SZ);
    if (v > 0)
        return (size_t)v;
#endif
    return INFINITYSEG_DEFAULT_CAPACITY;
}

size_t try_set_pipe_sz(int fd, size_t want)
{
#ifdef F_SETPIPE_SZ
    int rc = fcntl(fd, F_SETPIPE_SZ, (int)want);
    if (rc > 0)
        return (size_t)rc;
#endif
    return get_pipe_sz(fd);
}
#endif

struct infinityseg *infinityseg_new(size_t cap_hint, int flags)
{
    struct infinityseg *s = (struct infinityseg *)calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    if (x_pipe2(s->p, flags) != 0)
    {
        free(s);
        return NULL;
    }

#ifdef __linux__
    s->cap = try_set_pipe_sz(s->p[0], 
        cap_hint ? 
            cap_hint : INFINITYSEG_DEFAULT_CAPACITY);
#else
    (void)cap_hint;
    s->cap = INFINITYSEG_DEFAULT_CAPACITY;
#endif
    s->len = 0;
    s->next = NULL;
    return s;
}

void infinityseg_free(struct infinityseg* s)
{
    assert(s);
    close(s->p[0]);
    close(s->p[1]);
    free(s);
}