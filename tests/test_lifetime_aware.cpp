#include <threadsafe/threadsafe.h>

#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct Own {
    int v;
};
}

using threadsafe::is_lifetime_aware_v;

static_assert(is_lifetime_aware_v<int>,
              "is_lifetime_aware — a value owns its data");
static_assert(is_lifetime_aware_v<std::string>,
              "is_lifetime_aware — a container owns its data");
static_assert(is_lifetime_aware_v<std::vector<int>>,
              "is_lifetime_aware — a container owns its data");
static_assert(is_lifetime_aware_v<Own>,
              "is_lifetime_aware — a user struct owns its data");

static_assert(!is_lifetime_aware_v<std::span<int>>,
              "is_lifetime_aware — borrowed ranges do not own their data");
static_assert(!is_lifetime_aware_v<std::string_view>,
              "is_lifetime_aware — borrowed ranges do not own their data");
static_assert(!is_lifetime_aware_v<std::ranges::subrange<int*>>,
              "is_lifetime_aware — borrowed ranges do not own their data");

static_assert(!is_lifetime_aware_v<int&>,
              "is_lifetime_aware — T& does not keep its referent alive");
static_assert(!is_lifetime_aware_v<std::string&&>,
              "is_lifetime_aware — T&& does not keep its referent alive");
static_assert(!is_lifetime_aware_v<int*>,
              "is_lifetime_aware — T* does not keep its referent alive");
static_assert(!is_lifetime_aware_v<const int*>,
              "is_lifetime_aware — T* does not keep its referent alive");
static_assert(!is_lifetime_aware_v<std::reference_wrapper<int>>,
              "is_lifetime_aware — reference_wrapper does not keep its referent alive");

static_assert(is_lifetime_aware_v<int[4]>,
              "is_lifetime_aware — a bounded array owns its elements");
static_assert(is_lifetime_aware_v<int[]>,
              "is_lifetime_aware — an unbounded array owns its elements");
static_assert(!is_lifetime_aware_v<std::span<int>[4]>,
              "is_lifetime_aware — an array of a non-owning type is not lifetime aware");
static_assert(!is_lifetime_aware_v<std::span<int>[]>,
              "is_lifetime_aware — an array of a non-owning type is not lifetime aware");

static_assert(is_lifetime_aware_v<std::shared_ptr<int>>,
              "is_lifetime_aware — shared_ptr keeps its referent alive");
static_assert(is_lifetime_aware_v<std::weak_ptr<int>>,
              "is_lifetime_aware — weak_ptr keeps its control block alive");

static_assert(is_lifetime_aware_v<std::shared_ptr<std::span<int>>>,
              "is_lifetime_aware — an owner of a borrowed range is still an owner");
static_assert(!is_lifetime_aware_v<std::vector<int>&>,
              "is_lifetime_aware — the T& rule beats the by-value rule");
