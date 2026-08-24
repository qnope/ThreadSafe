#include <threadsafe/threadsafe.h>
#include <print>
#include <type_traits>

struct SyncType {};
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

// --- unions -----------------------------------------------------------
union EmptyUnion {};
union UnionWithBorrow { int i; int* p; };
struct AnonymousUnionHost { int tag; union { int i; int* p; }; };
struct AnonymousUnionHostSafe { int tag; union { int i; float f; }; };

// --- bit-fields -------------------------------------------------------
struct OnlyUnnamedBitfield { unsigned : 8; };
struct NamedBitfield { unsigned bits : 8; };
struct ZeroWidthOnly { unsigned : 0; };

// --- closures ---------------------------------------------------------
struct EmptyTag {};
using CaptureEmpty      = decltype([e = EmptyTag{}] {});
using CaptureEmptyRef   = decltype([](EmptyTag& e) { return [&e] { (void)e; }; }(std::declval<EmptyTag&>()));
using CapturePointer    = decltype([p = static_cast<int*>(nullptr)] {});
struct WrapsCapturing { decltype([p = static_cast<int*>(nullptr)] {}) f; };
struct DerivesFromCapturing : decltype([p = static_cast<int*>(nullptr)] {}) {};

// --- polymorphism -----------------------------------------------------
struct PolyBase { virtual ~PolyBase() = default; virtual void go() {} };
struct PolyDerived : PolyBase { int* borrowed; };
struct HoldsPolyBaseByValue { PolyBase b; };
struct DerivesPoly : PolyBase {};

// --- misc -------------------------------------------------------------
struct WithVirtualBase : virtual PolyBase {};

#define ROW(...) std::println("{:<34} sendable={:<5} empty={:<5} poly={}", #__VA_ARGS__, \
    threadsafe::is_sendable_v<__VA_ARGS__>, std::is_empty_v<__VA_ARGS__>, std::is_polymorphic_v<__VA_ARGS__>)

int main() {
    ROW(EmptyUnion);
    ROW(UnionWithBorrow);
    ROW(AnonymousUnionHost);
    ROW(AnonymousUnionHostSafe);
    ROW(OnlyUnnamedBitfield);
    ROW(NamedBitfield);
    ROW(ZeroWidthOnly);
    ROW(CaptureEmpty);
    ROW(CaptureEmptyRef);
    ROW(CapturePointer);
    ROW(WrapsCapturing);
    ROW(DerivesFromCapturing);
    ROW(PolyBase);
    ROW(PolyDerived);
    ROW(HoldsPolyBaseByValue);
    ROW(DerivesPoly);
    ROW(WithVirtualBase);
    std::println("sizeof(EmptyUnion)={} sizeof(OnlyUnnamedBitfield)={} sizeof(CaptureEmpty)={}",
                 sizeof(EmptyUnion), sizeof(OnlyUnnamedBitfield), sizeof(CaptureEmpty));
}
