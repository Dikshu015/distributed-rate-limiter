#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// #include "sharded_rate_limiter.h"    // we're not using these boths in this
// #include "thread_safe_token_bucket.h"

using Clock = std::chrono::steady_clock;

namespace {

// Simulates an expensive critical section — standing in for something
// like a network round-trip to Redis, which is orders of magnitude
// slower than the in-memory refill math benchmarked in bench_main.cpp.
void simulateExpensiveWork(std::chrono::microseconds delay) {
  auto start = Clock::now();
  while (Clock::now() - start < delay) {
    // Busy-wait rather than sleep_for: sleep_for can oversleep by a lot
    // on Windows due to OS timer-resolution limits, which would make
    // this delay wildly inconsistent across runs. A busy-wait gives a
    // much more precise, reproducible delay for benchmarking purposes,
    // at the cost of spinning a CPU core while it waits.
  }
}

double benchSingleLockExpensive(int num_threads, int requests_per_thread,
                                 std::chrono::microseconds work_delay) {
  std::mutex single_mutex;
  std::vector<std::thread> threads;
  auto start = Clock::now();

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&]() {
      for (int i = 0; i < requests_per_thread; ++i) {
        std::lock_guard<std::mutex> lock(single_mutex);
        simulateExpensiveWork(work_delay);
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }

  auto end = Clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count();
}

double benchShardedExpensive(int num_threads, int requests_per_thread,
                              int num_keys, size_t num_shards,
                              std::chrono::microseconds work_delay) {
  std::vector<std::mutex> shard_mutexes(num_shards);
  std::vector<std::thread> threads;
  auto start = Clock::now();

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      for (int i = 0; i < requests_per_thread; ++i) {
        int key = (t * requests_per_thread + i) % num_keys;
        size_t shard_index = static_cast<size_t>(key) % num_shards;
        std::lock_guard<std::mutex> lock(shard_mutexes[shard_index]);
        simulateExpensiveWork(work_delay);
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }

  auto end = Clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace

int main(int argc, char** argv) {
  int num_threads = argc > 1 ? std::atoi(argv[1]) : 16;
  int requests_per_thread = argc > 2 ? std::atoi(argv[2]) : 200;
  int num_keys = argc > 3 ? std::atoi(argv[3]) : 1000;
  int delay_microseconds = argc > 4 ? std::atoi(argv[4]) : 500;

  std::chrono::microseconds work_delay(delay_microseconds);

  std::cout << "Benchmark config: " << num_threads << " threads x "
            << requests_per_thread << " requests, " << num_keys
            << " distinct keys, " << delay_microseconds
            << "us simulated work per call\n\n";

  double single_lock_ms =
      benchSingleLockExpensive(num_threads, requests_per_thread, work_delay);
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Single global lock:      " << single_lock_ms << " ms total\n";

  for (size_t shards : {1, 4, 16, 64}) {
    double ms = benchShardedExpensive(num_threads, requests_per_thread,
                                       num_keys, shards, work_delay);
    double speedup = single_lock_ms / ms;
    std::cout << "Sharded (" << std::setw(2) << shards << " shards):   " << ms
              << " ms total   (" << speedup << "x vs single lock)\n";
  }

  return 0;
}