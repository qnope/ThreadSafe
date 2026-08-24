// A writes in place, hands a copy to B through a synchronized mailbox, B reads
// it and drops it, A writes in place again.  Does the acquire fence order A's
// second write after B's reads?  Hammered 50000 times under TSan.
#include <threadsafe/threadsafe.h>

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using threadsafe::copy_on_write;

namespace {
struct Payload {
    std::vector<int> counters;
};

constexpr int total_rounds = 50000;

std::mutex mailbox_mutex;
std::condition_variable mailbox_changed;
std::optional<copy_on_write<Payload>> mailbox;
bool mailbox_consumed = false;

long in_place_writes = 0;
long detaching_writes = 0;
}

int main() {
    copy_on_write<Payload> document{Payload{std::vector<int>(64, 0)}};

    std::thread consumer([] {
        for (int round = 0; round != total_rounds; ++round) {
            std::unique_lock<std::mutex> held{mailbox_mutex};
            mailbox_changed.wait(held, [] { return mailbox.has_value(); });
            copy_on_write<Payload> received = std::move(*mailbox);
            mailbox.reset();
            held.unlock();

            long long observed_sum = 0;
            for (int value : received->counters)
                observed_sum += value;
            if (observed_sum == -1)
                std::printf("unreachable\n");

            // `received` dies here: the non-last release decrement.
            {
                std::lock_guard<std::mutex> held_again{mailbox_mutex};
                mailbox_consumed = true;
            }
            mailbox_changed.notify_one();
        }
    });

    for (int round = 0; round != total_rounds; ++round) {
        {
            std::lock_guard<std::mutex> held{mailbox_mutex};
            mailbox = document;
            mailbox_consumed = false;
        }
        mailbox_changed.notify_one();
        {
            std::unique_lock<std::mutex> held{mailbox_mutex};
            mailbox_changed.wait(held, [] { return mailbox_consumed; });
        }

        const bool was_unique = document.operator->() != nullptr;
        Payload* before = const_cast<Payload*>(document.operator->());
        Payload& writable = document.as_mutable();
        if (&writable == before)
            ++in_place_writes;
        else
            ++detaching_writes;
        (void)was_unique;
        for (int& counter : writable.counters)
            ++counter;
    }

    consumer.join();
    std::printf("rounds=%d in_place=%ld detaching=%ld first_counter=%d\n",
                total_rounds, in_place_writes, detaching_writes,
                document->counters[0]);
    return 0;
}
