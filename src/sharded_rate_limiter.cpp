#include "sharded_rate_limiter.h"

#include <functional>

namespace ratelimiter {

ShardedRateLimiter::ShardedRateLimiter(double capacity,
                                     double refill_rate_per_sec,
                                     size_t num_shards)
    : capacity_(capacity),
      refill_rate_per_sec_(refill_rate_per_sec){
          shards_.reserve(num_shards);
            for(size_t i=0;i<num_shards;i++){
                shards_.push_back(std::make_unique<Shard>());
            }
}

size_t ShardedRateLimiter::shardIndexFor(const std::string& key) const {
  return std::hash<std::string>{}(key) % shards_.size();
}

bool ShardedRateLimiter::tryAcquire(const std::string& key, double tokens){
  Shard& shard = *shards_[shardIndexFor(key)];

  std::unique_lock<std::mutex> lock(shard.mutex);
  auto it = shard.buckets.find(key);
  if(it == shard.buckets.end()){
    auto bucket = std::make_unique<ThreadSafeTokenBucket>(
      capacity_, refill_rate_per_sec_);
    it = shard.buckets.emplace(key, std::move(bucket)).first;
  }
  ThreadSafeTokenBucket* bucket_ptr = it->second.get();
  lock.unlock();

  return bucket_ptr->tryAcquire(tokens);

}

} // namespace ratelimiter