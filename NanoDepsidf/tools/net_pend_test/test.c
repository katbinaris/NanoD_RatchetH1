// net_pend_pick (src/net_pend.h): where a new WiFi handshake goes. Run: tools/net_pend_test/run.sh
#include "net_pend.h"
#include <assert.h>
#include <stdio.h>

#define N 4

static net_pend_t slot(int fd, uint32_t addr, int64_t since) {
    return (net_pend_t){.fd = fd, .addr = addr, .since = since};
}

int main(void) {
    net_pend_t p[N];
    for (int i = 0; i < N; i++) p[i] = slot(-1, 0, 0);

    assert(net_pend_pick(p, N, 1) == 0); // empty: the first free slot
    p[0] = slot(10, 1, 100);
    assert(net_pend_pick(p, N, 1) == 0); // the same peer again: its own slot
    assert(net_pend_pick(p, N, 2) == 1); // another peer: a free one

    // A companion (peer 2) mid-handshake while peer 1 floods: peer 1 only ever replaces itself.
    p[1] = slot(11, 2, 200);
    for (int k = 0; k < 10000; k++) {
        int i = net_pend_pick(p, N, 1);
        assert(i == 0);
        p[i] = slot(1000 + k, 1, 300 + k);
    }
    assert(p[1].fd == 11 && p[1].addr == 2);

    // Full of other peers: the oldest gives way, the rest go on.
    p[2] = slot(12, 3, 50);
    p[3] = slot(13, 4, 400);
    assert(net_pend_pick(p, N, 5) == 2);
    p[2] = slot(14, 5, 500);
    assert(net_pend_pick(p, N, 6) == 1); // now peer 2's, at 200, is the oldest
    assert(net_pend_pick(p, N, 4) == 3); // a peer already in still replaces only itself

    puts("net_pend: ok");
    return 0;
}
