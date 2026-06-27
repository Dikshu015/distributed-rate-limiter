#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "sharded_rate_limiter.h"
#include "thread_safe_token_bucket.h"

using Clock = std::chrono::steady_clock;

namespace { // anonymous name space

double benchSingleLock(int num_threads, int requests_per_thread){
    ratelimiter::ThreadSafeTokenBucket bucket(/*capacity=*/1e9,
                                               /*refill_rate_per_sec=*/1e9);
    
    std::vector<std::thread> threads;
    auto start = Clock::now();
    
    for(int t=0; t < num_threads; ++t){
        threads.emplace_back([&](){
            for(int i=0; i< requests_per_thread; ++i){
                bucket.tryAcquire(1.0);
            }
        });
    }
    for (auto& th : threads){
        th.join();
    }

    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end-start).count();
}

double benchSharded(int num_threads, int request_per_thread, int num_keys,
                     size_t num_shards){
    ratelimiter::ShardedRateLimiter limiter(1e9,1e9,num_shards);

    std::vector<std::thread> threads;
    auto start = Clock::now();

    for (int t=0; t< num_threads; ++t){
        threads.emplace_back([&, t]{
            for(int i=0; i < request_per_thread; ++i){
                std::string key = "user_" + std::to_string((t* request_per_thread + i) % num_keys);
                limiter.tryAcquire(key, 1.0);
            }
        });
    }
    for(auto& th : threads){
        th.join();
    }

    auto end = Clock::now();
    return std::chrono::duration<double,std::milli>(end-start).count();
}

} // namespace

int main(int argc, char** argv){
    int num_threads = argc > 1 ? std::atoi(argv[1]) : 16;
    int requests_per_thread = argc > 2 ? std::atoi(argv[2]) : 50000;
    int num_keys = argc > 3 ? std::atoi(argv[3]) : 1000;

    std::cout << "Benchmark config: " << num_threads << " threads x "
            << requests_per_thread << " requests, " << num_keys
            << " distinct keys\n\n";

    double single_lock_ms = benchSingleLock(num_threads,requests_per_thread);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Single global lock:      " << single_lock_ms << " ms total\n";

    for (size_t shards : {1,4,16,64}){
        double ms = benchSharded(num_threads, requests_per_thread, num_keys, shards);
        double speedup = single_lock_ms / ms;
        std::cout << "Sharded (" << std::setw(2) << shards << " shards):   " << ms
                  << " ms total   (" << speedup << "x vs single lock)\n";
    }

  return 0;
}