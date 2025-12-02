#pragma once

#include "e4pipe/infinitypipe.h"

static inline void ip_note_change(struct infinitypipe *ip, size_t added, size_t deleted)
{
    if (added == 0 && deleted == 0)
        return;

    ip->stat.n_added += added;
    ip->stat.n_deleted += deleted;

    /* lightweight notify to parent: schedule deferred tick */
    if (ip->fn) 
    {
        ip->fn(ip->fn_arg);
    }
}

static inline void ip_seg_add(struct infinitypipe *ip, struct infinityseg *s)
{
    if (!ip->head)
    {
        ip->head = ip->tail = s;
    }
    else
    {
        ip->tail->next = s;
        ip->tail = s;
    }
}