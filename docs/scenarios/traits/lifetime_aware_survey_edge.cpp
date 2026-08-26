#include <threadsafe/threadsafe.h>
#include <span>
#include <stop_token>
#include <memory>
#include <vector>

using threadsafe::is_lifetime_aware_v;
#define YES(NAME, ...) static_assert(is_lifetime_aware_v<__VA_ARGS__>, "GOT-FALSE: " NAME);
#define NO(NAME, ...)  static_assert(!is_lifetime_aware_v<__VA_ARGS__>, "GOT-TRUE: " NAME);

struct Incomplete;
struct VirtualBaseBorrow { std::span<int> s; };
struct DerivedVirtual : virtual VirtualBaseBorrow {};
struct PrivateBorrow { private: int *p; public: PrivateBorrow(); };
struct Bitfields { int a : 3; unsigned b : 5; };
struct NoUniqueAddress { [[no_unique_address]] std::span<int> s; int v; };
struct StopCb { void operator()() const {} };

NO("incomplete type", Incomplete)
NO("virtual base that borrows", DerivedVirtual)
NO("private pointer member", PrivateBorrow)
YES("bitfields", Bitfields)
NO("[[no_unique_address]] span", NoUniqueAddress)
NO("std::stop_callback", std::stop_callback<StopCb>)
YES("std::stop_token", std::stop_token)
YES("std::stop_source", std::stop_source)
NO("void", void)
YES("const volatile int", const volatile int)
NO("shared_ptr<int[]> ? (expect TRUE, this line pins it)", std::shared_ptr<int[]>)
int main(){}
