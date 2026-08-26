#include <threadsafe/threadsafe.h>

#include <map>
#include <set>
#include <unordered_set>
#include <vector>

namespace {
auto make_capturing_compare(const std::vector<int> &order) {
    return [&order](int a, int b) { return order[a] < order[b]; };
}
using CapturingCompare = decltype(make_capturing_compare(std::declval<const std::vector<int> &>()));

auto make_capturing_hash(const std::size_t &seed) {
    return [&seed](int v) -> std::size_t { return v ^ seed; };
}
using CapturingHash = decltype(make_capturing_hash(std::declval<const std::size_t &>()));
}

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

#define P(NAME, ...) static_assert(__VA_ARGS__, "PROBE " NAME)

P("S CapturingCompare alone",  is_sendable_v<CapturingCompare>);
P("S set<int,CapturingCmp>",   is_sendable_v<std::set<int, CapturingCompare>>);
P("S map<int,int,CapturingCmp>", is_sendable_v<std::map<int,int,CapturingCompare>>);
P("L CapturingCompare alone",  is_lifetime_aware_v<CapturingCompare>);
P("L set<int,CapturingCmp>",   is_lifetime_aware_v<std::set<int, CapturingCompare>>);
P("CS const set<int,CapturingCmp>", is_synchronizable_v<const std::set<int, CapturingCompare>>);
P("S uset<int,CapturingHash>", is_sendable_v<std::unordered_set<int, CapturingHash>>);
P("L uset<int,CapturingHash>", is_lifetime_aware_v<std::unordered_set<int, CapturingHash>>);

int main() {}
