#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>
struct IntPtr { int *ptr; };
struct Middle { IntPtr ptr; };
struct Error  { Middle ptr; };
struct WithRef { int &r; };
struct Mut { mutable int m; };
static_assert((threadsafe::assert_sendable<Error>(), true));
