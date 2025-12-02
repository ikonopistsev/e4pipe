#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "e4pipe/infinitypipe.h"

#include <event2/event.h>
#include <event2/bufferevent.h>

struct pipeevent;

typedef void (*pipeevent_data_cb)(struct pipeevent *pev, void *ctx);
typedef void (*pipeevent_event_cb)(struct pipeevent *pev, short what, void *ctx);

/* what для eventcb */
#define PEV_EVENT_READING BEV_EVENT_READING     /**< error encountered while reading */
#define PEV_EVENT_WRITING BEV_EVENT_WRITING     /**< error encountered while writing */
#define PEV_EVENT_EOF BEV_EVENT_EOF             /**< eof file reached */
#define PEV_EVENT_ERROR BEV_EVENT_ERROR         /**< unrecoverable error encountered */
#define PEV_EVENT_TIMEOUT BEV_EVENT_TIMEOUT     /**< user-specified timeout reached */

enum pipeevent_options
{
    PEV_OPT_CLOSE_ON_FREE = BEV_OPT_CLOSE_ON_FREE
};

// Создать pipeevent над уже открытым fd (nonblocking будет выставлен внутри)
struct pipeevent* pipeevent_socket_new(struct event_base *base,
    int fd, int options);

// Освободить, при OPT_CLOSE_ON_FREE — закрыть fd
void pipeevent_free(struct pipeevent *pev);

// EV_READ / EV_WRITE — разрешить чтение/запись (как bufferevent_enable)
int pipeevent_enable(struct pipeevent *pev, short events);
int pipeevent_disable(struct pipeevent *pev, short events);

// Установить callbacks
void pipeevent_setcb(struct pipeevent *pev,
    pipeevent_data_cb readcb, pipeevent_data_cb writecb,
    pipeevent_event_cb eventcb, void *cb_ctx);

// Доступ к fd
int pipeevent_get_fd(struct pipeevent *pev);

// Доступ к event_base
struct event_base *pipeevent_get_base(struct pipeevent *pev);

// Доступ к input/output буферам (как bufferevent_get_input/output)
struct infinitypipe *pipeevent_get_input(struct pipeevent *pev);
struct infinitypipe *pipeevent_get_output(struct pipeevent *pev);

#ifdef __cplusplus
}
#endif
