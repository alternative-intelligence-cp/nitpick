; The Nitpick bootstrap runtime floor.
;
; THROWAWAY, alongside the seed (D-085). Hand-written LLVM IR, which is what
; D-015 specifies for the first rung: runtime symbols start as hand-written IR
; and are replaced at a later rung. Real allocation and real I/O arrive with
; nlibc in cycle 0.8.
;
; Freestanding: no libc, no crt. `_start` is provided here, and every kernel
; call is a raw syscall.

target triple = "x86_64-unknown-linux-gnu"

; ---------------------------------------------------------------------------
; Entry point.
;
; Written as module-level assembly rather than as an LLVM function on purpose:
; at _start the stack is NOT laid out the way it is at a normal call -- there is
; no return address -- so a compiler-generated prologue can leave %rsp
; misaligned and any later SSE spill faults. Aligning by hand sidesteps the
; whole class.
; ---------------------------------------------------------------------------

module asm ".globl _start"
module asm "_start:"
module asm "  xorq %rbp, %rbp"
module asm "  movq %rsp, %rdi"         ; the ORIGINAL rsp, before alignment
module asm "  andq $-16, %rsp"
module asm "  callq npk_start"
module asm "  hlt"

; ---------------------------------------------------------------------------
; The process boundary.
;
; `main` takes `cstring[]:argv` and nothing else (D-089): a slice carries its
; length (D-070), so there is no argc to pass and no second copy of a fact to
; disagree with the first.
;
; At _start the stack holds argc, then argv[0..argc-1], then a NULL, then envp.
; That layout is only reachable through the ORIGINAL %rsp, which is why the
; prologue above captures it into %rdi BEFORE the alignment that destroys it.
;
; This runs before `main`, so it allocates the cstring array from the bump
; allocator and never frees it -- which is what the allocator does anyway, and
; argv outlives everything regardless.
; ---------------------------------------------------------------------------

declare i32 @main({ ptr, i64 })

define internal void @npk_start(i64 %sp) noreturn {
entry:
  %spp = inttoptr i64 %sp to ptr
  %argc = load i64, ptr %spp
  %argvp = getelementptr i8, ptr %spp, i64 8      ; &argv[0]
  %bytes = mul i64 %argc, 16                      ; sizeof(cstring) = {ptr,i64}
  %buf = call ptr @npk_alloc(i64 %bytes)
  br label %loop

loop:                                             ; preds = %entry, %next
  %i = phi i64 [ 0, %entry ], [ %i1, %next ]
  %done = icmp uge i64 %i, %argc
  br i1 %done, label %ready, label %body

body:                                             ; preds = %loop
  %slotp = getelementptr ptr, ptr %argvp, i64 %i
  %s = load ptr, ptr %slotp
  br label %slen

; A cstring carries its length (D-049), and the kernel hands over a bare
; NUL-terminated pointer -- so the length is measured exactly once, here, at the
; boundary. Nothing downstream scans for a NUL again.
slen:                                             ; preds = %body, %slen
  %k = phi i64 [ 0, %body ], [ %k1, %slen ]
  %cp = getelementptr i8, ptr %s, i64 %k
  %ch = load i8, ptr %cp
  %k1 = add i64 %k, 1
  %atnul = icmp eq i8 %ch, 0
  br i1 %atnul, label %store, label %slen

store:                                            ; preds = %slen
  %pf = getelementptr { ptr, i64 }, ptr %buf, i64 %i, i32 0
  store ptr %s, ptr %pf
  %lf = getelementptr { ptr, i64 }, ptr %buf, i64 %i, i32 1
  store i64 %k, ptr %lf
  br label %next

next:                                             ; preds = %store
  %i1 = add i64 %i, 1
  br label %loop

ready:                                            ; preds = %loop
  %sl0 = insertvalue { ptr, i64 } undef, ptr %buf, 0
  %sl1 = insertvalue { ptr, i64 } %sl0, i64 %argc, 1
  %rc = call i32 @main({ ptr, i64 } %sl1)
  call void @npk_exit(i32 %rc)
  unreachable
}

; ---------------------------------------------------------------------------
; Raw syscall
; ---------------------------------------------------------------------------

define internal i64 @npk_sys6(i64 %nr, i64 %a1, i64 %a2, i64 %a3,
                              i64 %a4, i64 %a5, i64 %a6) {
  %r = call i64 asm sideeffect "syscall",
       "={ax},{ax},{di},{si},{dx},{r10},{r8},{r9},~{rcx},~{r11},~{memory},~{dirflag},~{fpsr},~{flags}"
       (i64 %nr, i64 %a1, i64 %a2, i64 %a3, i64 %a4, i64 %a5, i64 %a6)
  ret i64 %r
}

define void @npk_exit(i32 %code) noreturn {
  %c = sext i32 %code to i64
  %r = call i64 @npk_sys6(i64 60, i64 %c, i64 0, i64 0, i64 0, i64 0, i64 0)
  unreachable
}

define i64 @npk_write_raw(i32 %fd, ptr %buf, i64 %len) {
  %f = sext i32 %fd to i64
  %p = ptrtoint ptr %buf to i64
  %r = call i64 @npk_sys6(i64 1, i64 %f, i64 %p, i64 %len, i64 0, i64 0, i64 0)
  ret i64 %r
}

; ---------------------------------------------------------------------------
; string -> cstring, and reading a file by path.
;
; `to_cstring` FAILS ON AN INTERIOR NUL, and that is the entire point of D-049.
; A Nitpick `string` is {ptr, len, cap} and length-carrying, so an embedded 0u8
; is just a byte. Converting such a string by copying up to the first NUL
; silently truncates it, and the caller has no indication -- so a path that was
; validated and a path that was opened become DIFFERENT PATHS.
;
;     attacker supplies:   "avatar.png\0.sh"
;     validator sees:      len 14, suffix ".sh"
;     kernel sees:         "avatar.png"
;
; That is the poison-NUL class, and it is why `read_file` takes a cstring rather
; than accepting a string and terminating it itself. The conversion is explicit,
; fallible, and the caller handles the failure like any other.
;
; Error codes: 22 is EINVAL, used for the interior NUL. read_file returns the
; POSITIVE errno from the failing syscall, so a caller can tell ENOENT (2) from
; EACCES (13) without a second mechanism.
; ---------------------------------------------------------------------------

define { { ptr, i64 }, i32 } @npk_to_cstring({ ptr, i64, i64 } %s) {
entry:
  %p = extractvalue { ptr, i64, i64 } %s, 0
  %n = extractvalue { ptr, i64, i64 } %s, 1
  br label %scan

scan:                                     ; preds = %entry, %step
  %i = phi i64 [ 0, %entry ], [ %i1, %step ]
  %done = icmp uge i64 %i, %n
  br i1 %done, label %copy, label %check

check:                                    ; preds = %scan
  %cp = getelementptr i8, ptr %p, i64 %i
  %ch = load i8, ptr %cp
  %isnul = icmp eq i8 %ch, 0
  br i1 %isnul, label %interior, label %step

step:                                     ; preds = %check
  %i1 = add i64 %i, 1
  br label %scan

copy:                                     ; preds = %scan
  ; One byte more than the length, for the terminator the kernel needs.
  %sz = add i64 %n, 1
  %buf = call ptr @npk_alloc(i64 %sz)
  call ptr @memcpy(ptr %buf, ptr %p, i64 %n)
  %end = getelementptr i8, ptr %buf, i64 %n
  store i8 0, ptr %end
  %c0 = insertvalue { ptr, i64 } undef, ptr %buf, 0
  %c1 = insertvalue { ptr, i64 } %c0, i64 %n, 1
  %r0 = insertvalue { { ptr, i64 }, i32 } undef, { ptr, i64 } %c1, 0
  %r1 = insertvalue { { ptr, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64 }, i32 } %r1

interior:                                 ; preds = %check
  %e0 = insertvalue { ptr, i64 } undef, ptr null, 0
  %e1 = insertvalue { ptr, i64 } %e0, i64 0, 1
  %q0 = insertvalue { { ptr, i64 }, i32 } undef, { ptr, i64 } %e1, 0
  %q1 = insertvalue { { ptr, i64 }, i32 } %q0, i32 22, 1
  ret { { ptr, i64 }, i32 } %q1
}

; Can this path be opened for reading?
;
; Ambiguity-is-an-error (BUILD_REFERENCE section 3) means the resolver must probe
; EVERY dependency root before deciding, not stop at the first hit -- a resolver
; that returns early cannot report the second candidate. Probing by full read
; would be O(file size) per root, and most probes miss.
;
; It tests READABILITY rather than existence, which is the question actually
; being asked: a file that exists and cannot be opened is not a candidate.
define i8 @npk_path_exists({ ptr, i64 } %path) {
entry:
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %fd, 0
  br i1 %bad, label %no, label %yes

yes:
  %cr = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret i8 1

no:
  ret i8 0
}

; Write a whole buffer to a path, replacing what was there -- read_file's
; mirror, and the routine `npkc -o` stands on (0.8.3). openat/write/close live
; INSIDE it as one auditable body rather than as three exposed symbols; the
; fd-granular primitives arrive with D-050's line discipline, where a held-open
; descriptor is the point.
;
; A SHORT WRITE IS NOT FAILURE. write(2) may take fewer bytes than offered --
; signals, pipes, quotas -- so the loop advances by what was ACCEPTED and only a
; negative return is an error. Treating a short write as success is the classic
; way to truncate a file and report victory; the error code travels out as the
; Result's tbb32.
; Byte equality over two strings -- the primitive the comptime folder mirrors
; (fold_string_builtin) and the one string predicate worth a runtime symbol:
; everything nlibc builds (contains, starts_with, index_of) is ordinary Nitpick
; over byte access, but equality is called from generated code paths where a
; call beats an inlined loop for auditability.
define i8 @npk_string_equals({ ptr, i64, i64 } %a, { ptr, i64, i64 } %b) {
entry:
  %al = extractvalue { ptr, i64, i64 } %a, 1
  %bl = extractvalue { ptr, i64, i64 } %b, 1
  %same = icmp eq i64 %al, %bl
  br i1 %same, label %scan, label %no

scan:
  %ap = extractvalue { ptr, i64, i64 } %a, 0
  %bp = extractvalue { ptr, i64, i64 } %b, 0
  br label %loop

loop:
  %i = phi i64 [ 0, %scan ], [ %i2, %next ]
  %done = icmp eq i64 %i, %al
  br i1 %done, label %yes, label %cmp

cmp:
  %pa = getelementptr i8, ptr %ap, i64 %i
  %pb = getelementptr i8, ptr %bp, i64 %i
  %ca = load i8, ptr %pa
  %cb = load i8, ptr %pb
  %eq = icmp eq i8 %ca, %cb
  br i1 %eq, label %next, label %no

next:
  %i2 = add i64 %i, 1
  br label %loop

yes:
  ret i8 1

no:
  ret i8 0
}

define { i32 } @npk_write_file({ ptr, i64 } %path, { ptr, i64, i64 } %data) {
entry:
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  ; openat(AT_FDCWD, path, O_WRONLY|O_CREAT|O_TRUNC, 0644). 577 = 1|64|512.
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 577, i64 420, i64 0, i64 0)
  %obad = icmp slt i64 %fd, 0
  br i1 %obad, label %openfail, label %wstart

wstart:
  %base = extractvalue { ptr, i64, i64 } %data, 0
  %total = extractvalue { ptr, i64, i64 } %data, 1
  br label %wloop

wloop:
  %off = phi i64 [ 0, %wstart ], [ %off2, %wnext ]
  %left = sub i64 %total, %off
  %done = icmp eq i64 %left, 0
  br i1 %done, label %wclose, label %wone

wone:
  %at = getelementptr i8, ptr %base, i64 %off
  %ati = ptrtoint ptr %at to i64
  %n = call i64 @npk_sys6(i64 1, i64 %fd, i64 %ati, i64 %left, i64 0, i64 0, i64 0)
  %wbad = icmp slt i64 %n, 0
  br i1 %wbad, label %writefail, label %wnext

wnext:
  %off2 = add i64 %off, %n
  br label %wloop

wclose:
  %c = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %cbad = icmp slt i64 %c, 0
  br i1 %cbad, label %closefail, label %ok

ok:
  ret { i32 } zeroinitializer

openfail:
  %oe = sub i64 0, %fd
  %oec = trunc i64 %oe to i32
  %or0 = insertvalue { i32 } undef, i32 %oec, 0
  ret { i32 } %or0

writefail:
  ; close best-effort: the write's errno is the story, not the close's.
  %ce = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %we = sub i64 0, %n
  %wec = trunc i64 %we to i32
  %wr0 = insertvalue { i32 } undef, i32 %wec, 0
  ret { i32 } %wr0

closefail:
  ; A FAILED CLOSE IS A FAILED WRITE. Buffered-at-the-kernel errors surface
  ; here, and reporting success past one is reporting bytes that may not exist.
  %le = sub i64 0, %c
  %lec = trunc i64 %le to i32
  %lr0 = insertvalue { i32 } undef, i32 %lec, 0
  ret { i32 } %lr0
}

define { { ptr, i64, i64 }, i32 } @npk_read_file({ ptr, i64 } %path) {
entry:
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  ; openat(AT_FDCWD, path, O_RDONLY, 0). AT_FDCWD is -100.
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %fd, 0
  br i1 %bad, label %openfail, label %start

start:                                    ; preds = %entry
  %buf0 = call ptr @npk_alloc(i64 65536)
  br label %loop

loop:                                     ; preds = %start, %iter
  %buf = phi ptr [ %buf0, %start ], [ %rbuf, %iter ]
  %cap = phi i64 [ 65536, %start ], [ %rcap, %iter ]
  %len = phi i64 [ 0, %start ], [ %len_n, %iter ]
  %room = sub i64 %cap, %len
  %full = icmp eq i64 %room, 0
  br i1 %full, label %grow, label %read

grow:                                     ; preds = %loop
  %cap2 = mul i64 %cap, 2
  %buf2 = call ptr @npk_alloc(i64 %cap2)
  call ptr @memcpy(ptr %buf2, ptr %buf, i64 %len)
  br label %read

read:                                     ; preds = %loop, %grow
  %rbuf = phi ptr [ %buf, %loop ], [ %buf2, %grow ]
  %rcap = phi i64 [ %cap, %loop ], [ %cap2, %grow ]
  %rroom = sub i64 %rcap, %len
  %dst = getelementptr i8, ptr %rbuf, i64 %len
  %dsti = ptrtoint ptr %dst to i64
  %n = call i64 @npk_sys6(i64 0, i64 %fd, i64 %dsti, i64 %rroom, i64 0, i64 0, i64 0)
  %rbad = icmp slt i64 %n, 0
  br i1 %rbad, label %readfail, label %check

check:                                    ; preds = %read
  ; A SHORT READ IS NOT END OF FILE. Treating it as one is the classic way to
  ; truncate a file silently, and here it would mean parsing a prefix of a module
  ; and reporting success.
  %eof = icmp eq i64 %n, 0
  br i1 %eof, label %done, label %iter

iter:                                     ; preds = %check
  %len_n = add i64 %len, %n
  br label %loop

done:                                     ; preds = %check
  %cr = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %rbuf, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %len, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %rcap, 2
  %o0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %o1 = insertvalue { { ptr, i64, i64 }, i32 } %o0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %o1

readfail:                                 ; preds = %read
  ; Close before returning. The process is about to report an error, not exit,
  ; and a driver that reports many missing files must not leak a descriptor each
  ; time it succeeds partway.
  %cr2 = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %rerr = sub i64 0, %n
  %rerr32 = trunc i64 %rerr to i32
  br label %fail

openfail:                                 ; preds = %entry
  %oerr = sub i64 0, %fd
  %oerr32 = trunc i64 %oerr to i32
  br label %fail

fail:                                     ; preds = %readfail, %openfail
  %code = phi i32 [ %rerr32, %readfail ], [ %oerr32, %openfail ]
  %f0 = insertvalue { ptr, i64, i64 } undef, ptr null, 0
  %f1 = insertvalue { ptr, i64, i64 } %f0, i64 0, 1
  %f2 = insertvalue { ptr, i64, i64 } %f1, i64 0, 2
  %g0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %f2, 0
  %g1 = insertvalue { { ptr, i64, i64 }, i32 } %g0, i32 %code, 1
  ret { { ptr, i64, i64 }, i32 } %g1
}

; ---------------------------------------------------------------------------
; Reading standard input, whole.
;
; The frontend needs to run on REAL FILES for the rejection suite to mean what
; D-085 says it means -- every file there must PARSE and be refused later, and
; only the real parser can demonstrate that. Reading fd 0 rather than opening a
; path is deliberate: it needs no argv (which `_start` does not pass, and which
; would raise the separate question of what `main`'s signature is), and it does
; not pre-commit the file-opening API that cycle 0.3's module loader will want.
;
; SUBSET_1 section 1.6 has listed open/read/close in the runtime floor since 0.0.4
; while none of them existed, because nothing had needed one yet. This is the
; first, and the entry there now says which.
;
; Grows by doubling and reads until read(2) returns 0. A short read is NOT
; end-of-file -- treating it as one is the classic way to silently truncate a
; file, and here it would mean parsing a prefix of a test and reporting success.
; ---------------------------------------------------------------------------

define { { ptr, i64, i64 }, i32 } @npk_read_stdin() {
entry:
  %buf0 = call ptr @npk_alloc(i64 65536)
  br label %loop

loop:                                     ; preds = %entry, %iter
  %buf = phi ptr [ %buf0, %entry ], [ %rbuf, %iter ]
  %cap = phi i64 [ 65536, %entry ], [ %rcap, %iter ]
  %len = phi i64 [ 0, %entry ], [ %len_n, %iter ]
  %room = sub i64 %cap, %len
  %full = icmp eq i64 %room, 0
  br i1 %full, label %grow, label %read

grow:                                     ; preds = %loop
  ; The bump allocator never frees, so growing copies into a fresh block and
  ; abandons the old one. That is the trade the allocator's own comment already
  ; accepted: this process runs once and exits.
  %cap2 = mul i64 %cap, 2
  %buf2 = call ptr @npk_alloc(i64 %cap2)
  call ptr @memcpy(ptr %buf2, ptr %buf, i64 %len)
  br label %read

read:                                     ; preds = %loop, %grow
  %rbuf = phi ptr [ %buf, %loop ], [ %buf2, %grow ]
  %rcap = phi i64 [ %cap, %loop ], [ %cap2, %grow ]
  %rroom = sub i64 %rcap, %len
  %dst = getelementptr i8, ptr %rbuf, i64 %len
  %dsti = ptrtoint ptr %dst to i64
  %n = call i64 @npk_sys6(i64 0, i64 0, i64 %dsti, i64 %rroom, i64 0, i64 0, i64 0)
  %failed = icmp slt i64 %n, 0
  br i1 %failed, label %err, label %check

check:                                    ; preds = %read
  %eof = icmp eq i64 %n, 0
  br i1 %eof, label %done, label %iter

iter:                                     ; preds = %check
  %len_n = add i64 %len, %n
  br label %loop

done:                                     ; preds = %check
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %rbuf, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %len, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %rcap, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1

err:
  ; An errored Result carries a zeroed value, so a caller that unwraps without
  ; checking gets an empty string rather than a pointer into nothing.
  %e0 = insertvalue { ptr, i64, i64 } undef, ptr null, 0
  %e1 = insertvalue { ptr, i64, i64 } %e0, i64 0, 1
  %e2 = insertvalue { ptr, i64, i64 } %e1, i64 0, 2
  %q0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %e2, 0
  %q1 = insertvalue { { ptr, i64, i64 }, i32 } %q0, i32 5, 1
  ret { { ptr, i64, i64 }, i32 } %q1
}

; ---------------------------------------------------------------------------
; Allocation: a bump allocator that never frees.
;
; This is CORRECT here, not a shortcut. The compiler is a process that runs once
; and exits, so reclamation buys nothing -- and an allocator is exactly the kind
; of subtle code that must not live in the least-audited artifact in the chain.
; dalloc is therefore a no-op, and our sources still write `defer { dalloc(p); }`
; so they stay correct when real allocation lands.
;
; EVERY BLOCK CARRIES ITS SIZE IN SIXTEEN BYTES IN FRONT OF IT (0.7.3), which is
; the whole reason ralloc can be correct. Without it, ralloc had no way to know how
; much of the old block was real and copied the NEW size out of it -- an
; out-of-bounds read that leaves the mapping entirely when the old block sits near
; the end of a chunk, and takes the process down with SIGBUS.
;
; IT WAS NOT A LATENT BUG. About 511 declarations in one file is enough to reach a
; doubling that crosses a chunk boundary, and FIVE OF THE COMPILER'S OWN SOURCES
; exceed it -- so the compiler could not parse itself, and nothing said so because
; the harness had never asked it to.
;
; The alternative was to pass the old size in at every call site, which every
; caller does know. It was rejected: that moves a memory-safety obligation onto
; twenty call sites, and the one that forgets is silent corruption rather than a
; compile error. A header costs sixteen bytes per allocation in a process that runs
; once and exits.
; ---------------------------------------------------------------------------

@npk_cur = internal global i64 0
@npk_end = internal global i64 0

define ptr @npk_alloc(i64 %n) {
entry:
  ; Round the request up to 16 bytes so every allocation is aligned, then take one
  ; more 16-byte slot in front for the size. Sixteen and not eight: the header has
  ; to keep the returned pointer 16-aligned, which is what every caller's struct
  ; array depends on.
  %a = add i64 %n, 15
  %szb = and i64 %a, -16
  %sz = add i64 %szb, 16
  %cur = load i64, ptr @npk_cur
  %new = add i64 %cur, %sz
  %end = load i64, ptr @npk_end
  %fits = icmp ule i64 %new, %end
  %live = icmp ne i64 %cur, 0
  %ok = and i1 %fits, %live
  br i1 %ok, label %bump, label %grow

bump:
  store i64 %new, ptr @npk_cur
  %hdr1 = inttoptr i64 %cur to ptr
  store i64 %szb, ptr %hdr1
  %u1 = add i64 %cur, 16
  %p1 = inttoptr i64 %u1 to ptr
  ret ptr %p1

grow:
  ; take at least 1 MiB, or the whole request rounded to a page if larger
  %need = add i64 %sz, 4095
  %needp = and i64 %need, -4096
  %big = icmp ugt i64 %needp, 1048576
  %chunk = select i1 %big, i64 %needp, i64 1048576
  ; mmap(NULL, chunk, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
  %m = call i64 @npk_sys6(i64 9, i64 0, i64 %chunk, i64 3, i64 34, i64 -1, i64 0)
  ; a syscall error is a small negative value returned as a large unsigned one
  %bad = icmp ugt i64 %m, -4096
  br i1 %bad, label %oom, label %fresh

fresh:
  %nend = add i64 %m, %chunk
  store i64 %nend, ptr @npk_end
  %ncur = add i64 %m, %sz
  store i64 %ncur, ptr @npk_cur
  %hdr2 = inttoptr i64 %m to ptr
  store i64 %szb, ptr %hdr2
  %u2 = add i64 %m, 16
  %p2 = inttoptr i64 %u2 to ptr
  ret ptr %p2

oom:
  ; Out of memory is unrecoverable here. The real runtime routes this through
  ; failsafe; the seed has no allocation to preallocate for, so it stops.
  call void @npk_exit(i32 70)
  unreachable
}

define ptr @npk_calloc(i64 %count, i64 %size) {
  %n = mul i64 %count, %size
  %p = call ptr @npk_alloc(i64 %n)
  call void @llvm.memset.p0.i64(ptr %p, i8 0, i64 %n, i1 false)
  ret ptr %p
}

define ptr @npk_ralloc(ptr %old, i64 %n) {
entry:
  ; Never freeing means realloc is always a fresh block plus a copy. Correct,
  ; and wasteful in a way that does not matter for a process that exits.
  ;
  ; THE COPY IS BOUNDED BY THE OLD BLOCK, not by the new size. Copying `n` bytes
  ; out of a block that is `old` bytes long reads past its end, and when the block
  ; sits near the end of a chunk that read leaves the mapping -- SIGBUS, in the
  ; least-audited artifact in the chain, on every program large enough to grow an
  ; array twice.
  %isnull = icmp eq ptr %old, null
  br i1 %isnull, label %plain, label %copy

plain:
  ; A null old block is a first allocation, and has no header to read.
  %f = call ptr @npk_alloc(i64 %n)
  ret ptr %f

copy:
  %p = call ptr @npk_alloc(i64 %n)
  %oi = ptrtoint ptr %old to i64
  %hi = sub i64 %oi, 16
  %hp = inttoptr i64 %hi to ptr
  %osz = load i64, ptr %hp
  %smaller = icmp ult i64 %osz, %n
  %cnt = select i1 %smaller, i64 %osz, i64 %n
  call void @llvm.memcpy.p0.p0.i64(ptr %p, ptr %old, i64 %cnt, i1 false)
  ret ptr %p
}

define void @npk_dalloc(ptr %p) {
  ret void
}

; ---------------------------------------------------------------------------
; The symbols LLVM emits calls to. With no libc these must exist.
; ---------------------------------------------------------------------------

define ptr @memcpy(ptr %dst, ptr %src, i64 %n) {
entry:
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %body ]
  %more = icmp ult i64 %i, %n
  br i1 %more, label %body, label %done
body:
  %sp = getelementptr i8, ptr %src, i64 %i
  %dp = getelementptr i8, ptr %dst, i64 %i
  %b = load i8, ptr %sp
  store i8 %b, ptr %dp
  %i2 = add i64 %i, 1
  br label %head
done:
  ret ptr %dst
}

define ptr @memset(ptr %dst, i32 %c, i64 %n) {
entry:
  %b = trunc i32 %c to i8
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %body ]
  %more = icmp ult i64 %i, %n
  br i1 %more, label %body, label %done
body:
  %dp = getelementptr i8, ptr %dst, i64 %i
  store i8 %b, ptr %dp
  %i2 = add i64 %i, 1
  br label %head
done:
  ret ptr %dst
}

define ptr @memmove(ptr %dst, ptr %src, i64 %n) {
  %r = call ptr @memcpy(ptr %dst, ptr %src, i64 %n)
  ret ptr %r
}

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)

; ---------------------------------------------------------------------------
; Strings.
;
; string is {ptr, i64 len, i64 cap} and these return Result<string>, which is
; { {ptr,i64,i64}, i32 } -- the error field is tbb32, zero meaning success
; (D-069). The seed's checker types them exactly this way; the two must agree or
; llc rejects the caller, which is how the mismatch was caught the first time.
; ---------------------------------------------------------------------------

define { { ptr, i64, i64 }, i32 } @npk_string_concat({ ptr, i64, i64 } %a,
                                                     { ptr, i64, i64 } %b) {
entry:
  %ap = extractvalue { ptr, i64, i64 } %a, 0
  %al = extractvalue { ptr, i64, i64 } %a, 1
  %bp = extractvalue { ptr, i64, i64 } %b, 0
  %bl = extractvalue { ptr, i64, i64 } %b, 1
  %n = add i64 %al, %bl
  %p = call ptr @npk_alloc(i64 %n)
  call ptr @memcpy(ptr %p, ptr %ap, i64 %al)
  %tail = getelementptr i8, ptr %p, i64 %al
  call ptr @memcpy(ptr %tail, ptr %bp, i64 %bl)
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %p, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %n, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %n, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
}

define { { ptr, i64, i64 }, i32 } @npk_int_to_string(i64 %v) {
entry:
  ; Fill a 24-byte buffer from the END, then return a view of the filled tail.
  ; No reversal step, and 24 bytes is enough for -9223372036854775808.
  %buf = call ptr @npk_alloc(i64 24)
  %isneg = icmp slt i64 %v, 0
  %neg = sub i64 0, %v
  %u0 = select i1 %isneg, i64 %neg, i64 %v
  %zero = icmp eq i64 %u0, 0
  br i1 %zero, label %just_zero, label %digits

just_zero:
  %z = getelementptr i8, ptr %buf, i64 23
  store i8 48, ptr %z
  br label %sign

digits:
  %i = phi i64 [ 24, %entry ], [ %i2, %digits ]
  %u = phi i64 [ %u0, %entry ], [ %u2, %digits ]
  %i2 = sub i64 %i, 1
  %q = udiv i64 %u, 10
  %m = urem i64 %u, 10
  %d = trunc i64 %m to i8
  %ch = add i8 %d, 48
  %dp = getelementptr i8, ptr %buf, i64 %i2
  store i8 %ch, ptr %dp
  %u2 = udiv i64 %u, 10
  %more = icmp ugt i64 %q, 0
  br i1 %more, label %digits, label %after

after:
  br label %sign

sign:
  %start0 = phi i64 [ 23, %just_zero ], [ %i2, %after ]
  br i1 %isneg, label %put_sign, label %build

put_sign:
  %sp = sub i64 %start0, 1
  %spp = getelementptr i8, ptr %buf, i64 %sp
  store i8 45, ptr %spp
  br label %build

build:
  %start = phi i64 [ %start0, %sign ], [ %sp, %put_sign ]
  %len = sub i64 24, %start
  %base = getelementptr i8, ptr %buf, i64 %start
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %base, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %len, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %len, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
}

define { { ptr, i64, i64 }, i32 } @npk_string_slice({ ptr, i64, i64 } %s,
                                                    i64 %start, i64 %end) {
entry:
  %p = extractvalue { ptr, i64, i64 } %s, 0
  %l = extractvalue { ptr, i64, i64 } %s, 1
  ; Bounds are CHECKED, not assumed. A slice is a view, and a view past the end
  ; of its backing store is the defect the whole type exists to prevent (D-070).
  %b1 = icmp ult i64 %end, %start
  %b2 = icmp ugt i64 %end, %l
  %bad = or i1 %b1, %b2
  br i1 %bad, label %err, label %ok

err:
  %e0 = insertvalue { { ptr, i64, i64 }, i32 } undef,
                    { ptr, i64, i64 } zeroinitializer, 0
  %e1 = insertvalue { { ptr, i64, i64 }, i32 } %e0, i32 5, 1
  ret { { ptr, i64, i64 }, i32 } %e1

ok:
  %np = getelementptr i8, ptr %p, i64 %start
  %n = sub i64 %end, %start
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %np, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %n, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %n, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
}

; The dual of .ptr / .len: wrap a buffer the caller already owns. Used by the
; lexer to build a decoded string literal, where the decoded bytes are not a
; slice of the source.
define { ptr, i64, i64 } @npk_string_from_bytes(ptr %p, i64 %n) {
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %p, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %n, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %n, 2
  ret { ptr, i64, i64 } %s2
}
