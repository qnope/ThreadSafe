#include <threadsafe/threadsafe.h>

#include <functional>
#include <memory>

namespace {

struct PolyBase {
    virtual ~PolyBase() = default;
};
struct PolyFinal final : PolyBase {};

struct VouchedPolyBase {
    virtual ~VouchedPolyBase() = default;
};
struct VouchedPolyFinal final : VouchedPolyBase {};

struct HoldsVouchedPolyReference {
    VouchedPolyBase& referent;
};
struct HoldsVouchedFinalReference {
    VouchedPolyFinal& referent;
};

} // namespace

template <>
struct threadsafe::is_unsafe_synchronizable<VouchedPolyBase> : std::true_type {
};
template <>
struct threadsafe::is_unsafe_synchronizable<VouchedPolyFinal> : std::true_type {
};

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// By value: a value's dynamic type is its static type — copying slices.
static_assert(is_sendable_v<PolyBase>,
              "is_sendable — a by-value polymorphic object is exactly its "
              "static type");
static_assert(is_synchronizable_v<const PolyBase>,
              "is_synchronizable — a const value the reader holds directly "
              "hides no derived state");
static_assert(is_lifetime_aware_v<PolyBase>,
              "is_lifetime_aware — a by-value polymorphic object owns what its "
              "static type owns");
static_assert(is_sendable_v<PolyFinal> && is_synchronizable_v<const PolyFinal>
                  && is_lifetime_aware_v<PolyFinal>,
              "a final value answers like any other value");

// Lvalue references.
static_assert(!is_sendable_v<const PolyBase&>,
              "is_sendable — a const reference may bind to a derived object "
              "with a mutable member a virtual const function mutates");
static_assert(is_sendable_v<const PolyFinal&>,
              "is_sendable — a final type has no unknown dynamic type");
static_assert(!is_sendable_v<VouchedPolyBase&>,
              "is_sendable — a vouch names a type; it does not cover the "
              "unknown derived objects a reference may bind to");
static_assert(is_sendable_v<VouchedPolyFinal&>,
              "is_sendable — final plus a vouch: the referent is fully known");
static_assert(!is_lifetime_aware_v<PolyBase&>,
              "is_lifetime_aware — a reference owns nothing");
static_assert(!is_synchronizable_v<PolyBase&>,
              "is_synchronizable — only const data may be read from several "
              "threads");

// Rvalue references: unlike passing by value, binding does not slice.
static_assert(!is_sendable_v<PolyBase&&>,
              "is_sendable — an rvalue reference may still refer to a derived "
              "object whose members the walk cannot see");
static_assert(is_sendable_v<PolyFinal&&>,
              "is_sendable — a final referent is exactly its static type");

// Raw pointers: is_sendable<T*> = is_sendable<T&>.
static_assert(!is_sendable_v<const PolyBase*>,
              "is_sendable — a pointer to a polymorphic base may point at an "
              "unknown derived");
static_assert(is_sendable_v<const PolyFinal*>,
              "is_sendable — a final pointee has no unknown dynamic type");
static_assert(!is_synchronizable_v<VouchedPolyBase* const>,
              "is_synchronizable — the const-read walk may not trust a pointee "
              "whose dynamic type is unknown, vouched or not");
static_assert(is_synchronizable_v<VouchedPolyFinal* const>,
              "is_synchronizable — a vouched final pointee is fully known");
static_assert(!is_lifetime_aware_v<PolyBase*>,
              "is_lifetime_aware — a pointer owns nothing");

// A reference member seen by the const-read walk.
static_assert(!is_synchronizable_v<const HoldsVouchedPolyReference>,
              "is_synchronizable — a reference member crosses the same "
              "indirection");
static_assert(is_synchronizable_v<const HoldsVouchedFinalReference>,
              "is_synchronizable — a vouched final referent is fully known");

// std::reference_wrapper: a copyable T&.
static_assert(!is_sendable_v<std::reference_wrapper<VouchedPolyBase>>,
              "is_sendable — reference_wrapper crosses the same indirection "
              "as T&");
static_assert(is_sendable_v<std::reference_wrapper<VouchedPolyFinal>>,
              "is_sendable — a vouched final referent is fully known");
static_assert(
    !is_synchronizable_v<const std::reference_wrapper<VouchedPolyBase>>,
    "is_synchronizable — const on the wrapper does not pin the referent's "
    "dynamic type");
static_assert(
    is_synchronizable_v<const std::reference_wrapper<VouchedPolyFinal>>,
    "is_synchronizable — a vouched final referent is fully known");
static_assert(!is_lifetime_aware_v<std::reference_wrapper<PolyFinal>>,
              "is_lifetime_aware — a reference_wrapper owns nothing, final or "
              "not");

// std::unique_ptr.
static_assert(!is_sendable_v<std::unique_ptr<PolyBase>>,
              "is_sendable — the dynamic type behind a non-final polymorphic "
              "base is unknown");
static_assert(is_sendable_v<std::unique_ptr<PolyFinal>>,
              "is_sendable — a final pointee has no unknown dynamic type");
static_assert(!is_sendable_v<std::unique_ptr<PolyBase[]>>,
              "is_sendable — an array of polymorphic elements is as unknown as "
              "one element");
static_assert(!is_synchronizable_v<const std::unique_ptr<const PolyBase>>,
              "is_synchronizable — a derived pointee may hold a mutable "
              "member");
static_assert(is_synchronizable_v<const std::unique_ptr<const PolyFinal>>,
              "is_synchronizable — a final pointee is fully known");
static_assert(!is_lifetime_aware_v<std::unique_ptr<PolyBase>>,
              "is_lifetime_aware — a derived object may borrow what the base "
              "does not");
static_assert(is_lifetime_aware_v<std::unique_ptr<PolyFinal>>,
              "is_lifetime_aware — a final pointee owns what it says it owns");
static_assert(!is_lifetime_aware_v<std::unique_ptr<PolyBase[]>>,
              "is_lifetime_aware — the element type carries the answer, arrays "
              "included");

// std::shared_ptr / std::weak_ptr.
static_assert(!is_sendable_v<std::shared_ptr<VouchedPolyBase>>,
              "is_sendable — sharing multiplies readers of an object of "
              "unknown dynamic type");
static_assert(is_sendable_v<std::shared_ptr<VouchedPolyFinal>>,
              "is_sendable — a vouched final pointee is fully known");
static_assert(!is_sendable_v<std::weak_ptr<VouchedPolyBase>>,
              "is_sendable — a weak_ptr locks into the same unknown object");
static_assert(is_sendable_v<std::weak_ptr<VouchedPolyFinal>>,
              "is_sendable — a vouched final pointee is fully known");
static_assert(!is_synchronizable_v<const std::shared_ptr<VouchedPolyBase>>,
              "is_synchronizable — const on the shared_ptr does not pin the "
              "pointee's dynamic type");
static_assert(is_synchronizable_v<const std::shared_ptr<VouchedPolyFinal>>,
              "is_synchronizable — a vouched final pointee is fully known");
static_assert(!is_synchronizable_v<const std::weak_ptr<VouchedPolyBase>>,
              "is_synchronizable — a weak_ptr locks into the same unknown "
              "object");
static_assert(!is_lifetime_aware_v<std::shared_ptr<PolyBase>>,
              "is_lifetime_aware — shared ownership does not make the derived "
              "object an owner");
static_assert(is_lifetime_aware_v<std::shared_ptr<PolyFinal>>,
              "is_lifetime_aware — a final pointee owns what it says it owns");
static_assert(!is_lifetime_aware_v<std::weak_ptr<PolyBase>>,
              "is_lifetime_aware — a weak_ptr locks into the same unknown "
              "object");
