#include <threadsafe/threadsafe.h>
#include <functional>
#include <latch>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace { struct Borrowing { int *p; }; }

static_assert((threadsafe::assert_sendable<std::vector<int*>>(), true));
static_assert((threadsafe::assert_sendable<std::vector<Borrowing>>(), true));
static_assert((threadsafe::assert_sendable<std::map<int, int*>>(), true));
static_assert((threadsafe::assert_sendable<std::optional<int*>>(), true));
static_assert((threadsafe::assert_sendable<std::tuple<int, int*>>(), true));
static_assert((threadsafe::assert_sendable<std::pair<int, int*>>(), true));
static_assert((threadsafe::assert_sendable<std::reference_wrapper<int>>(), true));
static_assert((threadsafe::assert_sendable<std::shared_ptr<int>>(), true));
static_assert((threadsafe::assert_sendable<std::unique_ptr<int*>>(), true));
static_assert((threadsafe::assert_sendable<std::weak_ptr<int>>(), true));
static_assert((threadsafe::assert_synchronizable<const std::vector<int*>>(), true));
static_assert((threadsafe::assert_synchronizable<const std::shared_ptr<int>>(), true));
static_assert((threadsafe::assert_lifetime_aware<std::vector<int*>>(), true));
static_assert((threadsafe::assert_lifetime_aware<std::optional<std::string_view>>(), true));
int main() {}
