#include <threadsafe/threadsafe.h>
#include <meta>
#include <type_traits>
namespace {
struct PaddingOnly { int : 32; };
struct PaddedPair { char a; int : 24; char b; };
struct AnonUnion { union { int i; float f; }; };
struct AnonUnionBorrow { union { int i; int* p; }; };
}
static_assert(!std::is_empty_v<PaddingOnly>);
static_assert(sizeof(PaddingOnly) == 4);
static_assert(std::meta::nonstatic_data_members_of(^^PaddingOnly,
                  std::meta::access_context::unchecked()).empty(),
              "reflection does not list the unnamed bit-field");
static_assert(threadsafe::detail::has_unreflectable_state(^^PaddingOnly),
              "so the closure guard fires on plain padding");
static_assert(!threadsafe::is_sendable_v<PaddingOnly>);
static_assert(threadsafe::is_sendable_v<PaddedPair>, "padding next to real members is fine");
static_assert(threadsafe::is_sendable_v<AnonUnion>);
static_assert(!threadsafe::is_sendable_v<AnonUnionBorrow>);
static_assert((threadsafe::assert_sendable<PaddingOnly>(), true));
