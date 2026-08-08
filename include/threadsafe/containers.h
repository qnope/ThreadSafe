#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <threadsafe/sendable.h>

namespace threadsafe {

// std::allocator is stateless; its user-provided copy constructor (libstdc++)
// would otherwise block the reflection default.
template <class T>
constexpr bool is_sendable<std::allocator<T>> = true;

// A container owns its elements: moving it to another thread moves everything
// it stores — elements and the stored policy objects (allocator, comparator,
// hasher, key_equal). Sendable exactly when all of those are.
template <class T, class A>
constexpr bool is_sendable<std::vector<T, A>> = is_sendable<T> && is_sendable<A>;

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

}  // namespace threadsafe
