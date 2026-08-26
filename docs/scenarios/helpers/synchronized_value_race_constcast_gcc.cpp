// Data race through lock_shared(): the trait blesses `const LazySquare` because
// reflection only sees three scalars, so synchronized_value picks a
// std::shared_mutex and lets readers in concurrently -- but square() writes.
//
// g++-16 -std=c++26 -freflection -I<include> -g -O1 -pthread race_constcast_gcc.cpp -o race && ./race
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

struct LazySquare {
    int seed;
    int cache;
    bool cached;

    int square() const {
        if (!cached) {
            const_cast<LazySquare*>(this)->cache = seed * seed;
            const_cast<LazySquare*>(this)->cached = true;
        }
        return cache;
    }
};

int main() {
    int wrong_answers = 0;

    for (int attempt = 0; attempt < 20000; ++attempt) {
        threadsafe::synchronized_value<LazySquare> shared_value{
            LazySquare{7, 0, false}};

        std::atomic<int> ready{0};
        std::vector<int> results(8, -1);
        std::vector<std::jthread> readers;

        for (int index = 0; index < 8; ++index)
            readers.emplace_back([&, index] {
                ready.fetch_add(1);
                while (ready.load() < 8) {}
                const auto reader_guard = shared_value.lock_shared();
                results[index] = reader_guard->square();   // concurrent writers
            });

        readers.clear();

        for (int value : results)
            if (value != 49)
                ++wrong_answers;
    }

    std::printf("readers that observed a torn cache: %d\n", wrong_answers);
    return 0;
}
