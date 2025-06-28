#include <string>
#include <unordered_map>
#include <mutex>
#include <list>
#include <chrono>
#include <fstream>
#include <sstream>
#include <queue>
#include <thread>
#include <functional>
#include <condition_variable>
using namespace std;


//kv存储系统

class KVStore {
public:
    static KVStore& getInstance() { //单例模式
        static KVStore instance;
        return instance;
    }

    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;

    // 设置缓存最大容量
    void setMaxCapacity(size_t capacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_capacity_ = capacity;
    }

    // 异步设置 key 并指定过期时间
    void asyncSet(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
        {
            std::lock_guard<std::mutex> lock(task_mutex_);
            task_queue_.emplace([this, key, value, ttl]() {
                set(key, value, ttl);
            });
        }
        task_cv_.notify_one();
    }

    // 获取 key 的值
    std::string get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it == store_.end()) {
            return "";
        }

        if (std::chrono::system_clock::now() > it->second.expire_time) {
            access_order_.erase(it->second.it);
            store_.erase(it);
            return "";
        }

        access_order_.erase(it->second.it);
        access_order_.push_front(key);
        it->second.it = access_order_.begin();

        return it->second.value;
    }

    // 删除所有以指定前缀开头的 key
    void deleteKeysWithPrefix(const std::string& prefix) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = store_.begin(); it != store_.end(); ) {
            if (it->first.find(prefix) == 0) {
                access_order_.erase(it->second.it);
                it = store_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 数据持久化到文件
    void persistToFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream file(filename);
        if (file.is_open()) {
            for (const auto& entry : store_) {
                const auto& key = entry.first;
                const auto& value = entry.second.value;
                auto expire_time = entry.second.expire_time.time_since_epoch().count();
                file << key << "\t" << value << "\t" << expire_time << "\n";
            }
            file.close();
        }
    }

    // 从文件加载持久化数据
    void loadFromFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ifstream file(filename);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream iss(line);
                std::string key, value;
                int64_t expire_time_count;
                if (std::getline(iss, key, '\t') && 
                    std::getline(iss, value, '\t') && 
                    (iss >> expire_time_count)) {
                    auto expire_time = std::chrono::system_clock::time_point(
                        std::chrono::nanoseconds(expire_time_count));
                    if (std::chrono::system_clock::now() < expire_time) {
                        set(key, value, std::chrono::duration_cast<std::chrono::seconds>(
                            expire_time - std::chrono::system_clock::now()));
                    }
                }
            }
            file.close();
        }
    }

    // 启动定时清理过期 key 的任务
    void startExpirationCleaner(std::chrono::seconds interval) {
        std::thread([this, interval]() {
            while (true) {
                std::this_thread::sleep_for(interval);
                cleanExpiredKeys();
            }
        }).detach();
    }

    // 启动工作线程
    void startWorkerThread() {
        worker_running_ = true;
        worker_thread_ = std::thread([this]() {
            while (worker_running_) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(task_mutex_);
                    task_cv_.wait(lock, [this]() {
                        return!task_queue_.empty() ||!worker_running_;
                    });
                    if (!task_queue_.empty()) {
                        task = std::move(task_queue_.front());
                        task_queue_.pop();
                    }
                }
                if (task) {
                    task();
                }
            }
        });
    }

    // 停止工作线程
    void stopWorkerThread() {
        {
            std::lock_guard<std::mutex> lock(task_mutex_);
            worker_running_ = false;
        }
        task_cv_.notify_one();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    KVStore() : max_capacity_(100) {
        startWorkerThread();
    }

    ~KVStore() {
        stopWorkerThread();
    }

    struct Entry {
        std::string value;
        std::chrono::system_clock::time_point expire_time;
        std::list<std::string>::iterator it;
    };

    // 同步设置 key 并指定过期时间
    void set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto expire_time = std::chrono::system_clock::now() + ttl;

        if (store_.find(key) != store_.end()) {
            access_order_.erase(store_[key].it);
        }

        access_order_.push_front(key);
        store_[key] = {value, expire_time, access_order_.begin()};

        if (store_.size() > max_capacity_) {
            auto last_key = access_order_.back();
            access_order_.pop_back();
            store_.erase(last_key);
        }
    }

    // 清理过期的 key
    void cleanExpiredKeys() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        for (auto it = store_.begin(); it != store_.end(); ) {
            if (now > it->second.expire_time) {
                access_order_.erase(it->second.it);
                it = store_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::unordered_map<std::string, Entry> store_;
    std::list<std::string> access_order_;
    size_t max_capacity_;
    std::mutex mutex_;

    std::queue<std::function<void()>> task_queue_;
    std::mutex task_mutex_;
    std::condition_variable task_cv_;
    std::thread worker_thread_;
    bool worker_running_ = false;
};
