#include <threadsafe/threadsafe.h>
#include <string>
#include <string_view>
#include <vector>

using threadsafe::copy_on_write;
using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

struct Holder { copy_on_write<int> body; };

// --- what the const question says about the cow itself ---
static_assert(!is_synchronizable_v<copy_on_write<int>>);
static_assert(!is_synchronizable_v<const copy_on_write<int>>,
              "FALSE REJECTION: reading a const cow<int> from N threads is the "
              "whole point of the type");
static_assert(!is_synchronizable_v<const copy_on_write<std::string>>);

// --- consequence: a struct holding a cow can never be const-shared ---
static_assert(is_sendable_v<Holder>);
static_assert(!is_synchronizable_v<const Holder>);

// --- and a const cow reference cannot be handed to a thread ---
static_assert(!is_sendable_v<const copy_on_write<int>&>);
static_assert(!is_sendable_v<copy_on_write<int>&>);
static_assert(!is_sendable_v<copy_on_write<int>*>);

// --- lifetime ---
static_assert(is_lifetime_aware_v<copy_on_write<std::string>>);
static_assert(!is_lifetime_aware_v<copy_on_write<int*>>);
static_assert(!is_lifetime_aware_v<copy_on_write<std::string_view>>);
static_assert(is_lifetime_aware_v<copy_on_write<std::vector<int>>>);

// --- is the explicit is_sendable specialization reached for cv forms? ---
static_assert(is_sendable_v<copy_on_write<int>>);
static_assert(is_sendable_v<const copy_on_write<int>>);
static_assert(is_sendable_v<volatile copy_on_write<int>>);
static_assert(is_lifetime_aware_v<const copy_on_write<std::string>>);
int main() {}
