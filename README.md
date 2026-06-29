# Distributed Rate Limiter

A rate limiter built in C++17, progressing through five layers of
increasing coordination complexity: a single-threaded token-bucket
algorithm, a mutex-protected version of it, a sharded version to reduce
lock contention, a Redis-backed version for coordination across
multiple processes, and a leader election layer for designating a
single coordinator among several running nodes.

Each layer was built, compiled, and tested in isolation before the next
was added. Every number in this README is from an actual run on real
hardware, not an estimate.

## Architecture

| Layer | Files | Solves |
|---|---|---|
| Algorithm | `token_bucket.h/.cpp` | Token-bucket refill and consumption logic, single-threaded |
| Thread safety | `thread_safe_token_bucket.h/.cpp` | Safe concurrent access from multiple threads in one process |
| Sharding | `sharded_rate_limiter.h/.cpp` | Reduced lock contention across many distinct client keys |
| Distributed state | `redis_backend.h/.cpp`, `scripts/token_bucket.lua` | Shared rate limit state across multiple processes/machines via Redis |
| Coordination | `leader_elector.h/.cpp`, `scripts/leader_renew.lua` | Electing a single leader among several running nodes, with fencing-safe renewal |

## Building

Requires a C++17 compiler, CMake, and Redis running locally.

### Windows (MSYS2)

See `scripts/setup_windows.sh` for the full toolchain and Redis setup.
In short: install the MSYS2 UCRT64 toolchain, `hiredis` and
`redis-plus-plus` via pacman, CMake, and run Redis via Docker
(`redis:7-alpine`). Build and run from the MSYS2 UCRT64 terminal —
other MSYS2 sub-environments are known to fail at the linking step on
this setup.

### Build

```bash
mkdir -p build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
cd ..
```

This produces four targets in `build/`: `server`, `bench`,
`bench_contention`, and the `ratelimiter` static library they all link
against.

### Running

`server.exe` must be run from the project root, not from `build/`,
since it loads `scripts/token_bucket.lua` and `scripts/leader_renew.lua`
using paths relative to the working directory:

```bash
./build/server.exe node_A
```

## Correctness Verification

Before any performance numbers are meaningful, correctness was verified
under real concurrent load — not just read and assumed.

**Single-bucket contention** (`tests/manual_stress_test.cpp`): 16
threads, 50 attempts each (800 total), against a bucket with capacity
100 and refill rate 0 (no refill, so any deviation from exactly 100
successful grants is unambiguous evidence of a race):

```
successful_count = 100 (capacity = 100)
```

**Multi-key isolation under sharding**
(`tests/manual_multi_key_stress_test.cpp`): 20 distinct client keys, 4
threads per key, across only 16 shards — guaranteeing some keys hash to
the same shard. Every key independently received exactly its entitled
capacity of 10, with zero cross-contamination between keys sharing a
shard:

```
user_0: 10 grants (expected 10)
user_1: 10 grants (expected 10)
...
user_19: 10 grants (expected 10)
PASS: every key independently received exactly its capacity.
```

**Redis Lua script, full lifecycle** — three sequential `redis-cli
--eval` calls against the same key, capacity=10, refill_rate=1,
timestamp frozen (no refill noise):

| Call | Requested | Tokens before | Result | Tokens after |
|---|---|---|---|---|
| 1 | 1 | 10 | allowed | 9 |
| 2 | 9 | 9 | allowed (exact boundary) | 0 |
| 3 | 1 | 0 | denied | 0 |

This confirms state genuinely persists in Redis between separate
process invocations, and both the exact-boundary-allowed case and the
exhaustion-denied case behave correctly.

## Benchmark Results

Two separate benchmarks test sharding under different conditions: a
cheap critical section (`bench_main.cpp`, in-memory token math only)
and an artificially expensive one (`bench_contention_main.cpp`,
simulating a 500-microsecond network round-trip like a Redis call).

### Cheap critical section (in-memory token math)

Single global lock (`ThreadSafeTokenBucket`) vs. `ShardedRateLimiter`
at varying shard counts:

**16 threads, 50,000 requests/thread, 1,000 distinct keys:**

| Configuration | Time | vs. single lock |
|---|---|---|
| Single global lock | 147.97 ms | baseline |
| Sharded, 1 shard | 723.09 ms | 0.20x |
| Sharded, 4 shards | 262.28 ms | 0.56x |
| Sharded, 16 shards | 143.10 ms | 1.03x |
| Sharded, 64 shards | 210.65 ms | 0.70x |

**64 threads, 50,000 requests/thread, 1,000 distinct keys:**

| Configuration | Time | vs. single lock |
|---|---|---|
| Single global lock | 524.65 ms | baseline |
| Sharded, 1 shard | 2727.43 ms | 0.19x |
| Sharded, 4 shards | 997.78 ms | 0.53x |
| Sharded, 16 shards | 538.66 ms | 0.97x |
| Sharded, 64 shards | 572.94 ms | 0.92x |

**Finding:** sharding does not outperform a single global lock here, at
either thread count. The token-bucket critical section (a timestamp
read, a multiply, a comparison) is cheap enough that
`ShardedRateLimiter`'s own per-call overhead — hashing the key and an
`unordered_map` lookup — exceeds whatever contention cost it removes.
At 1 shard, the design pays sharding's overhead with none of its
benefit, explaining the ~5x regression visible there.

### Expensive critical section (simulated 500us network round-trip)

Same comparison, but each call now holds its lock for a simulated
500-microsecond delay — standing in for something like a Redis
round-trip, orders of magnitude slower than in-memory math:

**16 threads, 200 requests/thread, 1,000 distinct keys, 500us simulated work:**

| Configuration | Time | vs. single lock |
|---|---|---|
| Single global lock | 1610.26 ms | baseline |
| Sharded, 1 shard | 1608.81 ms | 1.00x |
| Sharded, 4 shards | 417.31 ms | 3.86x |
| Sharded, 16 shards | 244.06 ms | 6.60x |
| Sharded, 64 shards | 224.13 ms | 7.18x |

**Finding:** once the critical section is expensive enough, sharding
wins clearly — up to 7.18x faster at 64 shards. With 16 threads forced
through one lock, total time is dominated by threads queuing for that
lock; splitting work across shards lets threads on different shards run
their expensive work in parallel instead of queuing behind each other.
1 shard lands at ~1.00x rather than regressing, because the per-call
hashing/lookup overhead that hurt sharding in the cheap-work benchmark
is now negligible next to a 500-microsecond critical section. Returns
diminish from 16 to 64 shards (6.60x → 7.18x) because shard count has
exceeded available parallelism — adding more shards stops unlocking
real concurrency once there's no contention left to relieve.

### Conclusion

Sharding's benefit is conditional, not automatic — it depends on the
critical section being expensive relative to the cost of routing to a
shard (hashing + lookup). The in-memory `TokenBucket` math benchmarked
above is too cheap for sharding to help. But the actual deployed
system's critical section also includes a Redis round-trip (see
`RedisBackend`), which is far closer in cost to the simulated 500us
delay than to the near-instant in-memory case — exactly the regime
where this benchmark shows sharding paying off.

Reproduce these numbers:

```bash
./build/bench.exe 16 50000 1000
./build/bench.exe 64 50000 1000
./build/bench_contention.exe 16 200 1000 500
```

## Distributed Leader Election

`LeaderElector` uses a Redis key (`SET key value NX EX ttl`) as a
lease-based lock. Acquisition is a single atomic Redis command, so
there is no race even if two nodes attempt it at the exact same
instant — Redis processes commands one at a time, and exactly one
`SET ... NX` can ever succeed against an absent key.

Renewal is harder: a leader must extend its lease before the TTL
expires, but a naive renewal could let a node that has already lost
leadership (e.g. after a long pause) overwrite a new leader's lease.
This is fixed with a check-and-set Lua script
(`scripts/leader_renew.lua`) that only extends the TTL if the caller's
lease token still matches what is stored in Redis.

### Verified failover

Two instances of `tests/manual_leader_election_test.cpp` were run
concurrently against the same Redis instance and the same election
key, with a 3-second lease TTL. Wall-clock timestamps from the actual
run:

```
[node_A] 22:12:10 (t=0s):   ACQUIRED leadership
[node_A] 22:12:11 (t=1s):   renewed (still leader)
...
[node_A] 22:12:30 (t=19s):  renewed (still leader)
[node_A] -- process killed (Ctrl+C) --

[node_B] 22:12:33 (t=15s):  ACQUIRED leadership
[node_B] 22:12:34 (t=16s):  renewed (still leader)
...
```

node_B acquired leadership 3 seconds after node_A's last confirmed
renewal — exactly matching the configured TTL, with no overlap and no
gap longer than one lease cycle.

Reproduce this (`manual_leader_election_test.cpp` is not yet a CMake
target — build it directly):

```bash
g++ -std=c++17 -Iinclude src/leader_elector.cpp tests/manual_leader_election_test.cpp -o leader_test -lredis++ -lhiredis -lws2_32

# Terminal 1
./leader_test.exe node_A 120

# Terminal 2 (within a few seconds)
./leader_test.exe node_B 120
```

Kill node_A's process (Ctrl+C) and watch node_B's terminal — it should
log `ACQUIRED leadership` within one TTL window (3 seconds, as
configured in the test file).

`server.exe` (built via CMake, see Building above) also runs
`LeaderElector` continuously as part of its main loop, using a
10-second TTL, but logs leadership status inline with rate-limit
decisions rather than in the dedicated format shown above.

## Known Limitations

- **Relative file paths.** `RedisBackend` and `LeaderElector` load their
  Lua scripts using paths relative to the current working directory
  (`scripts/token_bucket.lua`), not relative to the executable's
  location. The binaries must be run from the project root. A more
  robust version would embed the scripts as string literals at compile
  time or resolve paths relative to the executable.
- **No automated test runner, and no test files wired into CMake.**
  Correctness is verified by standalone programs in `tests/`, each
  compiled and run by hand rather than through a GoogleTest suite:
  `manual_stress_test.cpp` (single-bucket contention),
  `manual_multi_key_stress_test.cpp` (sharded multi-key isolation),
  `manual_leader_election_test.cpp` (failover timing),
  `manual_redis_backend_test.cpp` and `redis_smoke_test.cpp` (Redis
  connectivity and backend behavior), plus two header-only compile
  checks (`leader_elector_header_check.cpp`,
  `redis_backend_header_check.cpp`). Expected output for each is
  documented above and in the files themselves.
- **Fixed TTLs and shard counts in the demo binaries.** `server_main.cpp`
  uses hardcoded capacity, refill rate, and lease TTL values for
  demonstration; a production deployment would make these configurable.