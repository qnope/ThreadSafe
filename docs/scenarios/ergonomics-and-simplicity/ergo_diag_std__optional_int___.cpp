#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

consteval bool ask() { threadsafe::assert_sendable<std::optional<int*>>(); return true; }
static_assert(ask());
