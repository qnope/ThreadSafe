#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

consteval bool ask() { threadsafe::assert_sendable<std::shared_ptr<const std::string>>(); return true; }
static_assert(ask());
