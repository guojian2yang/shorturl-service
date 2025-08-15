#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include "TokenBucket.h"
#include <unordered_map>
#include <string>

class RateLimiter {
private:
    std::unordered_map<std::string, TokenBucket> ip_buckets; // IP 令牌桶映射
    std::unordered_map<std::string, TokenBucket> user_buckets; // 用户令牌桶映射
    std::unordered_map<std::string, TokenBucket> rpc_buckets; // RPC 接口令牌桶映射

    int bucket_capacity; // 令牌桶容量
    double token_rate;   // 令牌生成速率

public:
    RateLimiter(int capacity, double rate) : bucket_capacity(capacity), token_rate(rate) {}

    // 检查 IP 请求是否允许
    bool checkIP(const std::string& ip) {
        if (ip_buckets.find(ip) == ip_buckets.end()) {
            ip_buckets.emplace(ip, TokenBucket(bucket_capacity, token_rate));
        }
        return ip_buckets[ip].tryAcquire();
    }

    // 检查用户请求是否允许
    bool checkUser(const std::string& user_id) {
        if (user_buckets.find(user_id) == user_buckets.end()) {
            user_buckets.emplace(user_id, TokenBucket(bucket_capacity, token_rate));
        }
        return user_buckets[user_id].tryAcquire();
    }

    // 检查 RPC 接口请求是否允许
    bool checkRPC(const std::string& rpc_name) {
        if (rpc_buckets.find(rpc_name) == rpc_buckets.end()) {
            rpc_buckets.emplace(rpc_name, TokenBucket(bucket_capacity, token_rate));
        }
        return rpc_buckets[rpc_name].tryAcquire();
    }
};

#endif // RATE_LIMITER_H