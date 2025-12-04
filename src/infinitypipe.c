#define _GNU_SOURCE

#include "e4pipe/infinitypipe.h"
#include "infinitypipe-int.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

size_t infinitypipe_get_length(const struct infinitypipe *ip)
{
    return ip->total_len;
}

void infinitypipe_mark(struct infinitypipe *ip, struct infinitypipe_mark *m)
{
    m->last_before = ip->tail;
}

void infinitypipe_set_max_size(struct infinitypipe *ip, size_t max_size)
{
    ip->max_size = max_size;
}

void infinitypipe_setcb(struct infinitypipe *ip, infinitypipe_notify_fn fn, void *fn_arg)
{
    assert(ip);

    ip->fn = fn;
    ip->fn_arg = fn_arg;
}

int infinitypipe_get_stat(struct infinitypipe *ip, struct infinitypipe_info *stat)
{
    assert(ip);
    assert(stat);

    if (ip->stat.n_added == 0 && ip->stat.n_deleted == 0)
    {
        ip->notify_pending = 0;
        return 0;
    }

    *stat = ip->stat;

    ip->stat.orig_size = ip->total_len;
    ip->stat.n_added = 0;
    ip->stat.n_deleted = 0;
    ip->notify_pending = 0;

    return 1;
}

/* ---- public ---- */

int infinitypipe_init(struct infinitypipe *ip, size_t seg_capacity, int flags)
{
    assert(ip);

    memset(ip, 0, sizeof(*ip));
    ip->seg_capacity = seg_capacity ? seg_capacity : INFINITYSEG_DEFAULT_CAPACITY;
    ip->flags = flags;
    ip->max_size = INFINITYPIPE_MAX_SIZE;
    return 0;
}

void infinitypipe_free(struct infinitypipe *ip)
{
    if (!ip)
        return;

    struct infinityseg *s = ip->head;
    while (s)
    {
        struct infinityseg *n = s->next;
        infinityseg_free(s);
        s = n;
    }
    memset(ip, 0, sizeof(*ip));
}

ssize_t infinitypipe_splice_in(struct infinitypipe *ip, int in_fd, size_t max_bytes)
{
#ifndef __linux__
    (void)ip;
    (void)in_fd;
    (void)max_bytes;
    errno = ENOSYS;
    return -1;
#else
    size_t total = 0;

    while (total < max_bytes)
    {
        // ЛИМИТ ОБЩЕЙ ДЛИНЫ
        if (ip->total_len >= ip->max_size) {
            errno = EAGAIN;  // буфер заполнен
            
            if (total)
                break;       // уже что-то прочитали в этом вызове

            return -1;       // сразу сигнализируем наверх
        }        

        struct infinityseg *s = ip->tail;
        size_t newly_allocated = 0;

        /* Нужен новый сегмент? Создаём, но НЕ прицепляем к списку пока не будет rc>0 */
        if (!s || s->len >= s->cap)
        {
            s = infinityseg_new(ip->seg_capacity, ip->flags);
            if (!s)
            {
                if (errno == EMFILE || errno == ENFILE) {
                    // буфер забит по ресурсам
                    errno = EAGAIN;   
                }
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

        ssize_t rc = splice(in_fd, NULL, s->p[1], 
            NULL, want, SPLICE_F_MOVE|SPLICE_F_NONBLOCK);

        if (rc > 0)
        {
            if (newly_allocated)
            {
                /* только теперь сегмент становится "активным" */
                ip_seg_add(ip, s);
            }

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
            /* EOF */
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

ssize_t infinitypipe_splice_out(struct infinitypipe *ip, int out_fd, size_t max_bytes)
{
#ifndef __linux__
    (void)ip;
    (void)out_fd;
    (void)max_bytes;
    errno = ENOSYS;
    return -1;
#else
    size_t total = 0;

    while (ip->head && total < max_bytes)
    {
        struct infinityseg *s = ip->head;
        size_t want = max_bytes - total;
        if (want > s->len)
            want = s->len;
        if (want == 0)
            break;

        ssize_t rc = splice(s->p[0], NULL, out_fd, NULL, want,
                            SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
        if (rc > 0)
        {
            s->len -= (size_t)rc;
            ip_dec_total_len(ip, (size_t)rc);
            total += (size_t)rc;

            if (s->len == 0)
            {
                ip->head = s->next;
                if (!ip->head)
                    ip->tail = NULL;

                infinityseg_free(s);
            }
            continue;
        }

        if (rc == 0)
        {
            // eof socket close
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
        ip_note_change(ip, 0, total);

    return (ssize_t)total;
#endif
}

ssize_t infinitypipe_discard(struct infinitypipe *ip, size_t max_bytes)
{
#ifndef __linux__
    (void)ip;
    (void)max_bytes;
    errno = ENOSYS;
    return -1;
#else
    int dn = open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (dn < 0)
        return -1;
    ssize_t rc = infinitypipe_splice_out(ip, dn, max_bytes);
    int e = errno;
    close(dn);
    errno = e;
    return rc;
#endif
}

ssize_t infinitypipe_move(struct infinitypipe *dst, struct infinitypipe *src, size_t max_bytes)
{
#ifndef __linux__
    (void)dst;
    (void)src;
    (void)max_bytes;
    errno = ENOSYS;
    return -1;
#else
    size_t total = 0;
    if (!dst || !src)
    {
        errno = EINVAL;
        return -1;
    }

    if (max_bytes == 0 || src->total_len == 0)
        return 0;

    // fast path: dst пустой и забираем всё
    if (!dst->head && max_bytes >= src->total_len)
    {
        size_t moved = src->total_len;

        // ip_stat_begin(dst, dst->total_len);
        dst->head = src->head;
        dst->tail = src->tail;
        ip_inc_total_len(dst, moved);

        // ip_stat_begin(src, src->total_len);
        src->head = src->tail = NULL;
        ip_dec_total_len(src, moved);

        /* уведомим про изменения */
        ip_note_change(dst, moved, 0);
        ip_note_change(src, 0, moved);

        return (ssize_t)moved;
    }

    /* 1) move whole segments by relinking */
    while (src->head && total < max_bytes)
    {
        struct infinityseg *ss = src->head;

        size_t remain = max_bytes - total;
        if (ss->len > remain)
            break; /* need partial move */
        /* ss->len == 0 не ожидается по инвариантам */

        src->head = ss->next;
        if (!src->head)
            src->tail = NULL;
        ss->next = NULL;

        if (!dst->head)
            dst->head = dst->tail = ss;
        else
        {
            dst->tail->next = ss;
            dst->tail = ss;
        }

        ip_dec_total_len(src, ss->len);
        ip_inc_total_len(dst, ss->len);
        total += ss->len;
    }

    /* 2) partial via splice(pipe->pipe) */
    while (src->head && total < max_bytes)
    {
        struct infinityseg *ss = src->head;
        struct infinityseg *ds = dst->tail;
        if (!ds || ds->len >= ds->cap)
        {
            ds = infinityseg_new(dst->seg_capacity, dst->flags);
            if (!ds)
                break;
            ip_seg_add(dst, ds);
        }

        size_t room = ds->cap - ds->len;
        size_t want = max_bytes - total;
        if (want > room)
            want = room;
        if (want > ss->len)
            want = ss->len;
        if (want == 0)
            break;

        ssize_t rc = splice(ss->p[0], NULL, ds->p[1], NULL, want,
                            SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
        if (rc > 0)
        {
            ss->len -= (size_t)rc;
            ip_dec_total_len(src, (size_t)rc);

            ds->len += (size_t)rc;
            ip_inc_total_len(dst, (size_t)rc);

            total += (size_t)rc;

            if (ss->len == 0)
            {
                src->head = ss->next;
                if (!src->head)
                    src->tail = NULL;
                infinityseg_free(ss);
            }
            continue;
        }

        if (rc == 0)
            break;
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
        break;
    }

    if (total)
    {
        ip_note_change(dst, total, 0);
        ip_note_change(src, 0, total);
    }

    return (ssize_t)total;
#endif
}

ssize_t infinitypipe_tee_pipe(struct infinitypipe *ip,
    const struct infinitypipe_mark *m, int pipe_fd, size_t max_bytes)
{
#ifndef __linux__
    (void)ip;
    (void)m;
    (void)pipe_fd;
    (void)max_bytes;
    errno = ENOSYS;
    return -1;
#else
    assert(ip);

    if (!m) {
        errno = EINVAL;
        return -1;
    }

    struct infinityseg *s =
        (!m || !m->last_before) ? ip->head : m->last_before->next;

    size_t total = 0;
    for (; s && total < max_bytes; s = s->next)
    {
        size_t left = s->len;
        while (left > 0 && total < max_bytes)
        {
            size_t remain = max_bytes - total;
            if (left > remain)
                left = remain;

            ssize_t rc = tee(s->p[0], pipe_fd, left, SPLICE_F_NONBLOCK);
            if (rc > 0)
            {
                total += (size_t)rc;
                left  -= (size_t)rc;
                continue;
            }
            if (rc == 0)
                break;
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return total ? (ssize_t)total : -1;
            return -1;
        }
    }

    return (ssize_t)total;
#endif
}

ssize_t infinitypipe_tee(struct infinitypipe *ip,
    struct infinityseg *seg, const struct infinitypipe_mark *m)
{
#ifndef __linux__
    (void)ip;
    (void)seg;
    (void)m;
    errno = ENOSYS;
    return -1;
#else
    assert(ip);

    if (!seg) {
        errno = EINVAL;
        return -1;
    }

    if (seg->len >= seg->cap) {
        errno = EAGAIN;  // сегмент полон
        return -1;
    }

    size_t room = seg->cap - seg->len;
    ssize_t rc = infinitypipe_tee_pipe(ip, m, seg->p[1], room);
    if (rc > 0) {
        seg->len += (size_t)rc;
    }
    return rc;
#endif
}