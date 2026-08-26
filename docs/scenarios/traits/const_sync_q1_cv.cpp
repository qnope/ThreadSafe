#include <threadsafe/threadsafe.h>
#include <atomic>
struct Plain { int value_; };
struct Bad   { int *borrowed_; };
using threadsafe::is_synchronizable_v;
static_assert(!is_synchronizable_v<const volatile Plain>, "cv Plain TRUE");
static_assert(!is_synchronizable_v<volatile Plain>,       "v Plain TRUE");
static_assert(!is_synchronizable_v<const Plain &>,        "const Plain& TRUE");
static_assert(!is_synchronizable_v<Plain &>,              "Plain& TRUE");
static_assert(!is_synchronizable_v<std::atomic<int> &>,   "atomic& TRUE");
static_assert(!is_synchronizable_v<const Plain *>,        "const Plain* TRUE");
static_assert(!is_synchronizable_v<const Plain[4]>,       "const Plain[4] TRUE");
static_assert(!is_synchronizable_v<const volatile Plain[4]>, "cv Plain[4] TRUE");
static_assert(!is_synchronizable_v<const Plain[]>,        "const Plain[] TRUE");
static_assert(!is_synchronizable_v<const void>,           "const void TRUE");
static_assert(!is_synchronizable_v<void>,                 "void TRUE");
static_assert(!is_synchronizable_v<const Bad[4]>,         "const Bad[4] TRUE");
static_assert(!is_synchronizable_v<const int[2][3]>,      "const int[2][3] TRUE");
static_assert(!is_synchronizable_v<std::atomic<int> const[4]>, "const atomic[4] TRUE");
static_assert(!is_synchronizable_v<const std::atomic<int>>,   "const atomic TRUE");
static_assert(!is_synchronizable_v<volatile std::atomic<int>>,"volatile atomic TRUE");
static_assert(!is_synchronizable_v<const void()>,         "const fn TRUE");
static_assert(!is_synchronizable_v<void() const>,         "fn const TRUE");
