#pragma once

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "pipeevent-st.h"

#define PEV_PENDING_READ  0x01u
#define PEV_PENDING_WRITE 0x02u

void pipeevent_ip_notify_(void *arg);

void pipeevent_on_deferred_(evutil_socket_t fd, short what, void *arg);

void pipeevent_flush_output_(struct pipeevent *pev);

void pipeevent_run_pending_(struct pipeevent *pev);