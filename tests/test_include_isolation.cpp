// The traits are variable templates, so their value is fixed at the point of
// instantiation. A translation unit that saw only a subset of the
// specializations would compute a DIFFERENT answer for the same type than one
// that saw them all — silently, with no diagnostic at link time (IFNDR).
//
// This TU deliberately includes ONE granular header and nothing else. Every
// assertion below must agree with the answer the umbrella header gives; if a
// specialization ever stops travelling with its trait, this file fails.
#include <threadsafe/lifetime_aware.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

using threadsafe::is_lifetime_aware;
using threadsafe::is_sendable;
using threadsafe::is_synchronizable;

// Reached only through containers.h.
static_assert(is_lifetime_aware<std::vector<int>>);
static_assert(is_lifetime_aware<std::string>);
static_assert(is_sendable<std::vector<int>>);
static_assert(is_sendable<std::string>);

// Reached only through smart_pointers.h.
static_assert(is_sendable<std::unique_ptr<int>>);

// Reached only through synchronizable.h.
static_assert(is_synchronizable<std::atomic<int>>);
static_assert(is_sendable<std::atomic<int>&>,
              "sending a reference to an atomic shares a synchronizable object");
static_assert(is_sendable<void (*)()>,
              "function pointers rely on function types being synchronizable");
