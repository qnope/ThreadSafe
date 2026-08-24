#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <string>

// Every one of these types has an EXPLICIT library specialization answering
// false. assert_* ignores the specialization and walks the structure instead.
consteval bool a() { threadsafe::assert_lifetime_aware<std::reference_wrapper<int>>(); return true; }
consteval bool b() { threadsafe::assert_sendable<std::vector<int *>>(); return true; }
consteval bool c() { threadsafe::assert_sendable<std::shared_ptr<std::vector<int>>>(); return true; }
consteval bool d() { threadsafe::assert_synchronizable<const std::vector<int *>>(); return true; }
consteval bool e() { threadsafe::assert_sendable<threadsafe::copy_on_write<std::vector<int *>>>(); return true; }
consteval bool f() { threadsafe::assert_lifetime_aware<std::unique_ptr<int *>>(); return true; }
