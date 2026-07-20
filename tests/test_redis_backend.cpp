#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "redis_backend.h"

// These tests require a live Redis instance at tcp://127.0.0.1:6379.
// In CI, this is provided by a Redis service container.
// Locally: docker start redis-dev

static const std::string kRedisUri = "tcp://127.0.0.1:6379";

TEST(RedisBackend, AllowsRequestsWithinCapacity) {
  RedisBackend backend(kRedisUri);
  const std::string key = "test:redis_backend:within_capacity";

  EXPECT_TRUE(backend.tryAcquire(key, /*capacity=*/5, /*refill_rate=*/0));
}

TEST(RedisBackend, DeniesRequestsOnceBucketIsExhausted) {
  RedisBackend backend(kRedisUri);
  const std::string key = "test:redis_backend:exhausted";

  for (int i = 0; i < 5; ++i) {
    backend.tryAcquire(key, /*capacity=*/5, /*refill_rate=*/0);
  }

  EXPECT_FALSE(backend.tryAcquire(key, /*capacity=*/5, /*refill_rate=*/0));
}

TEST(RedisBackend, RefillsTokensOverTime) {
  RedisBackend backend(kRedisUri);
  const std::string key = "test:redis_backend:refill";

  for (int i = 0; i < 3; ++i) {
    backend.tryAcquire(key, /*capacity=*/3, /*refill_rate=*/10);
  }
  EXPECT_FALSE(backend.tryAcquire(key, /*capacity=*/3, /*refill_rate=*/10));

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  EXPECT_TRUE(backend.tryAcquire(key, /*capacity=*/3, /*refill_rate=*/10));
}