#include <chrono>
#include <iostream>
#include <thread>

#include "leader_elector.h"
#include "redis_backend.h"

int main(int argc, char** argv) {
  std::string node_id = argc > 1 ? argv[1] : "node_default";
  std::string redis_uri =
      argc > 2 ? argv[2] : "tcp://127.0.0.1:6379";

  std::cout << "[" << node_id << "] Starting distributed rate limiter "
            << "node, connecting to " << redis_uri << "...\n";

  RedisBackend backend(redis_uri);
  LeaderElector elector(redis_uri, "ratelimiter:leader_lock", node_id,
                        /*ttl_seconds=*/10);

  const long capacity = 10;
  const double refill_rate = 2.0;
  int request_count = 0;

  while (true) {
    if (!elector.isLeader()) {
      elector.tryAcquireLeadership();
    } else {
      elector.renewLeadership();
    }

    std::string client_key = "client_demo";
    bool allowed = backend.tryAcquire(client_key, capacity, refill_rate);
    ++request_count;

    std::cout << "[" << node_id << "] request #" << request_count
              << " -> " << (allowed ? "ALLOWED" : "DENIED")
              << " | leader=" << (elector.isLeader() ? "yes" : "no") << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  return 0;
}