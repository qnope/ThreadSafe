#include <threadsafe/containers.h>

#include <cstddef>
#include <functional>

namespace {

struct UserCopyCtor {
    UserCopyCtor(const UserCopyCtor&);
};

// Stateful policy types holding a non-sendable member.
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

// Move-only, with the copy members explicitly deleted.
struct MoveOnlyStrings {
    std::vector<std::string> strings;
    MoveOnlyStrings(const MoveOnlyStrings&) = delete;
    MoveOnlyStrings& operator=(const MoveOnlyStrings&) = delete;
    MoveOnlyStrings(MoveOnlyStrings&&) = default;
    MoveOnlyStrings& operator=(MoveOnlyStrings&&) = default;
};

}  // namespace

using threadsafe::is_sendable;

// --- policy types: default allocator and functors are sendable ---
static_assert(is_sendable<std::allocator<int>>,
              "is_sendable — std::allocator is stateless, sending it is safe");
static_assert(is_sendable<std::less<int>>,
              "is_sendable — stateless comparators are sendable by default");
static_assert(is_sendable<std::hash<int>>,
              "is_sendable — stateless hashers are sendable by default");
static_assert(is_sendable<std::equal_to<int>>,
              "is_sendable — stateless equality functors are sendable by default");

// --- containers of sendable elements are sendable ---
static_assert(is_sendable<std::vector<int>>,
              "is_sendable — a vector of sendable elements is sendable");
static_assert(is_sendable<std::string>,
              "is_sendable — a string owns its characters");
static_assert(is_sendable<std::u8string>,
              "is_sendable — basic_string covers every character type");
static_assert(is_sendable<std::map<int, std::string>>,
              "is_sendable — a map of sendable keys and values is sendable");
static_assert(is_sendable<std::multimap<int, std::string>>,
              "is_sendable — multimap follows the same rule as map");
static_assert(is_sendable<std::set<int>>,
              "is_sendable — a set of sendable keys is sendable");
static_assert(is_sendable<std::multiset<int>>,
              "is_sendable — multiset follows the same rule as set");
static_assert(is_sendable<std::unordered_map<std::string, int>>,
              "is_sendable — an unordered_map of sendable keys and values is sendable");
static_assert(is_sendable<std::unordered_multimap<std::string, int>>,
              "is_sendable — unordered_multimap follows the same rule as unordered_map");
static_assert(is_sendable<std::unordered_set<int>>,
              "is_sendable — an unordered_set of sendable keys is sendable");
static_assert(is_sendable<std::unordered_multiset<int>>,
              "is_sendable — unordered_multiset follows the same rule as unordered_set");
static_assert(is_sendable<std::vector<std::map<int, std::string>>>,
              "is_sendable — nested containers recurse through their elements");
static_assert(is_sendable<MoveOnlyStrings>,
              "is_sendable — a move-only class holding a vector<string> is "
              "sendable: deleted copy members do not block sendability");

// --- non-sendable elements propagate ---
static_assert(!is_sendable<std::vector<UserCopyCtor>>,
              "is_sendable — a non-sendable element makes the container non-sendable");
static_assert(!is_sendable<std::vector<int*>>,
              "is_sendable — sending a container of pointers shares the pointees");
static_assert(!is_sendable<std::map<int, UserCopyCtor>>,
              "is_sendable — a non-sendable mapped type makes the map non-sendable");

// --- non-sendable stored policy objects propagate ---
static_assert(!is_sendable<std::vector<int, BadAlloc>>,
              "is_sendable — the allocator is stored, so it must be sendable too");
static_assert(!is_sendable<std::set<int, BadCompare>>,
              "is_sendable — the comparator is stored, so it must be sendable too");
static_assert(!is_sendable<std::unordered_set<int, BadHash>>,
              "is_sendable — the hasher is stored, so it must be sendable too");
static_assert(!is_sendable<std::unordered_set<int, std::hash<int>, BadCompare>>,
              "is_sendable — the key_equal is stored, so it must be sendable too");

// --- cv and references ---
static_assert(is_sendable<const std::vector<int>>,
              "is_sendable — cv-qualified T forwards to the container specialization");
static_assert(!is_sendable<std::vector<int>&>,
              "is_sendable — sending a reference shares the container, which is not synchronizable");
