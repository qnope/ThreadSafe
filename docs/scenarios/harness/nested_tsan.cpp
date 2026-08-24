// Does the pattern the fix unlocks -- a nested copy_on_write shared across
// threads by handle copies, with one thread detaching -- actually race?
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using threadsafe::copy_on_write;

struct Body { std::string text; };
struct Document { copy_on_write<Body> body; int revision; };

int main() {
    std::atomic<std::size_t> observed{0};
    for (int round = 0; round < 200; ++round) {
        copy_on_write<Document> shared(Document{copy_on_write<Body>(Body{std::string(64, 'a')}), 0});
        std::vector<std::thread> threads;
        for (int reader = 0; reader < 4; ++reader)
            threads.emplace_back([shared, &observed] {
                for (int i = 0; i < 200; ++i)
                    observed += (*shared).body->text.size() + (*shared).revision;
            });
        threads.emplace_back([shared, &observed]() mutable {
            for (int i = 0; i < 200; ++i) {
                Document &writable = shared.as_mutable();
                writable.revision += 1;
                writable.body.as_mutable().text.push_back('b');
                observed += writable.body->text.size();
            }
        });
        for (auto &t : threads) t.join();
    }
    std::printf("observed=%zu\n", observed.load());
}
