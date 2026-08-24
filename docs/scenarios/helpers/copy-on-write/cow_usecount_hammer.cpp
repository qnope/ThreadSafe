// Hammers the detach decision: one writer, four readers, handles handed out
// through a slot array and dropped by the readers with no synchronisation
// other than the shared_ptr refcount.  If use_count()==1 could ever be a
// FALSE "unique", the writer's in-place write would race a reader's read.
//
// Uses a byte-identical copy_on_write except that the acquire is done with a
// real atomic RMW instead of std::atomic_thread_fence, because GCC's
// ThreadSanitizer does not model atomic_thread_fence at all
// (-Wtsan: "'atomic_thread_fence' is not supported with '-fsanitize=thread'").
#include <atomic>
#include <concepts>
#include <cstdio>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {
template <class T>
class copy_on_write {
public:
    template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>, copy_on_write>
                      && ...))
    explicit copy_on_write(Args&&... args)
        : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}

    const T& operator*() const noexcept { return *ptr_; }
    const T* operator->() const noexcept { return ptr_.get(); }

    T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(std::as_const(*ptr_));
        else
            [[maybe_unused]] const std::shared_ptr<T> acquiring_copy = ptr_;
        return *ptr_;
    }

private:
    std::shared_ptr<T> ptr_;
};

struct Payload {
    std::vector<int> counters{std::vector<int>(32, 0)};
};

constexpr int reader_count = 4;
constexpr int rounds = 120000;

struct Slot {
    std::atomic<int> generation{0};
    copy_on_write<Payload>* handed{nullptr};
};

Slot slots[reader_count];
std::atomic<int> ready_count{0};
std::atomic<bool> stop_flag{false};
std::atomic<long long> total_observed{0};
long in_place = 0;
long detached = 0;
}

int main() {
    copy_on_write<Payload> document{Payload{}};

    std::vector<std::thread> readers;
    for (int reader_index = 0; reader_index != reader_count; ++reader_index)
        readers.emplace_back([reader_index] {
            int seen_generation = 0;
            long long local = 0;
            for (;;) {
                while (slots[reader_index].generation.load(std::memory_order_acquire)
                       == seen_generation) {
                    if (stop_flag.load(std::memory_order_relaxed)) {
                        total_observed.fetch_add(local, std::memory_order_relaxed);
                        return;
                    }
                    std::this_thread::yield();
                }
                ++seen_generation;
                {
                    copy_on_write<Payload> received = *slots[reader_index].handed;
                    for (int value : received->counters)
                        local += value;
                    // Signal BEFORE dropping: the drop itself is the only thing
                    // that can order the reader's reads before the writer's
                    // in-place write.
                    ready_count.fetch_add(1, std::memory_order_release);
                }
            }
        });

    for (int round = 0; round != rounds; ++round) {
        {
            copy_on_write<Payload> published = document;
            ready_count.store(0, std::memory_order_relaxed);
            for (int reader_index = 0; reader_index != reader_count; ++reader_index) {
                slots[reader_index].handed = &published;
                slots[reader_index].generation.fetch_add(1, std::memory_order_release);
            }
            while (ready_count.load(std::memory_order_acquire) != reader_count)
                std::this_thread::yield();
        }   // the writer's own publication handle dies here

        const Payload* before = document.operator->();
        for (;;) {
            Payload& writable = document.as_mutable();
            if (&writable == before) {
                ++in_place;
                for (int& counter : writable.counters)
                    ++counter;
                break;
            }
            ++detached;
            before = document.operator->();
        }
    }

    stop_flag.store(true, std::memory_order_relaxed);
    for (std::thread& reader : readers)
        reader.join();

    std::printf("rounds=%d in_place=%ld detached=%ld first=%d observed=%lld\n",
                rounds, in_place, detached, document->counters[0],
                total_observed.load());
    return 0;
}
