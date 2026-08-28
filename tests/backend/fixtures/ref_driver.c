/* The C REFERENCE DRIVER — the conformance suite's other end (1.1.13c).
 *
 * TEST TOOLING, NEVER IN THE ARTIFACT (the valgrind rule): the harness
 * builds this with the system C compiler and hands its path to tests via
 * `// argv:`. It implements the interface `extern_c_driver.npk` declares,
 * spelled canonically below so both sides compute the SAME hash — which is
 * the connect-time check doing its job, not a copy that can drift: change
 * either side's signatures and the handshake refuses.
 *
 * Kernels (declaration order = kernel_id space):
 *   0  echo_add(int64, int64) -> int64     args echoed: x + y at resp_off
 *   1  hostile_tail() -> NIL               completes normally, then writes a
 *                                          tail OUTSIDE [head-cap, head] — the
 *                                          NEXT dispatch must kill (the §6.2
 *                                          [UNTRUSTED] validation)
 *   2  refuses() -> int64                  reports status 55: the Bridge
 *                                          fails EDriverError
 */
#include "../../../sdk/npkdrv.h"

static const char *const CANON[] = {
    "echo_add=int64(int64,int64)",
    "hostile_tail=NIL()",
    "refuses=int64()",
    NULL,
};

int main(void) {
    npkdrv d;
    if (npkdrv_init(&d, npkdrv_iface_hash(CANON)) != 0) return 10;
    for (;;) {
        npkdrv_desc req;
        int r = npkdrv_next(&d, &req);
        if (r == 0) return 0;
        if (r < 0) return 11;
        switch (req.kernel_id) {
        case 0: {
            int64_t x, y;
            memcpy(&x, d.shm + req.arg_off, 8);
            memcpy(&y, d.shm + req.arg_off + 8, 8);
            int64_t sum = x + y;
            memcpy(d.shm + req.resp_off, &sum, 8);
            if (npkdrv_complete(req.seq, 0) != 0) return 12;
            break;
        }
        case 1: {
            if (npkdrv_complete(req.seq, 0) != 0) return 12;
            /* AFTER completing: a hostile free-running tail, far outside
             * [head - capacity, head]. The next dispatch's validation must
             * kill this process and fail EDriverProtocol. */
            atomic_store_explicit(d.tail,
                atomic_load_explicit(d.head, memory_order_acquire) +
                    (int64_t)d.capacity + 5,
                memory_order_release);
            break;
        }
        case 2: {
            fprintf(stderr, "ref_driver: refusing kernel 2 on purpose\n");
            if (npkdrv_complete(req.seq, 55) != 0) return 12;
            break;
        }
        default:
            fprintf(stderr, "ref_driver: unknown kernel %u\n", req.kernel_id);
            if (npkdrv_complete(req.seq, 99) != 0) return 12;
            break;
        }
    }
}
