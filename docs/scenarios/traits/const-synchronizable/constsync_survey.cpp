#include <threadsafe/threadsafe.h>

#include <array>
#include <atomic>
#include <bitset>
#include <complex>
#include <chrono>
#include <expected>
#include <flat_map>
#include <flat_set>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <span>
#include <stack>
#include <string>
#include <string_view>
#include <valarray>
#include <vector>
#include <cstdio>

using threadsafe::is_synchronizable_v;
using threadsafe::is_sendable_v;

#define SHOW(...) std::printf("%-58s const-sync=%d  sendable=%d\n", #__VA_ARGS__, \
        (int)is_synchronizable_v<const __VA_ARGS__>, (int)is_sendable_v<__VA_ARGS__>)

struct PolyBase { virtual ~PolyBase() = default; virtual int probe() const { return 0; } };
struct PolyDerived final : PolyBase { mutable int cache = 0; int probe() const override { return ++cache; } };
struct NoopDeleter { void operator()(const int *) const noexcept {} };

int slab[64];
struct SlabHandle {
    int index;
    void bump() const { ++slab[index]; }
};

struct ConstCastCache {
    int raw;
    int parsed;
    int value() const { return const_cast<ConstCastCache *>(this)->parsed = raw * 2; }
};

int main() {
    SHOW(std::bitset<64>);
    SHOW(std::valarray<int>);
    SHOW(std::stack<int>);
    SHOW(std::queue<int>);
    SHOW(std::priority_queue<int>);
    SHOW(std::flat_map<int, int>);
    SHOW(std::flat_set<int>);
    SHOW(std::span<const int>);
    SHOW(std::string_view);
    SHOW(std::pmr::vector<int>);
    SHOW(std::pmr::string);
    SHOW(std::expected<int, int>);
    SHOW(std::complex<double>);
    SHOW(std::chrono::seconds);
    SHOW(std::mutex);
    SHOW(std::shared_mutex);
    SHOW(std::vector<bool>);
    SHOW(threadsafe::copy_on_write<std::string>);
    SHOW(PolyBase);
    SHOW(PolyDerived);
    SHOW(std::unique_ptr<const PolyBase>);
    SHOW(std::unique_ptr<const int, NoopDeleter>);
    SHOW(SlabHandle);
    SHOW(ConstCastCache);
    return 0;
}
