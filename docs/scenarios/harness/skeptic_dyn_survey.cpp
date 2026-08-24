#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <memory>
#include <functional>
namespace {
struct PolyBase { virtual ~PolyBase() = default; virtual int f() const = 0; };
struct PolyDerived final : PolyBase { mutable int memo = -1; int f() const override { return memo = 1; } };
struct NonFinalConcrete { virtual ~NonFinalConcrete() = default; virtual int f() const { return 0; } };
struct HoldsUnique { std::unique_ptr<const PolyBase> p; };
}
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
#define ROW(T) std::printf("%-46s const-sync=%d  sendable=%d\n", #T, (int)is_synchronizable_v<const T>, (int)is_sendable_v<T>)
int main() {
    ROW(PolyBase);
    ROW(NonFinalConcrete);
    ROW(std::unique_ptr<const PolyBase>);
    ROW(std::unique_ptr<const NonFinalConcrete>);
    ROW(std::unique_ptr<const PolyDerived>);
    ROW(HoldsUnique);
    ROW(std::unique_ptr<const int>);
    ROW(std::unique_ptr<const int[]>);
    return 0;
}
