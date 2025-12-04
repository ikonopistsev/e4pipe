#define _GNU_SOURCE

#include "pipeevent-int.h"
#include "infinitypipe-int.h"

static inline void pipev_arm_write_event(struct pipeevent *pev)
{
    if (!(pev->enabled & EV_WRITE)) 
        return;

    if (!pev->ev_write_added) 
    {
        event_add(&pev->ev_write, NULL);
        pev->ev_write_added = 1;
    }
}

static inline void pipev_disarm_write_event(struct pipeevent *pev)
{
    if (pev->ev_write_added && ip_is_empty(&pev->out)) 
    {
        event_del(&pev->ev_write);
        pev->ev_write_added = 0;
    }
}

void pipev_flush_output(struct pipeevent *pev)
{
    if (!(pev->enabled & EV_WRITE)) 
        return;

    for (;;) {
        if (ip_is_empty(&pev->out)) {
            pipev_disarm_write_event(pev);
            return;
        }

        ssize_t rc = infinitypipe_splice_out(&pev->out, 
            pev->fd, INFINITYPIPE_MAX_SPLICE_AT_ONCE);

        if (rc > 0) {
            // out changed; infinitypipe already scheduled deferred tick
            continue;
        }

        if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pipev_arm_write_event(pev);
            return;
        }

        // error
        if (pev->eventcb) 
            pev->eventcb(pev, PEV_EVENT_ERROR, pev->cb_ctx);
            
        return;
    }
}

void pipev_run_pending(struct pipeevent *pev)
{
    if (pev->cb_running) 
        return;
    
    pev->cb_running = 1;

    while (pev->pending_flags) 
    {
        unsigned p = pev->pending_flags;
        pev->pending_flags = 0;

        if ((p & PEV_PENDING_READ) && pev->readcb) {
            pev->readcb(pev, pev->cb_ctx);
        }

        if (p & PEV_PENDING_WRITE) 
        {            
            pipev_flush_output(pev);

            if (pev->writecb && ip_is_empty(&pev->out))
                pev->writecb(pev, pev->cb_ctx);
        }
    }

    pev->cb_running = 0;
}

void pipev_on_deferred(evutil_socket_t fd, short what, void *arg)
{
    (void)fd; (void)what;
    struct pipeevent *pev = (struct pipeevent*)arg;
    pev->deferred_scheduled = 0;

    // pull buffered deltas into pipeevent pending flags
    struct infinitypipe_info st;
    size_t any = 0;
    if (infinitypipe_get_stat(&pev->in, &st)) {
        pev->pending_flags |= PEV_PENDING_READ;
        any = 1;
    }

    if (infinitypipe_get_stat(&pev->out, &st)) {
        pev->pending_flags |= PEV_PENDING_WRITE;
        any = 1;
    }

    if (any) {
        //fprintf(stdout, ".");
        pipev_run_pending(pev);
    }
}
void pipev_on_readable(evutil_socket_t fd, short what, void *arg)
{
    (void)what;
    struct pipeevent *pev = (struct pipeevent *)arg;

    ssize_t n = infinitypipe_splice_in(&pev->in, (int)fd, INFINITYPIPE_MAX_SPLICE_AT_ONCE);
    if (n > 0)
    {
        // infinitypipe already scheduled deferred via notify
        return;
    }
    
    if (n == 0)
    {
        pipeevent_disable(pev, EV_READ);

        if (pev->eventcb)
            pev->eventcb(pev, PEV_EVENT_EOF, pev->cb_ctx);
        
        return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) 
    {
        pipeevent_disable(pev, EV_READ);
        return;
    }

    pipeevent_disable(pev, EV_READ|EV_WRITE);
    if (pev->eventcb) 
        pev->eventcb(pev, PEV_EVENT_ERROR, pev->cb_ctx);
}

void pipev_ip_notify(void *arg)
{
    struct pipeevent *pev = (struct pipeevent*)arg;
    // если нет запущенных отложенных действий
    if (!pev->deferred_scheduled) 
    {
        // если мы уже выполняем каллбек
        // то стартуем его снова как отложенный
        if (pev->cb_running) 
        {            
            pev->deferred_scheduled = 1;
            struct timeval tv = {0, 0};
            //fprintf(stdout, " ");
            evtimer_add(&pev->ev_deferred, &tv);
            return;
        }

        // иначе fast-path: проверяем статистику 
        // и запускаем коллбеки напрямую
        struct infinitypipe_info st;
        size_t any = 0;

        if (infinitypipe_get_stat(&pev->in, &st)) {
            pev->pending_flags |= PEV_PENDING_READ;
            any = 1;
        }
        if (infinitypipe_get_stat(&pev->out, &st)) {
            pev->pending_flags |= PEV_PENDING_WRITE;
            any = 1;
        }

        if (any) {
            // Запускаем user-коллбеки напрямую.
            // Внутри pipev_run_pending() должен выставляться cb_running=1 на время
            // выполнения коллбеков и сбрасываться в 0 по завершению.
            //fprintf(stdout, "*");
            pipev_run_pending(pev);        
        }
    } else {
        //fprintf(stdout, "-");
    }
}