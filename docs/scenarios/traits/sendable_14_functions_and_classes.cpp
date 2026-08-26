#include <threadsafe/threadsafe.h>
#include <type_traits>

namespace {
struct EmptyUserDefaultCtor { EmptyUserDefaultCtor(); };
struct EmptyUserDefaultCtorDefaultedCopy {
    EmptyUserDefaultCtorDefaultedCopy();
    EmptyUserDefaultCtorDefaultedCopy(const EmptyUserDefaultCtorDefaultedCopy&) = default;
};
struct StatefulUserDefaultCtor { int v; StatefulUserDefaultCtor(); };
struct DeletedCopyDefaultedMove {
    int v;
    DeletedCopyDefaultedMove(const DeletedCopyDefaultedMove&) = delete;
    DeletedCopyDefaultedMove(DeletedCopyDefaultedMove&&) = default;
};
struct DeletedEverything {
    int v;
    DeletedEverything(const DeletedEverything&) = delete;
    DeletedEverything& operator=(const DeletedEverything&) = delete;
    ~DeletedEverything() = delete;
};
struct UserMoveOnly {
    int v;
    UserMoveOnly(UserMoveOnly&&);
};
struct PolyNoData { virtual void f(); };
struct PolyDefaultedDtor { virtual ~PolyDefaultedDtor() = default; };
struct AnonBitfieldOnly { int : 32; };
struct NamedBitfield { int a : 3; int b : 5; };
}

using threadsafe::is_sendable_v;

// function types
static_assert(is_sendable_v<void()>);
static_assert(is_sendable_v<int(int, double)>);
static_assert(is_sendable_v<void(...)>);
static_assert(is_sendable_v<void() noexcept>);
static_assert(std::is_function_v<void() const>);
static_assert(is_sendable_v<void() const>, "abominable function type");
static_assert(is_sendable_v<void() const & noexcept>);
static_assert(is_sendable_v<void(int...) volatile &&>);

// classes
static_assert(is_sendable_v<EmptyUserDefaultCtor>);
static_assert(is_sendable_v<EmptyUserDefaultCtorDefaultedCopy>);
static_assert(is_sendable_v<StatefulUserDefaultCtor>);
static_assert(is_sendable_v<DeletedCopyDefaultedMove>);
static_assert(is_sendable_v<DeletedEverything>);
static_assert(!is_sendable_v<UserMoveOnly>);
static_assert(is_sendable_v<PolyNoData>);
static_assert(is_sendable_v<PolyDefaultedDtor>);
static_assert(is_sendable_v<NamedBitfield>);
static_assert(is_sendable_v<AnonBitfieldOnly>, "unnamed bit-field only");
