#pragma once

#include "e4pipe/pipeevent.h"
#include "e4pipe/infinitypipe_struct.h"

#include <event2/event_struct.h>

struct pipeevent {
    struct event_base *base;
    int fd;
    int options;

    short enabled;

    struct event ev_read;
    struct event ev_write;
    int ev_write_added;

    /* deferred dispatcher */
    struct event ev_deferred;
    int deferred_scheduled;

    pipeevent_data_cb  readcb;
    pipeevent_data_cb  writecb;
    pipeevent_event_cb eventcb;
    void *cb_ctx;

    struct infinitypipe in;
    struct infinitypipe out;

    unsigned pending_flags;
    int cb_running;
};
