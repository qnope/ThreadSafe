#include <threadsafe/threadsafe.h>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
consteval void explain() { threadsafe::assert_synchronizable<const std::array<int*,3>>(); }
static_assert((explain(), true));
