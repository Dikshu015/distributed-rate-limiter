#include "token_bucket.h"

#include <algorithm>

namespace ratelimiter{

TokenBucket::TokenBucket(double capacity, double refill_rate_per_sec)
    : capacity_(capacity),
      refill_rate_per_sec_(refill_rate_per_sec),
      tokens_(capacity),
      last_refill_(std::chrono::steady_clock::now()){}

void TokenBucket::refill(){
    auto now = std::chrono::steady_clock::now();

    double elapsed_seconds = 
        std::chrono::duration<double>(now - last_refill_).count();
    
    double tokens_to_add = elapsed_seconds * refill_rate_per_sec_;

    if (tokens_to_add>0.0){
        tokens_ = std::min(capacity_, tokens_ + tokens_to_add);
        last_refill_ = now;
    }
}

bool TokenBucket::tryAcquire(double tokens){
    refill(); // lazy refill 
    if(tokens<=tokens_){
        tokens_-=tokens;
        return true;
    }
    return false;
}



} // namespace ratelimiter