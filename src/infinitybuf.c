#define _GNU_SOURCE

#include "e4pipe/infinitybuf.h"
#include "infinitypipe-int.h"

#include <unistd.h>
#include <errno.h>
#include <assert.h>

ev_ssize_t infinitypipe_write(struct infinitypipe *ip,
    struct evbuffer *out, size_t max_bytes)
{
#ifndef __linux__
    (void)ip;
    (void)out;
    (void)max_bytes;
    errno = ENOSYS;
    return -1;
#else
    assert(ip);

    if (!out)
    {
        errno = EINVAL;
        return -1;
    }
    if (max_bytes == 0 || ip->head == NULL)
        return 0;

    size_t total = 0;

    while (ip->head && total < max_bytes)
    {
        struct infinityseg *s = ip->head;

        // таких у нас быть не должно
        assert(s->len == 0);

        size_t avail = s->len;
        size_t want = max_bytes - total;
        if (want > avail)
            want = avail;
        if (want == 0)
            break;

        struct evbuffer_iovec vec[1];
        int n = evbuffer_reserve_space(out, want, vec, 1);
        if (n <= 0)
        {
            if (!total)
            {
                errno = ENOBUFS;
                return -1;
            }
            break;
        }

        /* реально читаем не больше, чем нам отдали в iov */
        size_t to_read = vec[0].iov_len;
        if (to_read > want)
            to_read = want;

        ssize_t r = read(s->p[0], vec[0].iov_base, to_read);
        if (r > 0)
        {
            vec[0].iov_len = (size_t)r;
            evbuffer_commit_space(out, vec, 1);

            s->len -= (size_t)r;
            ip_dec_total_len(ip, (size_t)r);
            total += (size_t)r;

            if (s->len == 0)
            {
                ip->head = s->next;
                if (!ip->head)
                    ip->tail = NULL;
                infinityseg_free(s);
            }
            continue;
        }

        /* ничего не прочитали — освободим зарезервированное место */
        vec[0].iov_len = 0;
        evbuffer_commit_space(out, vec, 1);

        if (r == 0)
        {
            /* EOF по pipe — странно, но считаем, что данных больше нет */
            break;
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            if (!total)
                return -1;
            break;
        }

        /* ошибка */
        if (!total)
            return -1;
        break;
    }

    if (total)
        ip_note_change(ip, 0, total);

    return (ssize_t)total;
#endif
}

ev_ssize_t infinitypipe_read(struct infinitypipe *ip,
    struct evbuffer *in, size_t max_bytes)
{
#ifndef __linux__
    (void)ip;
    (void)in;
    (void)max_bytes;
    errno = ENOSYS;
    return -1;
#else
    assert(ip);

    if (!in)
    {
        errno = EINVAL;
        return -1;
    }

    size_t avail = evbuffer_get_length(in);
    if (avail == 0 || max_bytes == 0)
        return 0;

    size_t total = 0;

    while ((total < max_bytes) && (avail > 0))
    {
        struct infinityseg *s = ip->tail;
        int newly_allocated = 0;

        /* Нужен новый сегмент? Создаём, но НЕ прицепляем к списку пока не будет rc>0 */
        if (!s || s->len >= s->cap)
        {
            s = infinityseg_new(ip->seg_capacity, ip->flags);
            if (!s)
            {
                if (total)
                    break;
                return -1;
            }
            newly_allocated = 1;
        }

        size_t room = s->cap - s->len;
        size_t want = max_bytes - total;
        if (want > room)
            want = room;
        if (want == 0)
        {
            if (newly_allocated)
                infinityseg_free(s);
            break;
        }

        int rc = evbuffer_write_atmost(in, s->p[1], (ev_ssize_t)want);

        if (rc > 0)
        {
            if (newly_allocated)
                ip_seg_add(ip, s);

            s->len += (size_t)rc;
            ip_inc_total_len(ip, (size_t)rc);
            total += (size_t)rc;
            continue;
        }

        /* ничего не записали => если сегмент новый, он должен быть убран */
        if (newly_allocated)
        {
            infinityseg_free(s);
        }

        if (rc == 0)
        {
            /* EOF or no data */
            break;
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            if (!total)
                return -1;
            break;
        }

        if (!total)
            return -1;
        break;
    }

    if (total)
        ip_note_change(ip, total, 0);

    return (ssize_t)total;
#endif
}