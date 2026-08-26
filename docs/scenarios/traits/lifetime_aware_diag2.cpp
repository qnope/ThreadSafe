#include <threadsafe/threadsafe.h>
#include <memory>
#include <span>
#include <vector>
struct Poly { virtual ~Poly() = default; };
consteval void probe1() { threadsafe::assert_lifetime_aware<std::shared_ptr<std::span<int>>>(); }
consteval void probe2() { threadsafe::assert_lifetime_aware<std::shared_ptr<Poly>>(); }
consteval void probe3() { threadsafe::assert_lifetime_aware<std::vector<int*>>(); }
consteval void probe4() { threadsafe::assert_lifetime_aware<std::shared_ptr<void>>(); }
static_assert((probe1(), true));
static_assert((probe2(), true));
static_assert((probe3(), true));
static_assert((probe4(), true));
int main(){}
