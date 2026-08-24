#include <threadsafe/threadsafe.h>
#include <cstdio>
using threadsafe::is_sendable_v;

// Six-trivial types that DO share state -- must stay rejected under the fix.
struct AnonUnionPtr { union { int* p; long l; }; };
struct PrivatePtr { private: int* p; };
struct BasePtr { int* p; };
struct DerivesPrivatelyFromPtr : private BasePtr {};
struct ArrayOfPtr { int* a[3]; };
struct EmptyWithCtorTemplate { template<class U> EmptyWithCtorTemplate(U&&){} EmptyWithCtorTemplate()=default; };
struct RefWrapLike { int* p; template<class U> RefWrapLike(const U& u):p(&u.v){} };
// A ctor template that hijacks via T& -- must stay rejected.
struct Hijacker { int x=0; Hijacker()=default; template<class U> Hijacker(U&& o):x(o.x+1){} };
// Defaulted-out-of-class copy (user-provided but trivial? no) -- sanity
struct DefaultedOutOfLine { int x; DefaultedOutOfLine(const DefaultedOutOfLine&); };
DefaultedOutOfLine::DefaultedOutOfLine(const DefaultedOutOfLine&) = default;

#define SHOW(T) std::printf("%-32s sendable=%s\n", #T, is_sendable_v<T> ? "TRUE  <-- check":"false")
int main(){
  SHOW(AnonUnionPtr); SHOW(PrivatePtr); SHOW(DerivesPrivatelyFromPtr);
  SHOW(ArrayOfPtr); SHOW(EmptyWithCtorTemplate); SHOW(RefWrapLike);
  SHOW(Hijacker); SHOW(DefaultedOutOfLine);
}
