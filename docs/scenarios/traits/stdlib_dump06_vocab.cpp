#include <threadsafe/threadsafe.h>

#include <memory_resource>
#include <span>
#include <stop_token>
#include <string>
#include <type_traits>

namespace {
struct SyncType {};
struct Noop { void operator()() const noexcept {} };
using Cb = std::stop_callback<Noop>;

struct HoldsStopSource { std::stop_source source; };
struct HoldsStopToken  { std::stop_token token; };
struct HoldsCallback   { Cb cb; };

// A char traits class that holds state (never actually stored by basic_string).
struct StatefulTraits : std::char_traits<char> {
    int *locale_table;
};
using StatefulString = std::basic_string<char, StatefulTraits>;
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

#define P(NAME, ...) static_assert(__VA_ARGS__, "PROBE " NAME)

P("S stop_callback",        is_sendable_v<Cb>);
P("CS const stop_callback", is_synchronizable_v<const Cb>);
P("L stop_callback",        is_lifetime_aware_v<Cb>);
P("S HoldsCallback",        is_sendable_v<HoldsCallback>);
P("L HoldsCallback",        is_lifetime_aware_v<HoldsCallback>);
P("CS const HoldsCallback", is_synchronizable_v<const HoldsCallback>);
P("movable stop_callback",  std::is_move_constructible_v<Cb>);

P("S HoldsStopSource",        is_sendable_v<HoldsStopSource>);
P("L HoldsStopSource",        is_lifetime_aware_v<HoldsStopSource>);
P("CS const HoldsStopSource", is_synchronizable_v<const HoldsStopSource>);
P("S HoldsStopToken",         is_sendable_v<HoldsStopToken>);
P("CS const HoldsStopToken",  is_synchronizable_v<const HoldsStopToken>);
P("CS stop_source (nonconst)", is_synchronizable_v<std::stop_source>);

P("S StatefulTraits",   is_sendable_v<StatefulTraits>);
P("S StatefulString",   is_sendable_v<StatefulString>);
P("L StatefulString",   is_lifetime_aware_v<StatefulString>);

P("S span<SyncType>",   is_sendable_v<std::span<SyncType>>);
P("S span<const int>",  is_sendable_v<std::span<const int>>);

P("S pmr::polymorphic_allocator<int>", is_sendable_v<std::pmr::polymorphic_allocator<int>>);
P("L pmr::polymorphic_allocator<int>", is_lifetime_aware_v<std::pmr::polymorphic_allocator<int>>);

P("CS const volatile vector<int>", is_synchronizable_v<const volatile std::vector<int>>);
P("S volatile vector<int>",        is_sendable_v<volatile std::vector<int>>);

int main() {}
