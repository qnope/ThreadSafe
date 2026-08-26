#include <threadsafe/threadsafe.h>
#include <mutex>
#include <string>

struct Cache {
    mutable std::mutex mutex_;
    mutable std::string memo_;
    int seed_;

    int compute() const {
        std::lock_guard<std::mutex> guard(mutex_);
        if (memo_.empty())
            memo_ = std::to_string(seed_);
        return static_cast<int>(memo_.size());
    }
};

static_assert((threadsafe::assert_synchronizable<const Cache>(), true));
