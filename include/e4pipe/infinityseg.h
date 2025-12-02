#pragma once

#include <stddef.h>

#ifndef INFINITYSEG_DEFAULT_CAPACITY
#define INFINITYSEG_DEFAULT_CAPACITY (256u * 1024u)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct infinityseg
{
    // p[0]=read, p[1]=write
    int p[2];
    // bytes currently in this pipe
    size_t len;
    // pipe capacity (best effort)
    size_t cap;

    struct infinityseg *next;
};

struct infinityseg *infinityseg_new(size_t cap_hint, int flags);

void infinityseg_free(struct infinityseg* s);

#ifdef __cplusplus
}
#endif