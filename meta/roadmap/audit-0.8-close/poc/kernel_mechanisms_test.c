/*
 * kernel_mechanisms_test.c — validates the five kernel mechanisms the v3
 * driver-architecture plan depends on, on the actual deployment kernel.
 *
 * This is research/validation code, NOT part of any Nitpick artifact.
 * Build:  gcc -O2 -o kernel_mechanisms_test kernel_mechanisms_test.c
 *
 * Tests:
 *   1. memfd seals: F_SEAL_SHRINK blocks a hostile ftruncate from the peer
 *      process — and, without seals, the same truncate SIGBUSes the mapper.
 *   2. pidfd: pollable child-death notification, pidfd_send_signal,
 *      waitid(P_PIDFD) reaping, ESRCH after reap (no pid-reuse race).
 *   3. PR_SET_PDEATHSIG: driver dies when its parent process dies, even
 *      when the parent is SIGKILLed (the case failsafe cannot cover).
 *   4. MSG_NOSIGNAL: send() to a dead peer returns EPIPE instead of
 *      raising SIGPIPE; plain write() would raise SIGPIPE.
 *   5. Cross-process SPSC ring over memfd with C11 atomics
 *      (release-store / acquire-load pairing), plus latency measurements:
 *      socketpair round-trip vs futex-in-shm round-trip.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <linux/futex.h>

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   - %s\n", msg); } \
    else      { printf("  FAIL - %s (errno=%d %s)\n", msg, errno, strerror(errno)); fails++; } \
} while (0)

static int pidfd_open_(pid_t pid) { return (int)syscall(SYS_pidfd_open, pid, 0); }
static int pidfd_send_signal_(int pidfd, int sig) {
    return (int)syscall(SYS_pidfd_send_signal, pidfd, sig, NULL, 0);
}
static long futex_(uint32_t *uaddr, int op, uint32_t val,
                   const struct timespec *ts) {
    return syscall(SYS_futex, uaddr, op, val, ts, NULL, 0);
}
static double now_ns(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

/* ---- 1. memfd seals ----------------------------------------------------- */
static void test_memfd_seals(void) {
    printf("[1] memfd seals (the anti-SIGBUS defense)\n");
    const size_t SZ = 1 << 20;

    /* 1a: sealed fd — hostile shrink must fail with EPERM */
    int mfd = memfd_create("npk-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    CHECK(mfd >= 0, "memfd_create(MFD_ALLOW_SEALING)");
    CHECK(ftruncate(mfd, SZ) == 0, "ftruncate to 1MiB");
    uint8_t *map = mmap(NULL, SZ, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
    CHECK(map != MAP_FAILED, "parent mmap");
    int r = fcntl(mfd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL);
    CHECK(r == 0, "F_ADD_SEALS(SHRINK|GROW|SEAL)");

    pid_t pid = fork();
    if (pid == 0) { /* hostile driver: try to shrink the sealed region */
        int t = ftruncate(mfd, 0);
        _exit(t == 0 ? 1 : (errno == EPERM ? 0 : 2));
    }
    int st; waitpid(pid, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0,
          "peer ftruncate(0) on sealed memfd rejected with EPERM");
    map[SZ - 1] = 0x5A; /* parent access still safe */
    CHECK(map[SZ - 1] == 0x5A, "parent access after hostile shrink attempt");
    munmap(map, SZ); close(mfd);

    /* 1b: UNSEALED fd — the same attack SIGBUSes the mapper (in a child) */
    mfd = memfd_create("npk-shm-noseal", MFD_CLOEXEC);
    (void)!ftruncate(mfd, SZ);
    pid = fork();
    if (pid == 0) { /* victim maps, peer shrinks, victim touches -> SIGBUS */
        uint8_t *m = mmap(NULL, SZ, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
        if (m == MAP_FAILED) _exit(3);
        (void)!ftruncate(mfd, 0);            /* the hostile shrink */
        m[0] = 1;                            /* -> SIGBUS expected */
        _exit(4);                            /* reached only if no SIGBUS */
    }
    waitpid(pid, &st, 0);
    CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGBUS,
          "without seals, shrink + touch => SIGBUS (proves the hazard is real)");
    close(mfd);
}

/* ---- 2. pidfd ----------------------------------------------------------- */
static void test_pidfd(void) {
    printf("[2] pidfd lifecycle\n");
    pid_t pid = fork();
    if (pid == 0) { usleep(100 * 1000); _exit(0); }
    int pfd = pidfd_open_(pid);
    CHECK(pfd >= 0, "pidfd_open on live child");

    struct pollfd p = { .fd = pfd, .events = POLLIN };
    int pr = poll(&p, 1, 2000);
    CHECK(pr == 1 && (p.revents & POLLIN), "poll(pidfd) fires on child exit");

    siginfo_t si; memset(&si, 0, sizeof si);
    int wr = waitid(P_PIDFD, pfd, &si, WEXITED);
    CHECK(wr == 0 && si.si_pid == pid, "waitid(P_PIDFD) reaps via the pidfd");

    int sr = pidfd_send_signal_(pfd, 0);
    CHECK(sr == -1 && errno == ESRCH,
          "pidfd_send_signal after reap => ESRCH (no pid-reuse race)");
    close(pfd);

    /* kill path: SIGKILL through a pidfd on a live child */
    pid = fork();
    if (pid == 0) { pause(); _exit(0); }
    pfd = pidfd_open_(pid);
    sr = pidfd_send_signal_(pfd, SIGKILL);
    CHECK(sr == 0, "pidfd_send_signal(SIGKILL) on live child");
    waitid(P_PIDFD, pfd, &si, WEXITED);
    CHECK(si.si_code == CLD_KILLED && si.si_status == SIGKILL,
          "child observed as SIGKILLed");
    close(pfd);
}

/* ---- 3. PR_SET_PDEATHSIG ------------------------------------------------ */
static void test_pdeathsig(void) {
    printf("[3] PR_SET_PDEATHSIG (driver cannot outlive a SIGKILLed runtime)\n");
    int sv[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) == 0,
          "socketpair for grandchild-pid handoff");
    pid_t mid = fork();               /* 'mid' plays the Nitpick runtime */
    if (mid == 0) {
        pid_t drv = fork();           /* 'drv' plays the driver */
        if (drv == 0) {
            prctl(PR_SET_PDEATHSIG, SIGKILL);
            if (getppid() == 1) _exit(9);   /* parent already gone: bail */
            close(sv[0]); close(sv[1]);
            pause();                  /* wait to be killed by the kernel */
            _exit(0);
        }
        (void)!write(sv[1], &drv, sizeof drv);
        pause();                      /* runtime "hangs" until SIGKILLed */
        _exit(0);
    }
    pid_t drv = 0;
    (void)!read(sv[0], &drv, sizeof drv);
    usleep(50 * 1000);
    kill(mid, SIGKILL);               /* simulate runtime death w/o failsafe */
    int st; waitpid(mid, &st, 0);

    /* the driver is not our child; poll its existence via /proc */
    char path[64]; snprintf(path, sizeof path, "/proc/%d/stat", drv);
    int gone = 0;
    for (int i = 0; i < 200; i++) {   /* up to 2s */
        if (access(path, F_OK) != 0) { gone = 1; break; }
        /* still present may be a zombie reparented to init; check state */
        FILE *f = fopen(path, "r");
        if (f) {
            int p; char comm[64], stch;
            if (fscanf(f, "%d %63s %c", &p, comm, &stch) == 3 && stch == 'Z') {
                gone = 1; fclose(f); break;
            }
            fclose(f);
        }
        usleep(10 * 1000);
    }
    CHECK(gone, "driver killed by kernel when runtime died (no failsafe involved)");
    close(sv[0]); close(sv[1]);
}

/* ---- 4. MSG_NOSIGNAL ---------------------------------------------------- */
static void test_msg_nosignal(void) {
    printf("[4] MSG_NOSIGNAL on a dead control socket\n");
    int sv[2];
    (void)!socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
    close(sv[1]);                             /* peer (driver) is gone */

    ssize_t n = send(sv[0], "x", 1, MSG_NOSIGNAL);
    CHECK(n == -1 && errno == EPIPE,
          "send(MSG_NOSIGNAL) to dead peer => EPIPE, no signal");

    /* prove plain write() raises SIGPIPE (fatal by default) — in a child */
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGPIPE, SIG_DFL);
        (void)!write(sv[0], "x", 1);          /* -> SIGPIPE kills the child */
        _exit(5);
    }
    int st; waitpid(pid, &st, 0);
    CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGPIPE,
          "plain write() to dead peer => fatal SIGPIPE (proves the hazard)");
    close(sv[0]);
}

/* ---- 5. SPSC ring + latency --------------------------------------------- */
struct ring_hdr {                      /* one cache line per contended field */
    uint64_t magic;
    uint32_t capacity;                 /* power of two */
    uint32_t flags;
    uint8_t  pad0[48];
    _Atomic uint64_t head;             /* producer-owned, line 1 */
    uint8_t  pad1[56];
    _Atomic uint64_t tail;             /* consumer-owned, line 2 */
    uint8_t  pad2[56];
    _Atomic uint32_t doorbell;         /* futex word, line 3 */
    _Atomic uint32_t consumer_parked;
    uint8_t  pad3[56];
};                                     /* payload follows at offset 256 */

static void test_ring_and_latency(void) {
    printf("[5] cross-process SPSC ring + latency\n");
    const size_t CAP = 1 << 16;        /* 64 KiB payload ring */
    const size_t SZ = sizeof(struct ring_hdr) + CAP;
    const long   ITERS = 200000;

    int mfd = memfd_create("npk-ring", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    (void)!ftruncate(mfd, SZ);
    (void)!fcntl(mfd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL);
    struct ring_hdr *h = mmap(NULL, SZ, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
    uint8_t *payload = (uint8_t *)h + sizeof(struct ring_hdr);
    memset(h, 0, sizeof *h);
    h->magic = 0x4E504B42UL; h->capacity = (uint32_t)CAP;

    int sv[2];
    (void)!socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);

    pid_t pid = fork();
    if (pid == 0) {                    /* consumer / echo driver */
        close(sv[0]);
        /* 5a: drain ITERS bytes through the ring, verify sequence */
        uint64_t expect = 0; long bad = 0;
        for (long i = 0; i < ITERS; i++) {
            uint64_t head;
            while ((head = atomic_load_explicit(&h->head, memory_order_acquire))
                    == atomic_load_explicit(&h->tail, memory_order_relaxed)) {}
            uint64_t t = atomic_load_explicit(&h->tail, memory_order_relaxed);
            uint8_t v = payload[t & (CAP - 1)];
            if (v != (uint8_t)(expect & 0xFF)) bad++;
            expect++;
            atomic_store_explicit(&h->tail, t + 1, memory_order_release);
        }
        (void)!write(sv[1], &bad, sizeof bad);
        /* 5b: socketpair ping-pong echo */
        char c;
        for (long i = 0; i < ITERS / 4; i++) {
            if (read(sv[1], &c, 1) != 1) _exit(6);
            if (write(sv[1], &c, 1) != 1) _exit(7);
        }
        /* 5c: futex ping-pong: wait even->odd transitions */
        for (long i = 0; i < ITERS / 4; i++) {
            uint32_t v = 2 * (uint32_t)i + 1;
            while (atomic_load_explicit(&h->doorbell, memory_order_acquire) != v)
                futex_((uint32_t *)&h->doorbell, FUTEX_WAIT, v - 1, NULL);
            atomic_store_explicit(&h->doorbell, v + 1, memory_order_release);
            futex_((uint32_t *)&h->doorbell, FUTEX_WAKE, 1, NULL);
        }
        _exit(0);
    }
    close(sv[1]);

    /* 5a: producer — stream ITERS sequence bytes */
    double t0 = now_ns();
    for (long i = 0; i < ITERS; i++) {
        uint64_t head = atomic_load_explicit(&h->head, memory_order_relaxed);
        while (head - atomic_load_explicit(&h->tail, memory_order_acquire) >= CAP) {}
        payload[head & (CAP - 1)] = (uint8_t)(i & 0xFF);
        atomic_store_explicit(&h->head, head + 1, memory_order_release);
    }
    long bad = -1;
    (void)!read(sv[0], &bad, sizeof bad);
    double t1 = now_ns();
    CHECK(bad == 0, "SPSC ring: release/acquire pairing, zero corrupted slots");
    printf("       ring throughput: %.1f M items/s (1-byte items, spin consumer)\n",
           ITERS / ((t1 - t0) / 1e9) / 1e6);

    /* 5b: socketpair RTT */
    char c = 'p';
    t0 = now_ns();
    for (long i = 0; i < ITERS / 4; i++) {
        (void)!write(sv[0], &c, 1);
        (void)!read(sv[0], &c, 1);
    }
    t1 = now_ns();
    double sock_rtt = (t1 - t0) / (ITERS / 4);
    printf("       socketpair round-trip: %.0f ns (dispatch+complete floor)\n", sock_rtt);

    /* 5c: futex RTT */
    t0 = now_ns();
    for (long i = 0; i < ITERS / 4; i++) {
        uint32_t v = 2 * (uint32_t)i;
        atomic_store_explicit(&h->doorbell, v + 1, memory_order_release);
        futex_((uint32_t *)&h->doorbell, FUTEX_WAKE, 1, NULL);
        while (atomic_load_explicit(&h->doorbell, memory_order_acquire) != v + 2)
            futex_((uint32_t *)&h->doorbell, FUTEX_WAIT, v + 1, NULL);
    }
    t1 = now_ns();
    double futex_rtt = (t1 - t0) / (ITERS / 4);
    printf("       futex-in-shm round-trip: %.0f ns (%.1fx the socket cost)\n",
           futex_rtt, futex_rtt / sock_rtt);

    int st; waitpid(pid, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0, "consumer exited clean");
    munmap(h, SZ); close(mfd); close(sv[0]);
}

int main(void) {
    printf("kernel mechanism validation — uname: ");
    fflush(stdout);
    (void)!system("uname -r");
    test_memfd_seals();
    test_pidfd();
    test_pdeathsig();
    test_msg_nosignal();
    test_ring_and_latency();
    printf("%s (%d failures)\n", fails ? "RESULT: FAIL" : "RESULT: ALL PASS", fails);
    return fails ? 1 : 0;
}
