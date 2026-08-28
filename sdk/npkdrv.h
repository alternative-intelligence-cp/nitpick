/* npkdrv.h — the Nitpick Bridge driver SDK (D-149 over D-055; wire v3).
 *
 * THE CONTRACT IS THE PROTOCOL, NOT THIS FILE (D-149): a driver in any
 * language that speaks these bytes is conformant; this header is the C
 * rendering, grown from the deployment-kernel POC. A driver is a SEPARATE
 * SUPERVISED PROCESS — outside the Nitpick TCB, disposable, killed via
 * pidfd on any protocol violation, deadline, or runtime trap — and it must
 * uphold the §7.5 obligations this header's helpers implement:
 *
 *   1. exit on control-socket EOF/HUP (the universal orphan backstop);
 *   2. fstat the received memfd and use st_size (cross-check INIT_REQ);
 *   3. validate magic, version, capacity-vs-size before INIT_ACK;
 *   4. touch head/tail with C11 atomics (volatile is NOT synchronization);
 *   5. stderr is the diagnostic channel; stdout is not interpreted.
 *
 * FD MAP AT EXEC: 0 /dev/null, 1 /dev/null, 2 the stderr pipe, 3 the
 * control socket (arrives NONBLOCK; npkdrv_init flips it blocking, which
 * is the driver's own end and its own business).
 *
 * THE CONTROL PLANE (same-host byte order):
 *   packet header: u16 opcode | u16 flags | u32 length, then the payload.
 *   0x0001 INIT_REQ   B→D  {u32 version, u64 shm_size, u64 iface_hash}
 *                          + the sealed memfd via SCM_RIGHTS
 *   0x8001 INIT_ACK   D→B  {u32 version}
 *   0x0002 EXEC_NOTIFY B→D {u32 seq, u32 kernel_id}
 *   0x8002 WORK_COMPLETE D→B {u32 seq, u32 status}   status 0 = ok;
 *                          nonzero is a driver-reported error (the Bridge
 *                          fails EDriverError; stderr carries the detail)
 *   0x00FF SHUTDOWN   B→D  {} — advisory; the write-shutdown EOF follows
 *
 * THE DATA PLANE — the sealed shared region:
 *   0x00  u64 magic = 0x4E504B4452563033 ("NPKDRV03" as a number)
 *   0x08  u32 abi_version (3) | 0x0C u32 capacity (POWER OF TWO)
 *   0x10  u64 bulk_offset  | 0x18 u64 bulk_size
 *   0x40  _Atomic i64 head (Bridge-written, free-running)
 *   0x80  _Atomic i64 tail (driver-written, free-running)
 *   0x100 the descriptor ring: capacity × 32-byte records
 *         {u32 seq, u32 kernel_id, u64 arg_off, u64 arg_len, u64 resp_off}
 *         offsets are from the MAPPING BASE; args are packed 8-byte slots
 *         (scalars sign-extended; a byte-slice is {u64 rel_off_from_bulk,
 *         u64 len} in two slots, bytes after the slot area); the response
 *         value (8 bytes for a scalar return) goes at resp_off.
 *
 * THE INTERFACE HASH: FNV-1a-shaped, with NITPICK'S OWN offset basis
 * 0xCBF5DAE484222325 — the D-179 error-identity seed, NOT the textbook
 * 0xCBF29CE484222325 (one constant for every derived identity in the
 * ecosystem; transcribe it exactly) — and prime 0x100000001B3,
 * over each method's canonical spelling `name=ret(p1,p2,...)` — ret/params
 * spelled `NIL`, `int32`, `int64`, `int8[]`, `uint8[]`; the Bridge receiver
 * and Duration deadline are STRUCTURAL and excluded — folding a ';' (0x3B)
 * after every method, in declaration order (= the kernel_id space). A
 * driver states its table's spellings, computes the same number, and
 * refuses a mismatched INIT_REQ before ACKing: a stale driver fails the
 * handshake loudly instead of confusing types silently.
 */
#ifndef NPKDRV_H
#define NPKDRV_H

#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define NPKDRV_MAGIC   0x4E504B4452563033ULL
#define NPKDRV_VERSION 3u
#define NPKDRV_CTRL_FD 3

#define NPKDRV_OP_INIT_REQ      0x0001u
#define NPKDRV_OP_INIT_ACK      0x8001u
#define NPKDRV_OP_EXEC_NOTIFY   0x0002u
#define NPKDRV_OP_WORK_COMPLETE 0x8002u
#define NPKDRV_OP_SHUTDOWN      0x00FFu

typedef struct {
    uint32_t seq;
    uint32_t kernel_id;
    uint64_t arg_off;
    uint64_t arg_len;
    uint64_t resp_off;
} npkdrv_desc;

typedef struct {
    uint8_t     *shm;
    uint64_t     shm_size;
    uint32_t     capacity;
    uint64_t     bulk_offset;
    uint64_t     bulk_size;
    _Atomic int64_t *head;
    _Atomic int64_t *tail;
} npkdrv;

static uint64_t npkdrv_fnv_str(const char *s, uint64_t h) {
    for (; *s; s++) { h = (h ^ (uint64_t)(uint8_t)*s) * 0x100000001B3ULL; }
    return h;
}

/* The interface hash over a NULL-terminated array of canonical spellings,
 * declaration order. */
static uint64_t npkdrv_iface_hash(const char *const *canon) {
    uint64_t h = 0xCBF5DAE484222325ULL;
    for (; *canon; canon++) {
        h = npkdrv_fnv_str(*canon, h);
        h = (h ^ 0x3BULL) * 0x100000001B3ULL;   /* ';' between methods */
    }
    return h;
}

static int npkdrv_read_exact(int fd, void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r == 0) return 0;                    /* EOF: shutdown */
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        p += r; n -= (size_t)r;
    }
    return 1;
}

static int npkdrv_write_exact(int fd, const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *)buf;
    while (n) {
        ssize_t r = write(fd, p, n);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        p += r; n -= (size_t)r;
    }
    return 1;
}

/* Handshake per §7.5. `expect_hash` is the driver's own table hash
 * (npkdrv_iface_hash), or 0 to accept any interface (a generic tool).
 * Returns 0 on success; on any failure it has already written the reason
 * to stderr, and the caller exits nonzero. */
static int npkdrv_init(npkdrv *d, uint64_t expect_hash) {
    /* our end, our business: the socket arrives NONBLOCK; this loop wants
     * blocking reads */
    int fl = fcntl(NPKDRV_CTRL_FD, F_GETFL, 0);
    if (fl >= 0) fcntl(NPKDRV_CTRL_FD, F_SETFL, fl & ~O_NONBLOCK);

    uint8_t pkt[28];
    struct iovec iov = { pkt, sizeof pkt };
    union { struct cmsghdr c; uint8_t buf[CMSG_SPACE(sizeof(int))]; } ctl;
    struct msghdr mh; memset(&mh, 0, sizeof mh);
    mh.msg_iov = &iov; mh.msg_iovlen = 1;
    mh.msg_control = ctl.buf; mh.msg_controllen = sizeof ctl.buf;
    ssize_t n = recvmsg(NPKDRV_CTRL_FD, &mh, 0);
    if (n != 28) { fprintf(stderr, "npkdrv: short INIT_REQ (%zd)\n", n); return -1; }
    if (mh.msg_flags & MSG_CTRUNC) { fprintf(stderr, "npkdrv: control truncated\n"); return -1; }

    uint16_t op;  memcpy(&op, pkt, 2);
    uint32_t len; memcpy(&len, pkt + 4, 4);
    if (op != NPKDRV_OP_INIT_REQ || len != 20) {
        fprintf(stderr, "npkdrv: bad INIT_REQ header\n"); return -1;
    }
    uint32_t ver; memcpy(&ver, pkt + 8, 4);
    uint64_t shm_size, hash;
    memcpy(&shm_size, pkt + 12, 8);
    memcpy(&hash, pkt + 20, 8);
    if (ver != NPKDRV_VERSION) { fprintf(stderr, "npkdrv: version %u\n", ver); return -1; }
    if (expect_hash && hash && hash != expect_hash) {
        fprintf(stderr, "npkdrv: interface hash mismatch (built %016llx, offered %016llx)\n",
                (unsigned long long)expect_hash, (unsigned long long)hash);
        return -1;
    }

    int shm_fd = -1;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) {
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
            memcpy(&shm_fd, CMSG_DATA(c), sizeof shm_fd);
        }
    }
    if (shm_fd < 0) { fprintf(stderr, "npkdrv: no memfd rode INIT_REQ\n"); return -1; }

    struct stat st;
    if (fstat(shm_fd, &st) != 0 || (uint64_t)st.st_size != shm_size) {
        fprintf(stderr, "npkdrv: st_size disagrees with INIT_REQ\n"); return -1;
    }
    void *m = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (m == MAP_FAILED) { fprintf(stderr, "npkdrv: mmap failed\n"); return -1; }
    d->shm = (uint8_t *)m;
    d->shm_size = shm_size;

    uint64_t magic; memcpy(&magic, d->shm, 8);
    uint32_t aver;  memcpy(&aver, d->shm + 8, 4);
    memcpy(&d->capacity, d->shm + 12, 4);
    memcpy(&d->bulk_offset, d->shm + 16, 8);
    memcpy(&d->bulk_size, d->shm + 24, 8);
    if (magic != NPKDRV_MAGIC || aver != NPKDRV_VERSION ||
        d->capacity == 0 || (d->capacity & (d->capacity - 1)) ||
        256 + (uint64_t)d->capacity * 32 > shm_size) {
        fprintf(stderr, "npkdrv: bad ring header\n"); return -1;
    }
    d->head = (_Atomic int64_t *)(d->shm + 0x40);
    d->tail = (_Atomic int64_t *)(d->shm + 0x80);

    uint8_t ack[12];
    uint64_t hdr = (uint64_t)NPKDRV_OP_INIT_ACK | (4ULL << 32);
    memcpy(ack, &hdr, 8);
    uint32_t v = NPKDRV_VERSION; memcpy(ack + 8, &v, 4);
    if (npkdrv_write_exact(NPKDRV_CTRL_FD, ack, sizeof ack) != 1) {
        fprintf(stderr, "npkdrv: INIT_ACK write failed\n"); return -1;
    }
    return 0;
}

/* Block for the next request. Returns 1 with *out filled, 0 on shutdown
 * (EOF or SHUTDOWN packet — exit cleanly), -1 on a wire defect (exit
 * nonzero; the Bridge kills on its side of any disagreement anyway). */
static int npkdrv_next(npkdrv *d, npkdrv_desc *out) {
    uint8_t pkt[16];
    int r = npkdrv_read_exact(NPKDRV_CTRL_FD, pkt, sizeof pkt);
    if (r == 0) return 0;
    if (r < 0) return -1;
    uint16_t op;  memcpy(&op, pkt, 2);
    if (op == NPKDRV_OP_SHUTDOWN) return 0;
    uint32_t len; memcpy(&len, pkt + 4, 4);
    if (op != NPKDRV_OP_EXEC_NOTIFY || len != 8) return -1;
    uint32_t seq, kern;
    memcpy(&seq, pkt + 8, 4);
    memcpy(&kern, pkt + 12, 4);

    int64_t tail = atomic_load_explicit(d->tail, memory_order_acquire);
    int64_t head = atomic_load_explicit(d->head, memory_order_acquire);
    if (tail >= head) return -1;
    const uint8_t *desc = d->shm + 256 +
        ((uint64_t)(tail & (int64_t)(d->capacity - 1)) * 32);
    memcpy(out, desc, sizeof *out);
    if (out->seq != seq || out->kernel_id != kern) return -1;
    atomic_store_explicit(d->tail, tail + 1, memory_order_release);
    return 1;
}

static int npkdrv_complete(uint32_t seq, uint32_t status) {
    uint8_t pkt[16];
    uint64_t hdr = (uint64_t)NPKDRV_OP_WORK_COMPLETE | (8ULL << 32);
    memcpy(pkt, &hdr, 8);
    memcpy(pkt + 8, &seq, 4);
    memcpy(pkt + 12, &status, 4);
    return npkdrv_write_exact(NPKDRV_CTRL_FD, pkt, sizeof pkt) == 1 ? 0 : -1;
}

#endif /* NPKDRV_H */
