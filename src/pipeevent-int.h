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

void pipev_ip_notify(void *arg);

void pipev_on_deferred(evutil_socket_t fd, short what, void *arg);

void pipev_flush_output(struct pipeevent *pev);

void pipev_run_pending(struct pipeevent *pev);