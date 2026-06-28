-- token_bucket.lua
--
-- Atomic refill-check-decrement for a single rate limit key, executed
-- server-side in Redis so concurrent callers from different processes
-- can never race each other on the same bucket.
--
-- KEYS[1] = bucket key, e.g. "ratelimit:user42"
-- ARGV[1] = capacity (max tokens the bucket can hold)
-- ARGV[2] = refill_rate (tokens added per second)
-- ARGV[3] = now (current unix time in seconds, as a float)
--
-- Returns 1 if a token was acquired, 0 if the bucket was empty.

local key = KEYS[1]
local capacity = tonumber(ARGV[1])
local refill_rate = tonumber(ARGV[2])
local now = tonumber(ARGV[3])

local bucket = redis.call("HMGET", key, "tokens", "last_refill")
local tokens = tonumber(bucket[1])
local last_refill = tonumber(bucket[2])

if tokens == nil then
    tokens = capacity
    last_refill = now
end

local elapsed = now - last_refill
if elapsed > 0 then
    tokens = math.min(capacity, tokens + elapsed * refill_rate)
    last_refill = now
end

local allowed = 0
if tokens >= 1 then
    tokens = tokens - 1
    allowed = 1
end

redis.call("HMSET", key, "tokens", tokens, "last_refill", last_refill)
redis.call("EXPIRE", key, 3600)

return allowed