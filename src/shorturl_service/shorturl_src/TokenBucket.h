#ifndef TOKEN_BUCKET_H
#define TOKEN_BUCKET_H

#include <chrono>
#include <mutex>

class TokenBucket {
private:
    int capacity;          // 桶的容量
    int tokens;            // 当前令牌数
    double rate;           // 令牌生成速率（令牌/秒）
    std::chrono::time_point<std::chrono::steady_clock> last_fill_time; // 上次填充令牌的时间
    mutable std::mutex mutex; // 互斥锁，保证线程安全

public:
    TokenBucket(int capacity, double rate) 
        : capacity(capacity), tokens(capacity), rate(rate), 
          last_fill_time(std::chrono::steady_clock::now()) {}

    // 尝试获取令牌
    bool tryAcquire(int required_tokens = 1) {
        std::lock_guard<std::mutex> lock(mutex);
        fillTokens();
        if (tokens >= required_tokens) {
            tokens -= required_tokens;
            return true;
        }
        return false;
    }

private:
    // 填充令牌
    void fillTokens() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fill_time).count();
        int new_tokens = static_cast<int>(elapsed * rate / 1000);
        if (new_tokens > 0) {
            tokens = std::min(capacity, tokens + new_tokens);
            last_fill_time = now;
        }
    }
};

#endif // TOKEN_BUCKET_H