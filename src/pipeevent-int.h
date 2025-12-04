#pragma once

#include "e4pipe/pipeevent_struct.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define PEV_PENDING_READ  EV_READ
#define PEV_PENDING_WRITE EV_WRITE

void pipev_ip_notify(void *arg);

void pipev_run_pending(struct pipeevent *pev);

void pipev_flush_output(struct pipeevent *pev);

void pipev_on_deferred(evutil_socket_t fd, short what, void *arg);

void pipev_on_readable(evutil_socket_t fd, short what, void *arg);

void pipev_on_writable(evutil_socket_t fd, short what, void *arg);