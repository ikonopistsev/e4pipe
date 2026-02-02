# e4pipe

e4pipe is a small Linux‑only helper library that provides:

- **infinitypipe** – a segmented buffer built on top of kernel pipes, using `splice(2)` / `tee(2)` for near zero‑copy I/O.
- **pipeevent** – a libevent‑style wrapper around a file descriptor, similar to `bufferevent`, but backed by `infinitypipe` instead of `evbuffer`.

The goal is to efficiently relay large amounts of data between file descriptors with minimal copying in user space.

## infinitypipe

`struct infinitypipe` is a linked list of segments (`infinityseg`); each segment owns an internal pipe and tracks how many bytes it currently stores.

Key operations (see include/e4pipe/infinitypipe.h):

- `infinitypipe_splice_in(ip, fd, max_bytes)` – read from `fd` into the buffer using `splice(2)`.
- `infinitypipe_splice_out(ip, fd, max_bytes)` – write from the buffer to `fd` using `splice(2)`.
- `infinitypipe_move(dst, src, max_bytes)` – move data between two `infinitypipe` instances, re‑linking whole segments when possible.
- `infinitypipe_discard(ip, max_bytes)` – discard data by splicing it into `/dev/null`.

The buffer has a configurable maximum size (`INFINITYPIPE_MAX_SIZE`, 64 MiB by default).
Changes to the buffer length can be tracked via `infinitypipe_setcb`, which installs a lightweight notification callback.

All operations assume non‑blocking I/O and rely on Linux‑specific syscalls (`splice`, `tee`).
On non‑Linux platforms they return `-1` with `errno = ENOSYS`.

## pipeevent

`struct pipeevent` is conceptually similar to libevent’s `struct bufferevent`:

- attached to an `event_base`;
- wraps an existing non‑blocking file descriptor;
- maintains separate `input` and `output` buffers, implemented as `infinitypipe` instances;
- controlled via `EV_READ` / `EV_WRITE` flags;
- uses callbacks for read, write‑ready, and error/EOF notifications.

Public API (see include/e4pipe/pipeevent.h):

- `pipeevent_socket_new(base, fd, options)` – create a new object over an existing descriptor; makes the fd non‑blocking.
- `pipeevent_free(pev)` – free the object; with `PEV_OPT_CLOSE_ON_FREE` it also closes the fd.
- `pipeevent_enable(pev, events)` / `pipeevent_disable(pev, events)` – enable or disable `EV_READ` / `EV_WRITE` processing (similar to `bufferevent_enable/disable`).
- `pipeevent_setcb(pev, readcb, writecb, eventcb, ctx)` – install callbacks.
- `pipeevent_get_input(pev)` / `pipeevent_get_output(pev)` – access the underlying `infinitypipe` buffers.

Callbacks:

- `readcb(pipeevent *pev, void *ctx)` – called when new data is available in `input`.
- `writecb(pipeevent *pev, void *ctx)` – called after some data from `output` has been flushed to the fd and more data can be queued.
- `eventcb(pipeevent *pev, short what, void *ctx)` – called on EOF, fatal errors, or timeouts.

The event codes and options intentionally mirror libevent:

- `PEV_EVENT_READING`, `PEV_EVENT_WRITING`, `PEV_EVENT_EOF`, `PEV_EVENT_ERROR`, `PEV_EVENT_TIMEOUT` map directly to the corresponding `BEV_EVENT_*` values.
- `PEV_OPT_CLOSE_ON_FREE` mirrors `BEV_OPT_CLOSE_ON_FREE`.

This lets you reuse typical `bufferevent` error‑handling code and port existing logic with minimal changes.

## Integration with libevent / bufferevent

e4pipe is designed to live alongside libevent:

- uses the same `event_base` and the same `EV_READ` / `EV_WRITE` model;
- event flags are compatible with `BEV_EVENT_*`;
- wraps a plain file descriptor, so it works with sockets, pipes, and other descriptors managed by libevent.

Typical usage:

1. Create an `event_base`.
2. Open a non‑blocking socket or other fd.
3. Wrap it with `pipeevent_socket_new(base, fd, PEV_OPT_CLOSE_ON_FREE)`.
4. Set callbacks with `pipeevent_setcb`.
5. Enable the desired directions with `pipeevent_enable(pev, EV_READ | EV_WRITE)`.
6. In your callbacks, manipulate data via `infinitypipe_*` helpers instead of `evbuffer_*`.

This gives you a `bufferevent`‑like programming model, but with a buffer implementation tuned for large, streaming, mostly pass‑through traffic over Linux pipes.
