# tests/parked/def395-by-value -- programs ACCEPTED today that landing 179 must REFUSE (DEF-395's by-value faces, and DEF-390's `Guard` road)

Parked at landing 177b (2026-11-01; management_13's ruling of 17:59:32 EST, from the seat's probe of the direct
form of `spawn_lends.npk`'s cells). THE OPPOSITE OF `def413-flow/`: nothing here is refused today. Each program
stores a borrow of a frame into a lock's interior -- a spawned task through its pointer parameter (DEF-395, HIGH,
registered 2026-10-31, placed at landing 179), or a plain `await` of a helper that stores through a `Guard` (a face
of DEF-390's unattributed callee store, found by management_13 at 177b) -- and the analyses attribute the store to
nothing, so the lock, or the frame it points into, leaves or is read with a dangling frame address: ACCEPTED on
176b, 177a and 177b alike, and at run time a race (0 or 114), a MachineFault (107) or a hang (`timeout`'s 124).
DEF-393(a)'s `lends` mark refuses only a DEREFERENCE out of the lending frame (`tests/analysis/rejection/spawn_lends.npk`,
re-spelled at 177b to a plain struct; the lock's own dereference-out is TYPE-104/TYPE-047 since DEF-416), never the
by-value `pass move(v)` / `pass v`. Landing 179's sizing covers the `Guard` road (management_13, 17:59:32); when
DEF-395's fix (i) sends the spawn's sanctioned arguments through the store matrix and the `Guard` store is
attributed, every file here becomes a rejection cell (BORROW-001 or BORROW-002 at the spawn, the store or the return),
and `def395_face4_ok.npk` goes to `tests/backend/programs/` as the legal twin.

| file | shape | exit today, 177a `9ef64589` / 177b `74281015` (-O0, `timeout 20`) | face |
|---|---|---|---|
| `d1_spawn_pass_move.npk` | spawn `stash_m(@v, @b)`, then `pass move(v)` | 114 / 114 | DEF-395 face 1 (`s1p_spawn_pass_v`) |
| `d2_spawn_pass_name.npk` | the same, `pass v` | 0 / 0 (a race: 0 or 114 run to run) | DEF-395 face 1 |
| `d3_spawn_rwlock.npk` | a `RwLock`, `pass move(v)` | 0 / 0 (a race) | DEF-395 face 1 |
| `d6_in_struct.npk` | the lock in a struct's field, the struct returned | 114 / 0 (a race) | DEF-395 face 1 |
| `s2_param_spawn.npk` | spawn into the CALLER's lock through a pointer parameter, no return involved | 0 / 0 | DEF-395 face 4 |
| `face3_nested_spawn.npk` | the task re-spawns, `@b` through two tasks into the caller's lock | 0 / 114 (a race) | DEF-395 face 3 (from `wt/51a/.internal/probes/t173/def395/`) |
| `def395_face4.npk` | `mk(Mutex<…>->:mp)` spawns `stash(mp, @b)`; the caller's lock is left dangling | 0 / 0 (a dead coroutine frame, never reused: the row's note) | DEF-395 face 4 (from `t173/gaps/`) |
| `def395_face4_ok.npk` | its legal twin: the caller's own pointer stored | 0 / 0 | stays a program at 179 |
| `d4_await_helper.npk` | `await plain_stash(@v, @b) ?! E1; pass move(v);` | 107 / 107 | DEF-390, the `Guard` road (new at 177b) |
| `d8_nested_helper.npk` | the same through a second helper | 107 / 107 | DEF-390, the `Guard` road |
| `d9_lock_in_struct_field.npk` | `plain_stash(@w.m, @b)`, then `pass move(w)` | 107 / 107 | DEF-390, the `Guard` road |
| `s1_param_await.npk` | through a pointer parameter into the caller's lock, no return involved | 107 / 107 | DEF-390, the `Guard` road |
| `s3_block_await.npk` | `b` in an inner block, the same frame, no return involved | 124 / 124 (a hang) | DEF-390, the `Guard` road |

The exits were taken into a variable by `.internal/probes177b/run_cells.sh` (`ulimit -n 1024`, floor `c8e5033a`), the rows
`.internal/probes177b/out_b6/parked395/results.txt`; management_13's own run is `MANAGEMENT/probes/177b-lent-lock/` (README,
`run.sh`), which also holds the controls that ARE refused today and so are not parked -- `d5_same_frame_guard` (BORROW-002),
`d7_held_task_moves_lock` (BORROW-016), `p1_plain_return` (BORROW-001), `p4_plain_block` (BORROW-002) -- and `p2_holder_return` /
`p3_holder_param` (accepted, 33), DEF-390's known holder road, on its row. The ten `d`/`s` files are management_13's verbatim; the
three others are `nitpick-compiler_53`'s from landing 173's probes (an untracked folder, hence their copies here). Nothing lists
`tests/parked/`: no `[[test]]` of `nitpick.toml` names it but the `grammar` suite, which only parses. The files are kept whole
(`mod:`, `main`, `failsafe`) so that `git mv` restores them.

DATED 2026-11-01 18:36 EST (build 7's cells run, `.internal/probes177b/out_b7/ALL.txt` against `out_b6/ALL.txt`): the exits of d1, d2, d3 and d6 are SCHEDULE-DEPENDENT -- under landing 177a's own binary (npkc 9ef64589) the two runs read d1 114 then 0, d2 0 then 114, d3 0 then 114, d6 114 then 0; the table above records the first run's. A dangling read's exit varies with the spawned task's timing; what is constant is that both compilers ACCEPT every file here (DEF-395's face 1, landing 179).
