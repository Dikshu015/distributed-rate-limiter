## Benchmark Results

Two separate benchmarks were run to test sharding under different
conditions: a cheap critical section (`bench_main.cpp`, in-memory token
math only) and an artificially expensive one (`bench_contention_main.cpp`,
simulating a network round-trip like a Redis call). Measured on [your
CPU model].

### Cheap critical section (in-memory token math)

Comparing a single global lock (`ThreadSafeTokenBucket`) against
`ShardedRateLimiter` at varying shard counts:

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
round-trip, which is orders of magnitude slower than in-memory math:

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