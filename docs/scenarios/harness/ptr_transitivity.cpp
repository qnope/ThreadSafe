#include <threadsafe/threadsafe.h>
#include <memory>
#include <span>
using threadsafe::is_lifetime_aware_v;
struct Borrow { int* p; };

// CLAUDE.md: "is_lifetime_aware<T> ... Ownership is **transitive**"
static_assert(!is_lifetime_aware_v<Borrow>, "a raw pointer member borrows");

// unique_ptr HONOURS transitivity: it asks the pointee.
static_assert(!is_lifetime_aware_v<std::unique_ptr<Borrow>>,
              "unique_ptr<Borrow> is correctly NOT lifetime aware");

// shared_ptr / weak_ptr DISCARD it: std::true_type regardless of T.
static_assert(is_lifetime_aware_v<std::shared_ptr<Borrow>>,
              "shared_ptr<Borrow> IS lifetime aware -- transitivity broken");
static_assert(is_lifetime_aware_v<std::weak_ptr<Borrow>>,
              "weak_ptr<Borrow> likewise");

// The existing suite pins the shared_ptr behaviour deliberately:
static_assert(is_lifetime_aware_v<std::shared_ptr<std::span<int>>>,
              "tests/test_lifetime_aware.cpp asserts this ON PURPOSE");
