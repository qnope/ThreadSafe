#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <memory>
namespace {
struct NoopDeleter { void operator()(const int*) const noexcept {} };
struct RawBorrow  { const int *observed; };
struct UptrBorrow { std::unique_ptr<const int, NoopDeleter> observed; };
}
using threadsafe::is_lifetime_aware_v; using threadsafe::is_sendable_v; using threadsafe::is_synchronizable_v;
int main() {
  std::printf("RawBorrow : sendable=%d lifetime_aware=%d const_sync=%d\n",
    (int)is_sendable_v<RawBorrow>, (int)is_lifetime_aware_v<RawBorrow>, (int)is_synchronizable_v<const RawBorrow>);
  std::printf("UptrBorrow: sendable=%d lifetime_aware=%d const_sync=%d\n",
    (int)is_sendable_v<UptrBorrow>, (int)is_lifetime_aware_v<UptrBorrow>, (int)is_synchronizable_v<const UptrBorrow>);
}
