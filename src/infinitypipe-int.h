#pragma once

#include "e4pipe/infinitypipe_struct.h"

static inline void ip_inc_total_len(struct infinitypipe *ip, size_t val)
{
    if (ip->stat.n_added == 0 && ip->stat.n_deleted == 0)
    {
        ip->stat.orig_size = ip->total_len;
    }

    ip->total_len += val;
}

static inline void ip_dec_total_len(struct infinitypipe *ip, size_t val)
{
    if (ip->stat.n_added == 0 && ip->stat.n_deleted == 0)
    {
        ip->stat.orig_size = ip->total_len;
    }

    ip->total_len -= val;
}

static inline void ip_note_change(struct infinitypipe *ip, size_t added, size_t deleted)
{
    if (added == 0 && deleted == 0)
        return;

    ip->stat.n_added += added;
    ip->stat.n_deleted += deleted;

    /* lightweight notify to parent: schedule deferred tick */
    if (ip->fn) 
    {
        ip->notify_pending = 1;
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

static inline void ip_seg_free_head(struct infinitypipe *ip)
{
    struct infinityseg *s = ip->head;
    if (!s) return;

    ip->head = s->next;
    if (!ip->head)
        ip->tail = NULL;
    
    infinityseg_free(s);
}