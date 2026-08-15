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

template <class T>
constexpr bool is_sendable<std::allocator<T>> = true;

template <class T, class A>
constexpr bool is_sendable<std::vector<T, A>> = is_sendable<T> && is_sendable<A>;

template <class T, class A>
constexpr bool is_sendable<std::deque<T, A>> = is_sendable<T> && is_sendable<A>;

template <class T, class A>
constexpr bool is_sendable<std::list<T, A>> = is_sendable<T> && is_sendable<A>;

template <class T, class A>
constexpr bool is_sendable<std::forward_list<T, A>> =
    is_sendable<T> && is_sendable<A>;

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

}
