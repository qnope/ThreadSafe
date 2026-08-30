#include <threadsafe/threadsafe.h>

#include <cstddef>
#include <functional>

namespace {

struct UserCopyCtor {
    UserCopyCtor(const UserCopyCtor&);
};

struct BadAlloc {
    UserCopyCtor state;
    using value_type = int;
    int* allocate(std::size_t);
    void deallocate(int*, std::size_t);
};

struct BadHash {
    UserCopyCtor state;
    std::size_t operator()(int) const;
};

struct BadCompare {
    UserCopyCtor state;
    bool operator()(int, int) const;
};

struct MutCache {
    int raw;
    mutable int parsed;
};

// Vouched for as if it synchronized itself: the point is not that a vector ever
// does, but that the std-wrapper rule must not short-circuit the invariant.
struct VouchedElement {
    mutable int cache;
};

struct OptedOut {};

struct MoveOnlyStrings {
    std::vector<std::string> strings;
    MoveOnlyStrings(const MoveOnlyStrings&) = delete;
    MoveOnlyStrings& operator=(const MoveOnlyStrings&) = delete;
    MoveOnlyStrings(MoveOnlyStrings&&) = default;
    MoveOnlyStrings& operator=(MoveOnlyStrings&&) = default;
};

}

template <>
struct threadsafe::is_unsafe_synchronizable<std::vector<VouchedElement>> {
    static constexpr threadsafe::TraitAnswer value = {};
};

template <>
struct threadsafe::is_unsafe_sendable<std::vector<OptedOut>> {
    static constexpr threadsafe::TraitAnswer value
        = "opted out by this test";
};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_sendable_v<std::allocator<int>>,
              "is_sendable — std::allocator is stateless, sending it is safe");
static_assert(is_sendable_v<std::less<int>>,
              "is_sendable — stateless comparators are sendable by default");
static_assert(is_sendable_v<std::hash<int>>,
              "is_sendable — stateless hashers are sendable by default");
static_assert(is_sendable_v<std::equal_to<int>>,
              "is_sendable — stateless equality functors are sendable by default");

static_assert(is_sendable_v<std::vector<int>>,
              "is_sendable — a vector of sendable elements is sendable");
static_assert(is_sendable_v<std::string>,
              "is_sendable — a string owns its characters");
static_assert(is_sendable_v<std::u8string>,
              "is_sendable — basic_string covers every character type");
static_assert(is_sendable_v<std::map<int, std::string>>,
              "is_sendable — a map of sendable keys and values is sendable");
static_assert(is_sendable_v<std::multimap<int, std::string>>,
              "is_sendable — multimap follows the same rule as map");
static_assert(is_sendable_v<std::set<int>>,
              "is_sendable — a set of sendable keys is sendable");
static_assert(is_sendable_v<std::multiset<int>>,
              "is_sendable — multiset follows the same rule as set");
static_assert(is_sendable_v<std::unordered_map<std::string, int>>,
              "is_sendable — an unordered_map of sendable keys and values is sendable");
static_assert(is_sendable_v<std::unordered_multimap<std::string, int>>,
              "is_sendable — unordered_multimap follows the same rule as unordered_map");
static_assert(is_sendable_v<std::unordered_set<int>>,
              "is_sendable — an unordered_set of sendable keys is sendable");
static_assert(is_sendable_v<std::unordered_multiset<int>>,
              "is_sendable — unordered_multiset follows the same rule as unordered_set");
static_assert(is_sendable_v<std::vector<std::map<int, std::string>>>,
              "is_sendable — nested containers recurse through their elements");
static_assert(is_sendable_v<MoveOnlyStrings>,
              "is_sendable — a move-only class holding a vector<string> is "
              "sendable: deleted copy members do not block sendability");

static_assert(!is_sendable_v<std::vector<UserCopyCtor>>,
              "is_sendable — a non-sendable element makes the container non-sendable");
static_assert(!is_sendable_v<std::vector<int*>>,
              "is_sendable — sending a container of pointers shares the pointees");
static_assert(!is_sendable_v<std::map<int, UserCopyCtor>>,
              "is_sendable — a non-sendable mapped type makes the map non-sendable");

static_assert(!is_sendable_v<std::vector<int, BadAlloc>>,
              "is_sendable — the allocator is stored, so it must be sendable too");
static_assert(!is_sendable_v<std::set<int, BadCompare>>,
              "is_sendable — the comparator is stored, so it must be sendable too");
static_assert(!is_sendable_v<std::unordered_set<int, BadHash>>,
              "is_sendable — the hasher is stored, so it must be sendable too");
static_assert(!is_sendable_v<std::unordered_set<int, std::hash<int>, BadCompare>>,
              "is_sendable — the key_equal is stored, so it must be sendable too");

static_assert(is_sendable_v<const std::vector<int>>,
              "is_sendable — cv-qualified T forwards to the container specialization");
static_assert(!is_sendable_v<std::vector<int>&>,
              "is_sendable — sending a reference shares the container, which is not synchronizable");

static_assert(is_synchronizable_v<const std::vector<int>>
                  && is_synchronizable_v<const std::string>
                  && is_synchronizable_v<const std::deque<int>>
                  && is_synchronizable_v<const std::list<int>>
                  && is_synchronizable_v<const std::forward_list<int>>,
              "is_synchronizable — [res.on.data.races]: const member functions "
              "of a standard container may run concurrently");
static_assert(is_synchronizable_v<const std::map<int, std::string>>
                  && is_synchronizable_v<const std::multimap<int, std::string>>
                  && is_synchronizable_v<const std::set<int>>
                  && is_synchronizable_v<const std::multiset<int>>,
              "is_synchronizable — and their stored policies are read too");
static_assert(is_synchronizable_v<const std::unordered_map<int, std::string>>
                  && is_synchronizable_v<const std::unordered_multimap<int, std::string>>
                  && is_synchronizable_v<const std::unordered_set<int>>
                  && is_synchronizable_v<const std::unordered_multiset<int>>,
              "is_synchronizable — the explicit rule is what keeps libstdc++'s "
              "mutable rehash-policy internals out of the recursion");
static_assert(!is_synchronizable_v<std::vector<int>>,
              "is_synchronizable — without the const the container is writable");
static_assert(!is_synchronizable_v<const std::vector<MutCache>>,
              "is_synchronizable — a reader reaches the elements, so their "
              "const form must be read-safe too");
static_assert(!is_synchronizable_v<const std::vector<int*>>
                  && !is_synchronizable_v<const std::vector<const int*>>,
              "is_synchronizable — an element that borrows gives readers a "
              "write path, and a pointed-to const proves nothing");
static_assert(is_synchronizable_v<const std::allocator<int>>,
              "is_synchronizable — stateless, ruled explicitly because its "
              "converting-constructor template blocks the structural default");
static_assert(is_synchronizable_v<const std::less<int>>
                  && is_synchronizable_v<const std::hash<int>>
                  && is_synchronizable_v<const std::equal_to<int>>,
              "is_synchronizable — empty policies with no constructor templates "
              "pass the structural default, no rule needed");
static_assert(!is_synchronizable_v<const std::set<int, BadCompare>>,
              "is_synchronizable — a stored policy with a user-provided copy "
              "fails the structural guard");

static_assert(is_sendable_v<std::vector<VouchedElement>>
                  && is_synchronizable_v<const std::vector<VouchedElement>>,
              "is_sendable/is_synchronizable — synchronizable implies sendable "
              "and readable from several threads at once; the std-wrapper rule "
              "answers before the walk, so it carries that invariant itself");
static_assert(!is_sendable_v<std::vector<OptedOut>>,
              "is_sendable — a full specialization outranks the std-wrapper "
              "rule, which is itself a constrained partial specialization");
