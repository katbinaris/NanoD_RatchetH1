#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A connection that hasn't proved the pairing key yet (net_link.c), and the rule for where a new
// one goes -- apart, so a host test can check the rule (tools/net_pend_test).

typedef struct {
    int fd;          // -1: a free slot
    uint32_t addr;   // the peer's IPv4 address
    int64_t since;   // accepted at (esp_timer_get_time())
    uint8_t buf[20]; // its hello, then its proof
    size_t have;
    bool replied;    // our half of the handshake is out: its proof next
    uint8_t k[32];   // the session key, from its hello on
} net_pend_t;

// The slot for a new connection from `addr`: that peer's own earlier attempt (one slot per peer
// address, so a peer that floods only ever replaces itself and the others' handshakes go on),
// else a free slot, else the oldest attempt -- it has had the longest to finish.
static inline int net_pend_pick(const net_pend_t *p, int n, uint32_t addr) {
    for (int i = 0; i < n; i++)
        if (p[i].fd >= 0 && p[i].addr == addr) return i;
    for (int i = 0; i < n; i++)
        if (p[i].fd < 0) return i;
    int oldest = 0;
    for (int i = 1; i < n; i++)
        if (p[i].since < p[oldest].since) oldest = i;
    return oldest;
}
