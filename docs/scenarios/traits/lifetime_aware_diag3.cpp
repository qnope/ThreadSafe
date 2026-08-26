#include <threadsafe/threadsafe.h>
#include <memory>
#include <span>
#include <vector>
#include <future>
struct Poly { virtual ~Poly() = default; };
consteval void p1() { threadsafe::assert_lifetime_aware<std::shared_ptr<std::span<int>>>(); }
consteval void p2() { threadsafe::assert_lifetime_aware<std::shared_ptr<Poly>>(); }
consteval void p3() { threadsafe::assert_lifetime_aware<std::vector<int*>>(); }
consteval void p4() { threadsafe::assert_lifetime_aware<std::shared_ptr<void>>(); }
consteval void p5() { threadsafe::assert_lifetime_aware<std::future<int>>(); }
consteval void p6() { threadsafe::assert_lifetime_aware<std::reference_wrapper<int>>(); }
static_assert((p1(), true)); static_assert((p2(), true)); static_assert((p3(), true));
static_assert((p4(), true)); static_assert((p5(), true)); static_assert((p6(), true));
int main(){}
