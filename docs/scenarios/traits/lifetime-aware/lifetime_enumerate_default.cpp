#include <threadsafe/threadsafe.h>

#include <cstddef>
#include <print>

struct Aggregate { int value; };
enum PlainEnum { plain_enum_value };
enum class ScopedEnum { scoped_enum_value };
union PlainUnion { int as_int; double as_double; };

using MemberDataPointer = int Aggregate::*;
using MemberFunctionPointer = void (Aggregate::*)();
using FunctionType = void();
using FunctionReference = void (&)();
using FunctionPointer = void (*)();

template <class T>
void report(const char *label) {
    std::println("{:44} lifetime_aware={} sendable={}", label,
                 threadsafe::is_lifetime_aware_v<T>,
                 threadsafe::is_sendable_v<T>);
}

int main() {
    report<void>("void");
    report<const void>("const void");
    report<volatile void>("volatile void");
    report<FunctionType>("void()");
    report<FunctionReference>("void(&)()");
    report<FunctionPointer>("void(*)()");
    report<MemberDataPointer>("int Aggregate::*");
    report<MemberFunctionPointer>("void (Aggregate::*)()");
    report<PlainEnum>("PlainEnum");
    report<ScopedEnum>("ScopedEnum");
    report<std::nullptr_t>("std::nullptr_t");
    report<PlainUnion>("PlainUnion");
    report<int>("int");
    report<Aggregate>("Aggregate");
    return 0;
}
