#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

struct plain_aggregate {
    int identifier;
    double weight;
    std::string label;
    std::vector<int> samples;
};

static_assert(threadsafe::is_sendable_v<plain_aggregate>);
static_assert(threadsafe::is_synchronizable_v<const plain_aggregate>);
static_assert(threadsafe::is_lifetime_aware_v<plain_aggregate>);
static_assert(threadsafe::is_sendable_v<std::vector<std::string>>);
static_assert(threadsafe::is_synchronizable_v<std::atomic<int>>);
static_assert(!threadsafe::is_sendable_v<int*>);

int consume(plain_aggregate value) {
    return value.identifier + int(value.samples.size());
}
