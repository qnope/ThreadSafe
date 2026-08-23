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

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class T>
struct is_sendable<std::allocator<T>> : std::true_type {};

template <class T, class A>
struct is_sendable<std::vector<T, A>>
    : std::bool_constant<is_sendable_v<T> && is_sendable_v<A>> {};

template <class T, class A>
struct is_sendable<std::deque<T, A>>
    : std::bool_constant<is_sendable_v<T> && is_sendable_v<A>> {};

template <class T, class A>
struct is_sendable<std::list<T, A>>
    : std::bool_constant<is_sendable_v<T> && is_sendable_v<A>> {};

template <class T, class A>
struct is_sendable<std::forward_list<T, A>>
    : std::bool_constant<is_sendable_v<T> && is_sendable_v<A>> {};

template <class C, class Tr, class A>
struct is_sendable<std::basic_string<C, Tr, A>>
    : std::bool_constant<is_sendable_v<C> && is_sendable_v<A>> {};

template <class K, class V, class Cmp, class A>
struct is_sendable<std::map<K, V, Cmp, A>>
    : std::bool_constant<is_sendable_v<K> && is_sendable_v<V>
                         && is_sendable_v<Cmp> && is_sendable_v<A>> {};

template <class K, class V, class Cmp, class A>
struct is_sendable<std::multimap<K, V, Cmp, A>>
    : std::bool_constant<is_sendable_v<K> && is_sendable_v<V>
                         && is_sendable_v<Cmp> && is_sendable_v<A>> {};

template <class K, class Cmp, class A>
struct is_sendable<std::set<K, Cmp, A>>
    : std::bool_constant<is_sendable_v<K> && is_sendable_v<Cmp>
                         && is_sendable_v<A>> {};

template <class K, class Cmp, class A>
struct is_sendable<std::multiset<K, Cmp, A>>
    : std::bool_constant<is_sendable_v<K> && is_sendable_v<Cmp>
                         && is_sendable_v<A>> {};

template <class K, class V, class H, class Eq, class A>
struct is_sendable<std::unordered_map<K, V, H, Eq, A>>
    : std::bool_constant<is_sendable_v<K> && is_sendable_v<V>
                         && is_sendable_v<H> && is_sendable_v<Eq>
                         && is_sendable_v<A>> {};

template <class K, class V, class H, class Eq, class A>
struct is_sendable<std::unordered_multimap<K, V, H, Eq, A>>
    : std::bool_constant<is_sendable_v<K> && is_sendable_v<V>
                         && is_sendable_v<H> && is_sendable_v<Eq>
                         && is_sendable_v<A>> {};

template <class K, class H, class Eq, class A>
struct is_sendable<std::unordered_set<K, H, Eq, A>>
    : std::bool_constant<is_sendable_v<K> && is_sendable_v<H>
                         && is_sendable_v<Eq> && is_sendable_v<A>> {};

template <class K, class H, class Eq, class A>
struct is_sendable<std::unordered_multiset<K, H, Eq, A>>
    : std::bool_constant<is_sendable_v<K> && is_sendable_v<H>
                         && is_sendable_v<Eq> && is_sendable_v<A>> {};

// [res.on.data.races]: the const member functions of a standard container may
// run concurrently, so a const container is read-safe exactly when everything
// a reader reaches through it — elements and stored policies — is. The
// explicit rules also keep the recursion out of libstdc++ internals, whose
// mutable members (unordered_*'s rehash policy) are covered by that guarantee.
template <class T>
struct is_synchronizable<const std::allocator<T>> : std::true_type {};

template <class T, class A>
struct is_synchronizable<const std::vector<T, A>>
    : std::bool_constant<is_synchronizable_v<const T>
                         && is_synchronizable_v<const A>> {};

template <class T, class A>
struct is_synchronizable<const std::deque<T, A>>
    : std::bool_constant<is_synchronizable_v<const T>
                         && is_synchronizable_v<const A>> {};

template <class T, class A>
struct is_synchronizable<const std::list<T, A>>
    : std::bool_constant<is_synchronizable_v<const T>
                         && is_synchronizable_v<const A>> {};

template <class T, class A>
struct is_synchronizable<const std::forward_list<T, A>>
    : std::bool_constant<is_synchronizable_v<const T>
                         && is_synchronizable_v<const A>> {};

template <class C, class Tr, class A>
struct is_synchronizable<const std::basic_string<C, Tr, A>>
    : std::bool_constant<is_synchronizable_v<const C>
                         && is_synchronizable_v<const A>> {};

template <class K, class V, class Cmp, class A>
struct is_synchronizable<const std::map<K, V, Cmp, A>>
    : std::bool_constant<is_synchronizable_v<const K>
                         && is_synchronizable_v<const V>
                         && is_synchronizable_v<const Cmp>
                         && is_synchronizable_v<const A>> {};

template <class K, class V, class Cmp, class A>
struct is_synchronizable<const std::multimap<K, V, Cmp, A>>
    : std::bool_constant<is_synchronizable_v<const K>
                         && is_synchronizable_v<const V>
                         && is_synchronizable_v<const Cmp>
                         && is_synchronizable_v<const A>> {};

template <class K, class Cmp, class A>
struct is_synchronizable<const std::set<K, Cmp, A>>
    : std::bool_constant<is_synchronizable_v<const K>
                         && is_synchronizable_v<const Cmp>
                         && is_synchronizable_v<const A>> {};

template <class K, class Cmp, class A>
struct is_synchronizable<const std::multiset<K, Cmp, A>>
    : std::bool_constant<is_synchronizable_v<const K>
                         && is_synchronizable_v<const Cmp>
                         && is_synchronizable_v<const A>> {};

template <class K, class V, class H, class Eq, class A>
struct is_synchronizable<const std::unordered_map<K, V, H, Eq, A>>
    : std::bool_constant<is_synchronizable_v<const K>
                         && is_synchronizable_v<const V>
                         && is_synchronizable_v<const H>
                         && is_synchronizable_v<const Eq>
                         && is_synchronizable_v<const A>> {};

template <class K, class V, class H, class Eq, class A>
struct is_synchronizable<const std::unordered_multimap<K, V, H, Eq, A>>
    : std::bool_constant<is_synchronizable_v<const K>
                         && is_synchronizable_v<const V>
                         && is_synchronizable_v<const H>
                         && is_synchronizable_v<const Eq>
                         && is_synchronizable_v<const A>> {};

template <class K, class H, class Eq, class A>
struct is_synchronizable<const std::unordered_set<K, H, Eq, A>>
    : std::bool_constant<is_synchronizable_v<const K>
                         && is_synchronizable_v<const H>
                         && is_synchronizable_v<const Eq>
                         && is_synchronizable_v<const A>> {};

template <class K, class H, class Eq, class A>
struct is_synchronizable<const std::unordered_multiset<K, H, Eq, A>>
    : std::bool_constant<is_synchronizable_v<const K>
                         && is_synchronizable_v<const H>
                         && is_synchronizable_v<const Eq>
                         && is_synchronizable_v<const A>> {};

template <class T>
struct is_lifetime_aware<std::allocator<T>> : std::true_type {};

template <class T, class A>
struct is_lifetime_aware<std::vector<T, A>>
    : std::bool_constant<is_lifetime_aware_v<T> && is_lifetime_aware_v<A>> {};

template <class T, class A>
struct is_lifetime_aware<std::deque<T, A>>
    : std::bool_constant<is_lifetime_aware_v<T> && is_lifetime_aware_v<A>> {};

template <class T, class A>
struct is_lifetime_aware<std::list<T, A>>
    : std::bool_constant<is_lifetime_aware_v<T> && is_lifetime_aware_v<A>> {};

template <class T, class A>
struct is_lifetime_aware<std::forward_list<T, A>>
    : std::bool_constant<is_lifetime_aware_v<T> && is_lifetime_aware_v<A>> {};

template <class C, class Tr, class A>
struct is_lifetime_aware<std::basic_string<C, Tr, A>>
    : std::bool_constant<is_lifetime_aware_v<C> && is_lifetime_aware_v<A>> {};

template <class K, class V, class Cmp, class A>
struct is_lifetime_aware<std::map<K, V, Cmp, A>>
    : std::bool_constant<is_lifetime_aware_v<K> && is_lifetime_aware_v<V>
                         && is_lifetime_aware_v<Cmp>
                         && is_lifetime_aware_v<A>> {};

template <class K, class V, class Cmp, class A>
struct is_lifetime_aware<std::multimap<K, V, Cmp, A>>
    : std::bool_constant<is_lifetime_aware_v<K> && is_lifetime_aware_v<V>
                         && is_lifetime_aware_v<Cmp>
                         && is_lifetime_aware_v<A>> {};

template <class K, class Cmp, class A>
struct is_lifetime_aware<std::set<K, Cmp, A>>
    : std::bool_constant<is_lifetime_aware_v<K> && is_lifetime_aware_v<Cmp>
                         && is_lifetime_aware_v<A>> {};

template <class K, class Cmp, class A>
struct is_lifetime_aware<std::multiset<K, Cmp, A>>
    : std::bool_constant<is_lifetime_aware_v<K> && is_lifetime_aware_v<Cmp>
                         && is_lifetime_aware_v<A>> {};

template <class K, class V, class H, class Eq, class A>
struct is_lifetime_aware<std::unordered_map<K, V, H, Eq, A>>
    : std::bool_constant<is_lifetime_aware_v<K> && is_lifetime_aware_v<V>
                         && is_lifetime_aware_v<H>
                         && is_lifetime_aware_v<Eq>
                         && is_lifetime_aware_v<A>> {};

template <class K, class V, class H, class Eq, class A>
struct is_lifetime_aware<std::unordered_multimap<K, V, H, Eq, A>>
    : std::bool_constant<is_lifetime_aware_v<K> && is_lifetime_aware_v<V>
                         && is_lifetime_aware_v<H>
                         && is_lifetime_aware_v<Eq>
                         && is_lifetime_aware_v<A>> {};

template <class K, class H, class Eq, class A>
struct is_lifetime_aware<std::unordered_set<K, H, Eq, A>>
    : std::bool_constant<is_lifetime_aware_v<K> && is_lifetime_aware_v<H>
                         && is_lifetime_aware_v<Eq>
                         && is_lifetime_aware_v<A>> {};

template <class K, class H, class Eq, class A>
struct is_lifetime_aware<std::unordered_multiset<K, H, Eq, A>>
    : std::bool_constant<is_lifetime_aware_v<K> && is_lifetime_aware_v<H>
                         && is_lifetime_aware_v<Eq>
                         && is_lifetime_aware_v<A>> {};

}
