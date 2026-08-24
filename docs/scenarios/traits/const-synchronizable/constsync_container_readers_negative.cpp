// Negative probe: do the explicit const container rules actually hold up?
// Four readers hammer const member functions of a vector<bool>, an
// unordered_map (whose libstdc++ rehash policy has a mutable member), a map
// (node reuse) and a string, all at once, under TSan.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <map>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using threadsafe::is_synchronizable_v;

static_assert(is_synchronizable_v<const std::vector<bool>>);
static_assert(is_synchronizable_v<const std::unordered_map<int, std::string>>);
static_assert(is_synchronizable_v<const std::map<int, std::string>>);
static_assert(is_synchronizable_v<const std::string>);

int main() {
    std::vector<bool> flags(1024);
    for (std::size_t position = 0; position < flags.size(); ++position)
        flags[position] = position % 3 == 0;

    std::unordered_map<int, std::string> hashed;
    std::map<int, std::string> ordered;
    for (int key = 0; key < 512; ++key) {
        hashed.emplace(key, std::to_string(key));
        ordered.emplace(key, std::to_string(key));
    }
    const std::string text = "the quick brown fox";

    const std::vector<bool> &const_flags = flags;
    const std::unordered_map<int, std::string> &const_hashed = hashed;
    const std::map<int, std::string> &const_ordered = ordered;

    auto reader = [&] {
        std::size_t accumulated = 0;
        for (int round = 0; round < 2000; ++round) {
            for (std::size_t position = 0; position < const_flags.size(); ++position)
                accumulated += const_flags[position] ? 1 : 0;
            accumulated += const_hashed.find(round % 512)->second.size();
            accumulated += const_hashed.bucket_count();
            accumulated += const_hashed.load_factor() > 0 ? 1 : 0;
            accumulated += const_ordered.find(round % 512)->second.size();
            accumulated += text.find('q');
        }
        return accumulated;
    };

    std::thread first{reader};
    std::thread second{reader};
    std::thread third{reader};
    std::thread fourth{reader};
    first.join(); second.join(); third.join(); fourth.join();

    std::printf("four concurrent const readers finished clean\n");
    return 0;
}
