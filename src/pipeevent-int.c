#define _GNU_SOURCE

#include "pipeevent-int.h"

static void arm_write_event(struct pipeevent *pev)
{
    if (!(pev->enabled & EV_WRITE)) return;
    if (!pev->ev_write_added) {
        event_add(pev->ev_write, NULL);
        pev->ev_write_added = 1;
    }
}

static void disarm_write_event_if_empty(struct pipeevent *pev)
{
    if (pev->ev_write_added && infinitypipe_len(&pev->out) == 0) {
        event_del(pev->ev_write);
        pev->ev_write_added = 0;
    }
}

void pipeevent_ip_notify_(void *arg)
{
    struct pipeevent *pev = (struct pipeevent*)arg;
    if (!pev->deferred_scheduled) {
        pev->deferred_scheduled = 1;
        struct timeval tv = {0, 0};
        evtimer_add(&pev->ev_deferred, &tv);
    }
}

void pipeevent_flush_output_(struct pipeevent *pev)
{
    if (!(pev->enabled & EV_WRITE)) return;

    for (;;) {
        if (infinitypipe_len(&pev->out) == 0) {
            disarm_write_event_if_empty(pev);
            return;
        }

        ssize_t rc = infinitypipe_splice_out(&pev->out, pev->fd, 8u * 1024u * 1024u);
        if (rc > 0) {
            /* out changed; infinitypipe already scheduled deferred tick */
            continue;
        }

        if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            arm_write_event(pev);
            return;
        }

        /* error */
        if (pev->eventcb) pev->eventcb(pev, PEV_EVENT_ERROR, pev->cb_ctx);
        return;
    }
}

void pipeevent_run_pending_(struct pipeevent *pev)
{
    if (pev->cb_running) return;
    pev->cb_running = 1;

    while (pev->pending_flags) {
        unsigned p = pev->pending_flags;
        pev->pending_flags = 0;

        if ((p & PEV_PENDING_READ) && pev->readcb)
            pev->readcb(pev, pev->cb_ctx);

        if (p & PEV_PENDING_WRITE) {
            pipeevent_flush_output_(pev);
            if (pev->writecb && infinitypipe_len(&pev->out) == 0)
                pev->writecb(pev, pev->cb_ctx);
        }
    }

    pev->cb_running = 0;
}

void pipeevent_on_deferred_(evutil_socket_t fd, short what, void *arg)
{
    (void)fd; (void)what;
    struct pipeevent *pev = (struct pipeevent*)arg;
    pev->deferred_scheduled = 0;

    /* pull buffered deltas into pipeevent pending flags */
    struct infinitypipe_info stat_in;
    infinitypipe_get_stat(&pev->in, &stat_in);
    if (stat_in.n_added || stat_in.n_deleted)
        pev->pending_flags |= PEV_PENDING_READ;

    struct infinitypipe_info stat_out;
    infinitypipe_get_stat(&pev->out, &stat_out);
    if (stat_out.n_added || stat_out.n_deleted)
        pev->pending_flags |= PEV_PENDING_WRITE;

    pipeevent_run_pending_(pev);
}
