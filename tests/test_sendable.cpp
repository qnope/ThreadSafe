#include <threadsafe/sendable.h>

#include <threadsafe/synchronizable.h>

#include <cstddef>

namespace {

struct SyncType {};

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

struct ExplicitlyDefaulted {
    ExplicitlyDefaulted(const ExplicitlyDefaulted&) = default;
    ExplicitlyDefaulted(ExplicitlyDefaulted&&) = default;
    ExplicitlyDefaulted& operator=(const ExplicitlyDefaulted&) = default;
    ExplicitlyDefaulted& operator=(ExplicitlyDefaulted&&) = default;
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
static_assert(is_sendable<DeletedCopy>,
              "is_sendable — a deleted copy constructor does not block: deleting "
              "an operation cannot introduce sharing");

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
