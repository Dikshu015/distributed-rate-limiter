#include "redis_backend.h"

#include <sw/redis++/redis++.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

/**
 * @brief Constructs a RedisBackend, connects to Redis, and loads the
 * rate limiting script.
 *
 * If script loading fails (e.g. Redis unreachable, or the script file
 * is missing), this throws — a RedisBackend that can't load its script
 * is not in a usable state, so failing fast at construction is better
 * than discovering the problem on the first real request.
 */
RedisBackend::RedisBackend(const std::string& connection_uri)
    : redis_(std::make_unique<sw::redis::Redis>(connection_uri)) {
    script_sha_ = loadScriptSha();
}

/**
 * @brief Destructor. Defined here (not defaulted in the header) because
 * sw::redis::Redis's full definition must be visible to generate the
 * unique_ptr's deletion logic correctly.
 */
RedisBackend::~RedisBackend() = default;

/**
 * @brief Reads scripts/token_bucket.lua from disk and registers it with
 * Redis via SCRIPT LOAD, returning the SHA1 hash Redis assigns it.
 *
 * @throws std::runtime_error if the script file cannot be read.
 */
std::string RedisBackend::loadScriptSha() {
    std::ifstream file("scripts/token_bucket.lua");
    if (!file.is_open()) {
        throw std::runtime_error(
            "RedisBackend: could not open scripts/token_bucket.lua "
            "(check that the program is run from the project root)");
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string script = buffer.str();

    return redis_->script_load(script);
}

/**
 * @brief Attempts to consume one token for the given client key by
 * calling the cached Lua script via EVALSHA.
 *
 * Fails closed: if Redis is unreachable or the call otherwise throws,
 * this returns false (denies the request) rather than propagating the
 * exception. During an outage, rejecting traffic is usually safer than
 * letting it through unchecked.
 */
bool RedisBackend::tryAcquire(const std::string& key, long capacity,
                               double refill_rate) {
    double now = std::chrono::duration<double>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();

    try {
        long long allowed = redis_->evalsha<long long>(
            script_sha_,
            {key},
            {std::to_string(capacity), std::to_string(refill_rate),
             std::to_string(now)});

        return allowed == 1;
    } catch (const sw::redis::Error& e) {
        std::cerr << "RedisBackend::tryAcquire failed, denying request: "
                  << e.what() << std::endl;
        return false;
    }
}