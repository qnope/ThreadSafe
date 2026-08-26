#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

struct Base {
    virtual ~Base() = default;
    virtual void bump() { count_.fetch_add(1, std::memory_order_relaxed); }
    std::atomic<int> count_{0};
};

template <>
struct threadsafe::is_synchronizable<Base> : std::true_type {};

struct Derived : Base {
    void bump() override { Base::bump(); ++unsynchronized_; }
    long unsynchronized_ = 0;
};

static_assert(!threadsafe::is_sendable_v<std::unique_ptr<Base>>,
              "unique_ptr is refused");
static_assert(threadsafe::is_sendable_v<std::shared_ptr<Base>>,
              "shared_ptr to the same Base is accepted");
static_assert(std::is_same_v<
                  threadsafe::synchronized_value<std::shared_ptr<Base>>::mutex,
                  std::shared_mutex>);

int main() {
    constexpr long per_thread = 200000;
    auto derived = std::make_shared<Derived>();

    threadsafe::synchronized_value<std::shared_ptr<Base>> shared{derived};

    {
        std::vector<std::jthread> workers;
        for (int i = 0; i < 4; ++i)
            workers.emplace_back([&shared] {
                for (long n = 0; n < per_thread; ++n) {
                    auto reader = shared.lock_shared();   // shared_lock
                    reader->get()->bump();
                }
            });
    }

    std::printf("atomic member  : %d (expected %ld)\n",
                derived->count_.load(), per_thread * 4);
    std::printf("derived member : %ld (expected %ld)\n",
                derived->unsynchronized_, per_thread * 4);
    return derived->unsynchronized_ == per_thread * 4 ? 0 : 1;
}
