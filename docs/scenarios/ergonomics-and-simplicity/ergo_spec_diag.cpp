#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
consteval bool ask() { threadsafe::assert_sendable<std::unique_ptr<int[]>>(); return true; }
static_assert(ask());
