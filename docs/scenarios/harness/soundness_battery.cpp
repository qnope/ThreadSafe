#include <threadsafe/threadsafe.h>
#include <chrono>
#include <complex>
#include <bitset>
#include <random>
#include <span>
#include <string_view>
#include <functional>
#include <any>
#include <exception>
#include <locale>
#include <thread>
#include <memory>
#include <vector>
#include <utility>
#include <cstdio>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

struct ForwardingCtor { int x=0; ForwardingCtor()=default; template<class U> ForwardingCtor(U&& o):x(o.x){} };
struct ForwardingAssign { int x=0; template<class U> ForwardingAssign& operator=(U&& o){x=o.x;return *this;} };
struct EmptyUserCopy { EmptyUserCopy(const EmptyUserCopy&){} };
struct DerivesFromEmptyUserCopy : EmptyUserCopy {};
struct ConstRefCtorTemplate { int x=0; ConstRefCtorTemplate()=default; template<class U> ConstRefCtorTemplate(const U& o):x(o.x){} };
struct GuardedForwardingCtor { int x=0; GuardedForwardingCtor()=default;
  template<class U> requires(!std::same_as<std::remove_cvref_t<U>,GuardedForwardingCtor>)
  GuardedForwardingCtor(U&& o):x(o.x){} };
struct RawPtrHolder { int* p; };
struct MutableIntHolder { mutable int m; };
struct Poly { virtual ~Poly(); int x; };

#define SHOW(T) std::printf("%-38s sendable=%-5s constsync=%s\n", #T, \
   is_sendable_v<T> ? "true":"false", is_synchronizable_v<const T> ? "true":"false")

int main(){
  SHOW(std::chrono::milliseconds);
  SHOW(std::complex<double>);
  SHOW(std::bitset<64>);
  SHOW(std::mt19937);
  SHOW(ForwardingCtor);
  SHOW(ForwardingAssign);
  SHOW(EmptyUserCopy);
  SHOW(DerivesFromEmptyUserCopy);
  SHOW(ConstRefCtorTemplate);
  SHOW(GuardedForwardingCtor);
  SHOW(RawPtrHolder);
  SHOW(MutableIntHolder);
  SHOW(Poly);
  SHOW(std::span<int>);
  SHOW(std::string_view);
  SHOW(std::reference_wrapper<int>);
  SHOW(std::function<void()>);
  SHOW(std::any);
  SHOW(std::exception_ptr);
  SHOW(std::locale);
  SHOW(std::thread);
  SHOW(std::shared_ptr<int>);
  using Cap = decltype([x=42]{});
  SHOW(Cap);
  using CapPtr = decltype([p=(int*)nullptr]{});
  SHOW(CapPtr);
  SHOW(std::vector<std::chrono::milliseconds>);
  using PairIS = std::pair<int, std::chrono::seconds>;
  SHOW(PairIS);
}
