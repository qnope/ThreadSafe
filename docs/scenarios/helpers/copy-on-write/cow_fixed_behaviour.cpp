#include <threadsafe/threadsafe.h>

#include <algorithm>
#include <concepts>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using threadsafe::copy_on_write;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

namespace {
template <class C>
constexpr bool can_detach = requires(C c) { c.as_mutable(); };
template <class C, class Mutation>
constexpr bool can_modify = requires(C c, Mutation m) { c.modify(m); };

struct Cache { int raw; mutable std::optional<int> parsed; };
struct DocumentWithCowMember { copy_on_write<std::string> body; };

struct StealOnCopy {
    int payload = 0;
    StealOnCopy() = default;
    explicit StealOnCopy(int initial) : payload(initial) {}
    StealOnCopy(const StealOnCopy& other) noexcept : payload(other.payload) {}
    StealOnCopy(StealOnCopy& other) noexcept : payload(other.payload) {
        other.payload = 0;
    }
};
}

template <> struct threadsafe::is_sendable<StealOnCopy> : std::true_type {};
template <> struct threadsafe::is_synchronizable<const StealOnCopy> : std::true_type {};

// composition restored
static_assert(is_synchronizable_v<const copy_on_write<std::string>>);
static_assert(!is_synchronizable_v<copy_on_write<std::string>>);
static_assert(!is_synchronizable_v<const copy_on_write<Cache>>);
static_assert(is_sendable_v<copy_on_write<copy_on_write<std::string>>>);
static_assert(is_sendable_v<copy_on_write<DocumentWithCowMember>>);
static_assert(!is_sendable_v<copy_on_write<Cache>>);
static_assert(std::same_as<
    threadsafe::synchronized_value<copy_on_write<std::string>>::mutex,
    std::shared_mutex>);

// still movable-enough for the launcher, and no empty state
static_assert(std::move_constructible<copy_on_write<std::string>>);
static_assert(std::copy_constructible<copy_on_write<std::string>>);
static_assert(std::is_nothrow_move_constructible_v<copy_on_write<std::string>>);
static_assert(threadsafe::launchable_task<
                  decltype([](copy_on_write<std::string>) {}),
                  copy_on_write<std::string>>);

// the escaping T& is gone
static_assert(!can_detach<copy_on_write<int>>);
static_assert(can_modify<copy_on_write<int>, decltype([](int&) {})>);
static_assert(!can_modify<copy_on_write<std::unique_ptr<int>>,
                          decltype([](std::unique_ptr<int>&) {})>,
              "a non-copyable T still gives a read-only handle, not a hard error");

int main() {
    // 1. the moved-from segfault is gone
    std::vector<copy_on_write<std::string>> documents;
    documents.emplace_back(copy_on_write<std::string>{"alpha"});
    documents.emplace_back(copy_on_write<std::string>{"beta"});
    documents.emplace_back(copy_on_write<std::string>{"gamma"});
    auto new_end = std::remove_if(documents.begin(), documents.end(),
                                  [](const copy_on_write<std::string>& d) {
                                      return *d == "beta";
                                  });
    documents.back().modify([](std::string& s) { s.push_back('!'); });
    std::printf("after remove_if: kept=%ld tail=%s\n",
                new_end - documents.begin(), documents.back()->c_str());

    // 2. modify returns the mutation's result and detaches correctly
    copy_on_write<std::string> document{"shared-text"};
    copy_on_write<std::string> snapshot = document;
    const std::size_t new_size =
        document.modify([](std::string& body) { body += "-edited"; return body.size(); });
    std::printf("detached: doc=%s snapshot=%s size=%zu\n", document->c_str(),
                snapshot->c_str(), new_size);

    // 3. the detach copies through const: the shared object is untouched
    copy_on_write<StealOnCopy> stealing{StealOnCopy{4242}};
    copy_on_write<StealOnCopy> observer = stealing;
    stealing.modify([](StealOnCopy& value) { value.payload = 1; });
    std::printf("observer still sees payload=%d (want 4242)\n", observer->payload);
    return 0;
}
