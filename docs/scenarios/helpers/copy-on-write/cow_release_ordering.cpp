// Scenario asked for: A writes via as_mutable, hands a copy to B, B reads and
// drops its copy, A calls as_mutable again and must observe use_count()==1.
// Repeated hard so the two threads actually interleave on the refcount.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using threadsafe::copy_on_write;

namespace {
struct Payload {
    std::vector<int> data;
};

std::atomic<int> handoff_ready{0};
std::atomic<int> handoff_dropped{0};
copy_on_write<Payload>* shared_slot = nullptr;
copy_on_write<Payload>* receiver_slot = nullptr;

constexpr int rounds = 20000;

long in_place_detaches = 0;
long copying_detaches = 0;
}

int main() {
    copy_on_write<Payload> document{Payload{std::vector<int>(8, 0)}};

    std::thread reader([&] {
        for (int round = 0; round != rounds; ++round) {
            while (handoff_ready.load(std::memory_order_acquire) != round + 1)
                std::this_thread::yield();
            // B owns its own handle here; read it, then drop it.
            copy_on_write<Payload> received = *receiver_slot;
            volatile int observed = received->data[0];
            (void)observed;
            *receiver_slot = copy_on_write<Payload>{Payload{}};
            handoff_dropped.store(round + 1, std::memory_order_release);
        }
    });

    for (int round = 0; round != rounds; ++round) {
        copy_on_write<Payload> handed_over = document;
        receiver_slot = &handed_over;
        shared_slot = &document;
        handoff_ready.store(round + 1, std::memory_order_release);
        while (handoff_dropped.load(std::memory_order_acquire) != round + 1)
            std::this_thread::yield();

        const long use_count_before = handed_over.operator->() == document.operator->() ? 2 : 1;
        Payload& writable = document.as_mutable();
        if (writable.data.data() == nullptr)
            std::printf("impossible\n");
        writable.data[0] = round;
        if (use_count_before == 1)
            ++in_place_detaches;
        else
            ++copying_detaches;
    }

    reader.join();
    std::printf("rounds=%d in_place=%ld copying=%ld final=%d\n", rounds,
                in_place_detaches, copying_detaches, document->data[0]);
    return 0;
}
