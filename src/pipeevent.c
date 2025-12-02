#define _GNU_SOURCE

#include "pipeevent-int.h"
#include <assert.h>

static void pipeevent_on_readable(evutil_socket_t fd, short what, void *arg)
{
    (void)what;
    struct pipeevent *pev = (struct pipeevent *)arg;

    ssize_t n = infinitypipe_splice_in(&pev->in, (int)fd, INFINITYPIPE_MAX_SPLICE_AT_ONCE);
    if (n > 0)
    {
        /* infinitypipe already scheduled deferred via notify */
        return;
    }
    
    if (n == 0)
    {
        if (pev->eventcb)
            pev->eventcb(pev, PEV_EVENT_EOF, pev->cb_ctx);
        return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;

    if (pev->eventcb)
        pev->eventcb(pev, PEV_EVENT_ERROR, pev->cb_ctx);
}

static void pipeevent_on_writable(evutil_socket_t fd, short what, void *arg)
{
    (void)fd;
    (void)what;
    struct pipeevent *pev = (struct pipeevent *)arg;
    pipev_flush_output(pev);
}

/* public API */

struct pipeevent *pipeevent_socket_new(struct event_base *base, int fd, int options)
{
#ifndef __linux__
    (void)base;
    (void)fd;
    (void)options;
    errno = ENOSYS;
    return NULL;
#else
    struct pipeevent *pev = (struct pipeevent *)calloc(1, sizeof(*pev));
    if (!pev)
        return NULL;

    pev->base = base;
    pev->fd = fd;
    pev->options = options;

    if (evutil_make_socket_nonblocking(fd) != 0)
    {
        free(pev);
        return NULL;
    }

    infinitypipe_init(&pev->in, INFINITYSEG_DEFAULT_CAPACITY,
        IP_NONBLOCK|IP_CLOEXEC);
    infinitypipe_init(&pev->out, INFINITYSEG_DEFAULT_CAPACITY,
        IP_NONBLOCK|IP_CLOEXEC);

    /* pipeevent is the "parent" */
    infinitypipe_setcb(&pev->in, pipev_ip_notify, pev);
    infinitypipe_setcb(&pev->out, pipev_ip_notify, pev);

    pev->ev_read = event_new(base, fd, 
        EV_READ|EV_PERSIST, pipeevent_on_readable, pev);
    pev->ev_write = event_new(base, fd, 
        EV_WRITE|EV_PERSIST, pipeevent_on_writable, pev);
    evtimer_assign(&pev->ev_deferred, base, pipev_on_deferred, pev);

    if (!pev->ev_read || !pev->ev_write)
    {
        pipeevent_free(pev);
        return NULL;
    }

    return pev;
#endif
}

void pipeevent_free(struct pipeevent *pev)
{
    if (!pev)
        return;

    if (pev->ev_read)
    {
        event_del(pev->ev_read);
        event_free(pev->ev_read);
    }
    if (pev->ev_write)
    {
        event_del(pev->ev_write);
        event_free(pev->ev_write);
    }

    evtimer_del(&pev->ev_deferred);

    infinitypipe_free(&pev->in);
    infinitypipe_free(&pev->out);

    if ((pev->options & PEV_OPT_CLOSE_ON_FREE) && pev->fd >= 0)
        close(pev->fd);

    free(pev);
}

int pipeevent_enable(struct pipeevent *pev, short events)
{
    assert(pev);

    if ((events & EV_READ) && !(pev->enabled & EV_READ))
    {
        if (event_add(pev->ev_read, NULL) != 0)
            return -1;
        pev->enabled |= EV_READ;
    }

    if (events & EV_WRITE)
    {
        pev->enabled |= EV_WRITE;
        /* start flushing if already has data */
        if (infinitypipe_get_length(&pev->out) > 0)
            pipev_ip_notify(pev);
    }

    return 0;
}

int pipeevent_disable(struct pipeevent *pev, short events)
{
    assert(pev);

    if ((events & EV_READ) && (pev->enabled & EV_READ))
    {
        event_del(pev->ev_read);
        pev->enabled &= ~EV_READ;
    }

    if ((events & EV_WRITE) && (pev->enabled & EV_WRITE))
    {
        if (pev->ev_write_added)
        {
            event_del(pev->ev_write);
            pev->ev_write_added = 0;
        }
        pev->enabled &= ~EV_WRITE;
    }

    return 0;
}

void pipeevent_setcb(struct pipeevent *pev,
    pipeevent_data_cb readcb, pipeevent_data_cb writecb,
    pipeevent_event_cb eventcb, void *cb_ctx)
{
    pev->readcb = readcb;
    pev->writecb = writecb;
    pev->eventcb = eventcb;
    pev->cb_ctx = cb_ctx;
}

int pipeevent_get_fd(struct pipeevent *pev)
{
    assert(pev);
    return pev->fd;
}

struct event_base *pipeevent_get_base(struct pipeevent *pev)
{
    assert(pev);
    return pev->base;
}

struct infinitypipe *pipeevent_get_input(struct pipeevent *pev) 
{ 
    assert(pev);
    return &pev->in; 
}
struct infinitypipe *pipeevent_get_output(struct pipeevent *pev) 
{ 
    assert(pev);
    return &pev->out; 
}
