#include <threadsafe/threadsafe.h>

#include <atomic>
#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>

namespace {

struct SyncType {};

struct MutableCounters {
    mutable std::atomic<int> slots[4];
};

struct EmptyCallable {
    void operator()() const {}
};

struct EmptyTag {};

struct StatefulCallable {
    int state = 0;
    void operator()() { ++state; }
};

struct PlainAggregate {
    int a;
    double b;
};

struct UserCopyCtor {
    UserCopyCtor(const UserCopyCtor&);
};

struct UserAssign {
    UserAssign& operator=(const UserAssign&);
};

struct DeletedCopy {
    DeletedCopy(const DeletedCopy&) = delete;
};

struct UserDtor {
    ~UserDtor();
};

struct ForwardingCtor {
    int x = 0;
    ForwardingCtor() = default;
    template <class U>
    ForwardingCtor(U&& other) : x(other.x) {}
};

struct ConstRefCtorTemplate {
    int x = 0;
    ConstRefCtorTemplate() = default;
    template <class U>
    ConstRefCtorTemplate(const U& other) : x(other.x) {}
};

struct GuardedForwardingCtor {
    int x = 0;
    GuardedForwardingCtor() = default;
    template <class U>
        requires(!std::same_as<std::remove_cvref_t<U>, GuardedForwardingCtor>)
    GuardedForwardingCtor(U&& other) : x(other.x) {}
};

struct ForwardingAssign {
    int x = 0;
    template <class U>
    ForwardingAssign& operator=(U&& other) {
        x = other.x;
        return *this;
    }
};

struct ComparisonTemplate {
    int x = 0;
    template <class U>
    bool operator==(const U& other) const {
        return x == other.x;
    }
};

struct ExplicitlyDefaulted {
    ExplicitlyDefaulted(const ExplicitlyDefaulted&) = default;
    ExplicitlyDefaulted(ExplicitlyDefaulted&&) = default;
    ExplicitlyDefaulted& operator=(const ExplicitlyDefaulted&) = default;
    ExplicitlyDefaulted& operator=(ExplicitlyDefaulted&&) = default;
    ~ExplicitlyDefaulted() = default;
};

class PrivateBad {
    UserCopyCtor m_;
};

struct DerivedGood : PlainAggregate {};
struct DerivedBad : UserCopyCtor {};

struct HasBadMember {
    UserCopyCtor m;
};

struct Node {
    Node* next;
    int v;
};

struct HoldsRef {
    int& r;
};

union IntOrFloat {
    int i;
    float f;
};

struct OptIn {
    OptIn(const OptIn&);
};

enum class Color { red, green };

}

template <>
struct threadsafe::is_unsafe_synchronizable<SyncType> {
    static consteval threadsafe::TraitAnswer diagnose() {
        return {};
    }
};
template <>
struct threadsafe::is_unsafe_sendable<OptIn> {
    static consteval threadsafe::TraitAnswer diagnose() {
        return {};
    }
};

using threadsafe::is_sendable_v;

static_assert(!is_sendable_v<int&>,
              "is_sendable — sending a reference shares the referent");
static_assert(is_sendable_v<SyncType&>,
              "is_sendable — a reference to a synchronizable type is sendable");
static_assert(is_sendable_v<SyncType&&>,
              "is_sendable — an rvalue reference shares the referent too");

static_assert(is_sendable_v<SyncType>,
              "is_sendable — is_synchronizable_v<T> implies is_sendable_v<T>");

static_assert(is_sendable_v<int>, "is_sendable — arithmetic types are sendable");
static_assert(is_sendable_v<double>, "is_sendable — arithmetic types are sendable");
static_assert(is_sendable_v<Color>, "is_sendable — enums are sendable");
static_assert(is_sendable_v<std::nullptr_t>, "is_sendable — nullptr_t is sendable");
static_assert(is_sendable_v<void (*)()>,
              "is_sendable — function pointers are sendable");
static_assert(is_sendable_v<int PlainAggregate::*>,
              "is_sendable — member pointers are sendable");
static_assert(!is_sendable_v<int*>,
              "is_sendable — sending an object pointer shares the referent");
static_assert(!is_sendable_v<void*>,
              "is_sendable — sending an object pointer shares the referent");
static_assert(is_sendable_v<SyncType*>,
              "is_sendable — a pointer to a synchronizable type is sendable");
static_assert(is_sendable_v<std::atomic<int> (*)[4]>,
              "is_sendable — a pointer to an array shares the array, so the "
              "element's synchronizability decides");
static_assert(!is_sendable_v<int (*)[4]>,
              "is_sendable — a pointer to an array shares the array, so the "
              "element's synchronizability decides");
static_assert(is_sendable_v<threadsafe::copy_on_write<MutableCounters>>,
              "is_sendable — a mutable array member is writable through const, "
              "so the shared read asks the element's full synchronizability");
static_assert(is_sendable_v<const int>,
              "is_sendable — cv-qualified T forwards to T");
static_assert(!is_sendable_v<const UserCopyCtor>,
              "is_sendable — cv-qualified T forwards to T");

static_assert(is_sendable_v<PlainAggregate>,
              "is_sendable — implicitly-declared special members count as defaulted");
static_assert(is_sendable_v<ExplicitlyDefaulted>,
              "is_sendable — explicitly defaulted special members are fine");
static_assert(!is_sendable_v<UserCopyCtor>,
              "is_sendable — a user-provided copy constructor blocks default sendability");
static_assert(!is_sendable_v<UserAssign>,
              "is_sendable — a user-provided copy assignment blocks default sendability");
static_assert(!is_sendable_v<UserDtor>,
              "is_sendable — the receiving thread destroys what it was sent, so a "
              "user-provided destructor runs there too");
static_assert(is_sendable_v<DeletedCopy>,
              "is_sendable — a deleted copy constructor does not block: deleting "
              "an operation cannot introduce sharing");

static_assert(!std::is_trivially_copy_constructible_v<ForwardingCtor>
                  || !std::is_trivially_constructible_v<ForwardingCtor,
                                                        ForwardingCtor&>,
              "the forwarding constructor really does win the copy");
static_assert(!is_sendable_v<ForwardingCtor>,
              "is_sendable — a constructor template can be selected over the "
              "implicit copy constructor, so it runs user code on a copy");
static_assert(!is_sendable_v<ForwardingAssign>,
              "is_sendable — likewise an operator= template over the implicit "
              "copy assignment");
static_assert(!is_sendable_v<ConstRefCtorTemplate> && !is_sendable_v<GuardedForwardingCtor>,
              "is_sendable — parameters_of rejects a template, so a shape that "
              "could never hijack is indistinguishable from one that does");
static_assert(is_sendable_v<ComparisonTemplate>,
              "is_sendable — only constructor and operator= templates can stand "
              "in for a copy or a move");

static_assert(!is_sendable_v<HasBadMember>,
              "is_sendable — a non-sendable member makes the class non-sendable");
static_assert(!is_sendable_v<DerivedBad>,
              "is_sendable — a non-sendable base makes the class non-sendable");
static_assert(is_sendable_v<DerivedGood>,
              "is_sendable — sendable bases and members make the class sendable");
static_assert(!is_sendable_v<PrivateBad>,
              "is_sendable — private members are inspected too");
static_assert(is_sendable_v<IntOrFloat>,
              "is_sendable — unions follow the same defaulted-members rule");
static_assert(!is_sendable_v<HoldsRef>,
              "is_sendable — a reference member shares its referent, so it is "
              "not sendable");

static_assert(!is_sendable_v<Node>,
              "is_sendable — recursion terminates on self-referential types");

static_assert(is_sendable_v<OptIn>,
              "is_sendable — explicit specialization beats the computed default");

static_assert(is_sendable_v<void()>,
              "is_sendable — function types are synchronizable, hence sendable");
static_assert(is_sendable_v<void (*const)()>,
              "is_sendable — a cv-qualified function pointer forwards to T*, "
              "which forwards to is_synchronizable on the function type");
static_assert(is_sendable_v<void (EmptyCallable::*)()>,
              "is_sendable — a member function pointer is a scalar; the trait "
              "states safety, not invocability");

static_assert(is_sendable_v<decltype([] {})>,
              "is_sendable — a captureless lambda is empty: nothing to inspect "
              "and nothing to race on");
static_assert(is_sendable_v<decltype([](auto) {})>,
              "is_sendable — a captureless generic lambda has no state either");
static_assert(is_sendable_v<decltype([]() mutable {})>,
              "is_sendable — mutable changes nothing when there is no state to "
              "mutate");
static_assert(!is_sendable_v<decltype([x = 42] {})>,
              "is_sendable — a capturing closure reflects no members, so its "
              "captures are state the recursion cannot inspect");
static_assert(!is_sendable_v<decltype([p = static_cast<int*>(nullptr)] {})>,
              "is_sendable — least of all a capture that borrows");

static_assert(is_sendable_v<EmptyCallable>,
              "is_sendable — an empty class has no per-object state");
static_assert(is_sendable_v<EmptyTag>,
              "is_sendable — emptiness is what matters, not invocability");
static_assert(is_sendable_v<StatefulCallable>,
              "is_sendable — declared per-object state is fine when the callable "
              "is handed over rather than shared");
static_assert(!is_sendable_v<std::function<void()>>,
              "is_sendable — std::function has a user-provided copy");
