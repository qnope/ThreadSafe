#include <threadsafe/threadsafe.h>

#include <cstddef>
#include <functional>
#include <map>
#include <memory_resource>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// A stateful allocator that holds a raw pointer to an arena owned elsewhere.
template <class T>
struct ArenaAllocator {
    using value_type = T;
    unsigned char *arena;             // borrows!
    T *allocate(std::size_t) { return nullptr; }
    void deallocate(T *, std::size_t) {}
};

// A stateless allocator written as a class template with no members.
template <class T>
struct StatelessAllocator {
    using value_type = T;
    T *allocate(std::size_t) { return nullptr; }
    void deallocate(T *, std::size_t) {}
};

// A comparator that borrows a collation table.
struct BorrowingCompare {
    const int *table;
    bool operator()(int a, int b) const { return table[a] < table[b]; }
};

// A comparator holding a reference.
struct ReferenceCompare {
    const std::vector<int> &order;
    bool operator()(int a, int b) const { return order[a] < order[b]; }
};

// A hasher that borrows a seed.
struct BorrowingHash {
    const std::size_t *seed;
    std::size_t operator()(int v) const { return v ^ *seed; }
};

constexpr auto lambda_compare = [](int a, int b) { return a < b; };
using LambdaCompare = decltype(lambda_compare);

int captured_bias = 0;
auto make_capturing_compare() { return [&](int a, int b) { return a + captured_bias < b; }; }
using CapturingCompare = decltype(make_capturing_compare());

}

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

#define P(NAME, ...) static_assert(__VA_ARGS__, "PROBE " NAME)

P("S pmr::vector<int>",           is_sendable_v<std::pmr::vector<int>>);
P("S pmr::string",                is_sendable_v<std::pmr::string>);
P("S vector<int,ArenaAlloc>",     is_sendable_v<std::vector<int, ArenaAllocator<int>>>);
P("S vector<int,StatelessAlloc>", is_sendable_v<std::vector<int, StatelessAllocator<int>>>);
P("S map<int,int,less<>>",        is_sendable_v<std::map<int,int,std::less<>>>);
P("S map<int,int,BorrowingCmp>",  is_sendable_v<std::map<int,int,BorrowingCompare>>);
P("S map<int,int,ReferenceCmp>",  is_sendable_v<std::map<int,int,ReferenceCompare>>);
P("S map<int,int,LambdaCmp>",     is_sendable_v<std::map<int,int,LambdaCompare>>);
P("S map<int,int,CapturingCmp>",  is_sendable_v<std::map<int,int,CapturingCompare>>);
P("S umap borrowing hash",        is_sendable_v<std::unordered_map<int,int,BorrowingHash>>);
P("S umap<int,int>",              is_sendable_v<std::unordered_map<int,int>>);

P("L pmr::vector<int>",           is_lifetime_aware_v<std::pmr::vector<int>>);
P("L vector<int,ArenaAlloc>",     is_lifetime_aware_v<std::vector<int, ArenaAllocator<int>>>);
P("L vector<int,StatelessAlloc>", is_lifetime_aware_v<std::vector<int, StatelessAllocator<int>>>);
P("L map<int,int,ReferenceCmp>",  is_lifetime_aware_v<std::map<int,int,ReferenceCompare>>);
P("L map<int,int,LambdaCmp>",     is_lifetime_aware_v<std::map<int,int,LambdaCompare>>);
P("L map<int,int,CapturingCmp>",  is_lifetime_aware_v<std::map<int,int,CapturingCompare>>);
P("L map<int,int,less<>>",        is_lifetime_aware_v<std::map<int,int,std::less<>>>);
P("L umap borrowing hash",        is_lifetime_aware_v<std::unordered_map<int,int,BorrowingHash>>);

P("CS const pmr::vector<int>",         is_synchronizable_v<const std::pmr::vector<int>>);
P("CS const vector<int,ArenaAlloc>",   is_synchronizable_v<const std::vector<int, ArenaAllocator<int>>>);
P("CS const vector<int,Stateless>",    is_synchronizable_v<const std::vector<int, StatelessAllocator<int>>>);
P("CS const map less<>",               is_synchronizable_v<const std::map<int,int,std::less<>>>);
P("CS const map BorrowingCmp",         is_synchronizable_v<const std::map<int,int,BorrowingCompare>>);
P("CS const map ReferenceCmp",         is_synchronizable_v<const std::map<int,int,ReferenceCompare>>);
P("CS const map LambdaCmp",            is_synchronizable_v<const std::map<int,int,LambdaCompare>>);
P("CS const map CapturingCmp",         is_synchronizable_v<const std::map<int,int,CapturingCompare>>);
P("CS const umap BorrowingHash",       is_synchronizable_v<const std::unordered_map<int,int,BorrowingHash>>);

// std::char_traits and basic_string variants
P("S string",                is_sendable_v<std::string>);
P("S u8string",              is_sendable_v<std::u8string>);
P("S char_traits<char>",     is_sendable_v<std::char_traits<char>>);
P("CS const char_traits",    is_synchronizable_v<const std::char_traits<char>>);
P("L char_traits<char>",     is_lifetime_aware_v<std::char_traits<char>>);

int main() {}
