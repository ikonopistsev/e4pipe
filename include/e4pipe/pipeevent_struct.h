#pragma once

#include "e4pipe/pipeevent.h"
#include "e4pipe/infinitypipe_struct.h"

#include <event2/event_struct.h>

struct pipeevent {
    struct event_base *base;
    evutil_socket_t fd;
    size_t options;
    short enabled;

    struct event ev_read;
    struct event ev_write;
    size_t ev_write_added;

    /* deferred dispatcher */
    struct event ev_deferred;
    size_t deferred_scheduled;

    pipeevent_data_cb  readcb;
    pipeevent_data_cb  writecb;
    pipeevent_event_cb eventcb;
    void *cb_ctx;

    struct infinitypipe in;
    struct infinitypipe out;

    unsigned pending_flags;
    size_t cb_running;
};
