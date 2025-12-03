#define _GNU_SOURCE

#include "e4pipe/infinityseg.h"

#define _FILE_OFFSET_BITS 64
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

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
        int e = errno;
        free(s);
        // сохраним код ошибки
        errno = e;
        return NULL;
    }

#ifdef __linux__
    s->cap = try_set_pipe_sz(s->p[0],
                             cap_hint ? cap_hint : INFINITYSEG_DEFAULT_CAPACITY);
#else
    (void)cap_hint;
    s->cap = INFINITYSEG_DEFAULT_CAPACITY;
#endif
    s->len = 0;
    s->next = NULL;
    return s;
}

void infinityseg_free(struct infinityseg *s)
{
    assert(s);
    close(s->p[0]);
    close(s->p[1]);
    free(s);
}

ssize_t infinityseg_read(struct infinityseg *s, void *buf, size_t size)
{
    assert(s);

    if (!buf)
    {
        errno = EINVAL;
        return -1;
    }

    if (size == 0 || s->len == 0)
        return 0;

    // Не читаем больше, чем есть в пайпе
    size_t want = (size < s->len) ? size : s->len;
    ssize_t rc;
    do
    {
        rc = read(s->p[0], buf, want);
    // повторяем при прерывании сигналом
    } while (rc < 0 && errno == EINTR); 

    if (rc > 0)
        s->len -= (size_t)rc;

    return rc;
}

ssize_t infinityseg_write(struct infinityseg *s, const void *buf, size_t size)
{
    assert(s);

    if (!buf)
    {
        errno = EINVAL;
        return -1;
    }

    if (size == 0)
        return 0;

    // Проверяем, есть ли место в пайпе
    size_t room = s->cap - s->len;
    if (room == 0)
    {
        errno = EAGAIN;
        return -1;
    }

    // Не пишем больше, чем есть места
    size_t want = (size < room) ? size : room;
    ssize_t rc;
    do
    {
        rc = write(s->p[1], buf, want);
    // повторяем при прерывании сигналом
    } while (rc < 0 && errno == EINTR);

    if (rc > 0)
    {
        s->len += (size_t)rc;
    }

    return rc;
}