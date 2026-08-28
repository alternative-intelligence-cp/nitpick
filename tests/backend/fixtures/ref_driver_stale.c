/* The STALE C driver — built against a DIFFERENT interface spelling, so its
 * computed hash disagrees with what INIT_REQ offers and `npkdrv_init`
 * refuses the handshake: stderr names the mismatch, the process exits, the
 * Bridge's spawn reads EOF and fails EDriverSpawn. This is the wire-level
 * answer to "the driver was built against another version of the block"
 * (D-149), demonstrated rather than asserted.
 */
#include "../../../sdk/npkdrv.h"

static const char *const CANON[] = {
    "echo_add=int64(int64,int32)",   /* one spelling off: int32, not int64 */
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
        npkdrv_complete(req.seq, 0);
    }
}
