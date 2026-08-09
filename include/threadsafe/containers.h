#pragma once

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <threadsafe/lifetime_aware.h>
#include <threadsafe/sendable.h>

namespace threadsafe {

// std::allocator is stateless; its user-provided copy constructor (libstdc++)
// would otherwise block the reflection default. Note that "stateless" is a
// statement about the allocator object, not about the arena it allocates
// from: an empty allocator backed by a thread_local or global free list is
// blessed here and cannot be caught by any type-level trait.
template <class T>
constexpr bool is_sendable<std::allocator<T>> = true;

// A container owns its elements: moving it to another thread moves everything
// it stores — elements and the stored policy objects (allocator, comparator,
// hasher, key_equal). Sendable exactly when all of those are.
//
// Every container needs an explicit rule, not just the ones whose default
// would be wrong: the reflection default walks the implementation's internal
// raw pointers (node links, begin/end) and answers false for every node-based
// container, which is over-strict rather than unsafe but pushes users toward
// blessing types by hand.
template <class T, class A>
constexpr bool is_sendable<std::vector<T, A>> = is_sendable<T> && is_sendable<A>;

template <class T, class A>
constexpr bool is_sendable<std::deque<T, A>> = is_sendable<T> && is_sendable<A>;

template <class T, class A>
constexpr bool is_sendable<std::list<T, A>> = is_sendable<T> && is_sendable<A>;

template <class T, class A>
constexpr bool is_sendable<std::forward_list<T, A>> =
    is_sendable<T> && is_sendable<A>;

// char_traits is never stored, so it puts no condition on sendability.
template <class C, class Tr, class A>
constexpr bool is_sendable<std::basic_string<C, Tr, A>> =
    is_sendable<C> && is_sendable<A>;

template <class K, class V, class Cmp, class A>
constexpr bool is_sendable<std::map<K, V, Cmp, A>> =
    is_sendable<K> && is_sendable<V> && is_sendable<Cmp> && is_sendable<A>;

template <class K, class V, class Cmp, class A>
constexpr bool is_sendable<std::multimap<K, V, Cmp, A>> =
    is_sendable<K> && is_sendable<V> && is_sendable<Cmp> && is_sendable<A>;

template <class K, class Cmp, class A>
constexpr bool is_sendable<std::set<K, Cmp, A>> =
    is_sendable<K> && is_sendable<Cmp> && is_sendable<A>;

template <class K, class Cmp, class A>
constexpr bool is_sendable<std::multiset<K, Cmp, A>> =
    is_sendable<K> && is_sendable<Cmp> && is_sendable<A>;

template <class K, class V, class H, class Eq, class A>
constexpr bool is_sendable<std::unordered_map<K, V, H, Eq, A>> =
    is_sendable<K> && is_sendable<V> && is_sendable<H> && is_sendable<Eq>
    && is_sendable<A>;

template <class K, class V, class H, class Eq, class A>
constexpr bool is_sendable<std::unordered_multimap<K, V, H, Eq, A>> =
    is_sendable<K> && is_sendable<V> && is_sendable<H> && is_sendable<Eq>
    && is_sendable<A>;

template <class K, class H, class Eq, class A>
constexpr bool is_sendable<std::unordered_set<K, H, Eq, A>> =
    is_sendable<K> && is_sendable<H> && is_sendable<Eq> && is_sendable<A>;

template <class K, class H, class Eq, class A>
constexpr bool is_sendable<std::unordered_multiset<K, H, Eq, A>> =
    is_sendable<K> && is_sendable<H> && is_sendable<Eq> && is_sendable<A>;

// --- lifetime ---------------------------------------------------------------
// A container keeps its elements alive, but NOT whatever those elements point
// at: a vector<string_view> owns the views and borrows the characters. The
// allocator is checked too, so a std::pmr container — whose allocator holds a
// memory_resource* — correctly comes out borrowing.
//
// These rules are required for correctness, not merely for precision: the
// reflection default would descend into the container's own internal raw
// pointers and answer false for every one of them.
template <class T>
constexpr bool is_lifetime_aware<std::allocator<T>> = true;

template <class T, class A>
constexpr bool is_lifetime_aware<std::vector<T, A>> =
    is_lifetime_aware<T> && is_lifetime_aware<A>;

template <class T, class A>
constexpr bool is_lifetime_aware<std::deque<T, A>> =
    is_lifetime_aware<T> && is_lifetime_aware<A>;

template <class T, class A>
constexpr bool is_lifetime_aware<std::list<T, A>> =
    is_lifetime_aware<T> && is_lifetime_aware<A>;

template <class T, class A>
constexpr bool is_lifetime_aware<std::forward_list<T, A>> =
    is_lifetime_aware<T> && is_lifetime_aware<A>;

template <class C, class Tr, class A>
constexpr bool is_lifetime_aware<std::basic_string<C, Tr, A>> =
    is_lifetime_aware<C> && is_lifetime_aware<A>;

template <class K, class V, class Cmp, class A>
constexpr bool is_lifetime_aware<std::map<K, V, Cmp, A>> =
    is_lifetime_aware<K> && is_lifetime_aware<V> && is_lifetime_aware<Cmp>
    && is_lifetime_aware<A>;

template <class K, class V, class Cmp, class A>
constexpr bool is_lifetime_aware<std::multimap<K, V, Cmp, A>> =
    is_lifetime_aware<K> && is_lifetime_aware<V> && is_lifetime_aware<Cmp>
    && is_lifetime_aware<A>;

template <class K, class Cmp, class A>
constexpr bool is_lifetime_aware<std::set<K, Cmp, A>> =
    is_lifetime_aware<K> && is_lifetime_aware<Cmp> && is_lifetime_aware<A>;

template <class K, class Cmp, class A>
constexpr bool is_lifetime_aware<std::multiset<K, Cmp, A>> =
    is_lifetime_aware<K> && is_lifetime_aware<Cmp> && is_lifetime_aware<A>;

template <class K, class V, class H, class Eq, class A>
constexpr bool is_lifetime_aware<std::unordered_map<K, V, H, Eq, A>> =
    is_lifetime_aware<K> && is_lifetime_aware<V> && is_lifetime_aware<H>
    && is_lifetime_aware<Eq> && is_lifetime_aware<A>;

template <class K, class V, class H, class Eq, class A>
constexpr bool is_lifetime_aware<std::unordered_multimap<K, V, H, Eq, A>> =
    is_lifetime_aware<K> && is_lifetime_aware<V> && is_lifetime_aware<H>
    && is_lifetime_aware<Eq> && is_lifetime_aware<A>;

template <class K, class H, class Eq, class A>
constexpr bool is_lifetime_aware<std::unordered_set<K, H, Eq, A>> =
    is_lifetime_aware<K> && is_lifetime_aware<H> && is_lifetime_aware<Eq>
    && is_lifetime_aware<A>;

template <class K, class H, class Eq, class A>
constexpr bool is_lifetime_aware<std::unordered_multiset<K, H, Eq, A>> =
    is_lifetime_aware<K> && is_lifetime_aware<H> && is_lifetime_aware<Eq>
    && is_lifetime_aware<A>;

}  // namespace threadsafe
