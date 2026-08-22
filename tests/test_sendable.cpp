#include <threadsafe/sendable.h>

#include <threadsafe/synchronizable.h>

#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>

namespace {

struct SyncType {};

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
constexpr bool threadsafe::is_synchronizable<SyncType> = true;
template <>
constexpr bool threadsafe::is_sendable<OptIn> = true;

using threadsafe::is_sendable;

static_assert(!is_sendable<int&>,
              "is_sendable — sending a reference shares the referent");
static_assert(is_sendable<SyncType&>,
              "is_sendable — a reference to a synchronizable type is sendable");
static_assert(is_sendable<SyncType&&>,
              "is_sendable — an rvalue reference shares the referent too");

static_assert(is_sendable<SyncType>,
              "is_sendable — is_synchronizable<T> implies is_sendable<T>");

static_assert(is_sendable<int>, "is_sendable — arithmetic types are sendable");
static_assert(is_sendable<double>, "is_sendable — arithmetic types are sendable");
static_assert(is_sendable<Color>, "is_sendable — enums are sendable");
static_assert(is_sendable<std::nullptr_t>, "is_sendable — nullptr_t is sendable");
static_assert(is_sendable<void (*)()>,
              "is_sendable — function pointers are sendable");
static_assert(is_sendable<int PlainAggregate::*>,
              "is_sendable — member pointers are sendable");
static_assert(!is_sendable<int*>,
              "is_sendable — sending an object pointer shares the referent");
static_assert(!is_sendable<void*>,
              "is_sendable — sending an object pointer shares the referent");
static_assert(is_sendable<SyncType*>,
              "is_sendable — a pointer to a synchronizable type is sendable");
static_assert(is_sendable<const int>,
              "is_sendable — cv-qualified T forwards to T");
static_assert(!is_sendable<const UserCopyCtor>,
              "is_sendable — cv-qualified T forwards to T");

static_assert(is_sendable<PlainAggregate>,
              "is_sendable — implicitly-declared special members count as defaulted");
static_assert(is_sendable<ExplicitlyDefaulted>,
              "is_sendable — explicitly defaulted special members are fine");
static_assert(!is_sendable<UserCopyCtor>,
              "is_sendable — a user-provided copy constructor blocks default sendability");
static_assert(!is_sendable<UserAssign>,
              "is_sendable — a user-provided copy assignment blocks default sendability");
static_assert(!is_sendable<UserDtor>,
              "is_sendable — the receiving thread destroys what it was sent, so a "
              "user-provided destructor runs there too");
static_assert(is_sendable<DeletedCopy>,
              "is_sendable — a deleted copy constructor does not block: deleting "
              "an operation cannot introduce sharing");

// A constructor or operator= template is never a copy or move member, but it
// can be selected in place of one: deduced from a non-const lvalue it takes
// T&, an exact match the implicit const T& overload cannot beat.
static_assert(!std::is_trivially_copy_constructible_v<ForwardingCtor>
                  || !std::is_trivially_constructible_v<ForwardingCtor,
                                                        ForwardingCtor&>,
              "the forwarding constructor really does win the copy");
static_assert(!is_sendable<ForwardingCtor>,
              "is_sendable — a constructor template can be selected over the "
              "implicit copy constructor, so it runs user code on a copy");
static_assert(!is_sendable<ForwardingAssign>,
              "is_sendable — likewise an operator= template over the implicit "
              "copy assignment");
static_assert(!is_sendable<ConstRefCtorTemplate> && !is_sendable<GuardedForwardingCtor>,
              "is_sendable — parameters_of rejects a template, so a shape that "
              "could never hijack is indistinguishable from one that does");
static_assert(is_sendable<ComparisonTemplate>,
              "is_sendable — only constructor and operator= templates can stand "
              "in for a copy or a move");

static_assert(!is_sendable<HasBadMember>,
              "is_sendable — a non-sendable member makes the class non-sendable");
static_assert(!is_sendable<DerivedBad>,
              "is_sendable — a non-sendable base makes the class non-sendable");
static_assert(is_sendable<DerivedGood>,
              "is_sendable — sendable bases and members make the class sendable");
static_assert(!is_sendable<PrivateBad>,
              "is_sendable — private members are inspected too");
static_assert(is_sendable<IntOrFloat>,
              "is_sendable — unions follow the same defaulted-members rule");
static_assert(!is_sendable<HoldsRef>,
              "is_sendable — a reference member shares its referent, so it is "
              "not sendable");

static_assert(!is_sendable<Node>,
              "is_sendable — recursion terminates on self-referential types");

static_assert(is_sendable<OptIn>,
              "is_sendable — explicit specialization beats the computed default");

// Callables. There is no separate is_safe_callable: handing a callable to
// another thread is sending it, so these are the same rules as above applied to
// class types that happen to have an operator().
static_assert(is_sendable<void()>,
              "is_sendable — function types are synchronizable, hence sendable");
static_assert(is_sendable<void (*const)()>,
              "is_sendable — a cv-qualified function pointer forwards to T*, "
              "which forwards to is_synchronizable on the function type");
static_assert(is_sendable<void (EmptyCallable::*)()>,
              "is_sendable — a member function pointer is a scalar; the trait "
              "states safety, not invocability");

static_assert(is_sendable<decltype([] {})>,
              "is_sendable — a captureless lambda is empty: nothing to inspect "
              "and nothing to race on");
static_assert(is_sendable<decltype([](auto) {})>,
              "is_sendable — a captureless generic lambda has no state either");
static_assert(is_sendable<decltype([]() mutable {})>,
              "is_sendable — mutable changes nothing when there is no state to "
              "mutate");
static_assert(!is_sendable<decltype([x = 42] {})>,
              "is_sendable — a capturing closure reflects no members, so its "
              "captures are state the recursion cannot inspect");
static_assert(!is_sendable<decltype([p = static_cast<int*>(nullptr)] {})>,
              "is_sendable — least of all a capture that borrows");

static_assert(is_sendable<EmptyCallable>,
              "is_sendable — an empty class has no per-object state");
static_assert(is_sendable<EmptyTag>,
              "is_sendable — emptiness is what matters, not invocability");
static_assert(is_sendable<StatefulCallable>,
              "is_sendable — declared per-object state is fine when the callable "
              "is handed over rather than shared");
static_assert(!is_sendable<std::function<void()>>,
              "is_sendable — std::function has a user-provided copy");
