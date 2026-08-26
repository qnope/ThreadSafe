#include <threadsafe/threadsafe.h>

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
struct SyncType {};
struct MutCache { int raw; mutable int parsed; };
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

#define P(NAME, ...) static_assert(__VA_ARGS__, "PROBE " NAME)

// ---- std::array
P("S array<int,0>",       is_sendable_v<std::array<int,0>>);
P("S array<int*,0>",      is_sendable_v<std::array<int*,0>>);
P("CS const array<int,0>",  is_synchronizable_v<const std::array<int,0>>);
P("CS const array<int*,0>", is_synchronizable_v<const std::array<int*,0>>);
P("L array<int*,0>",      is_lifetime_aware_v<std::array<int*,0>>);
P("L array<int,0>",       is_lifetime_aware_v<std::array<int,0>>);
P("S array<MutCache,3>",  is_sendable_v<std::array<MutCache,3>>);
P("CS const array<MutCache,3>", is_synchronizable_v<const std::array<MutCache,3>>);

// ---- std::variant
P("S variant<int,string>",     is_sendable_v<std::variant<int,std::string>>);
P("S variant<int,int*>",       is_sendable_v<std::variant<int,int*>>);
P("S variant<monostate,int>",  is_sendable_v<std::variant<std::monostate,int>>);
P("S monostate",               is_sendable_v<std::monostate>);
P("CS const monostate",        is_synchronizable_v<const std::monostate>);
P("L monostate",               is_lifetime_aware_v<std::monostate>);
P("S variant<int,ref_wrapper<int>>", is_sendable_v<std::variant<int, std::reference_wrapper<int>>>);
P("S variant<int,ref_wrapper<Sync>>", is_sendable_v<std::variant<int, std::reference_wrapper<SyncType>>>);
P("L variant<int,ref_wrapper<Sync>>", is_lifetime_aware_v<std::variant<int, std::reference_wrapper<SyncType>>>);
P("CS const variant<int,string>", is_synchronizable_v<const std::variant<int,std::string>>);
P("CS const variant<int,MutCache>", is_synchronizable_v<const std::variant<int,MutCache>>);

// ---- vector<bool>
P("S vector<bool>",        is_sendable_v<std::vector<bool>>);
P("CS const vector<bool>", is_synchronizable_v<const std::vector<bool>>);
P("L vector<bool>",        is_lifetime_aware_v<std::vector<bool>>);
P("S bool",                is_sendable_v<bool>);
P("CS const bool",         is_synchronizable_v<const bool>);

// ---- pair<const K, V> as stored inside a map
P("S pair<const int,string>",  is_sendable_v<std::pair<const int,std::string>>);
P("CS const pair<const int,string>", is_synchronizable_v<const std::pair<const int,std::string>>);
P("S pair<const int, int*>",   is_sendable_v<std::pair<const int, int*>>);
P("CS const map<int,MutCache>", is_synchronizable_v<const std::map<int, MutCache>>);
P("CS const map<MutCache,int>", is_synchronizable_v<const std::map<MutCache,int>>);

// ---- vector of vector<bool>
P("S vector<vector<bool>>", is_sendable_v<std::vector<std::vector<bool>>>);

int main() {}
