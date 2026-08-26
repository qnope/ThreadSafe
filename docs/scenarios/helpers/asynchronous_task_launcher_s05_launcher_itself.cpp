#include <threadsafe/threadsafe.h>

#include <cstdint>
#include <functional>
#include <thread>
#include <type_traits>

using threadsafe::asynchronous_task_launcher;

// value semantics
static_assert(std::is_copy_constructible_v<asynchronous_task_launcher>,
              "POLARITY: the trait says copy-constructible -- vector<jthread> declares a copy ctor");
static_assert(std::is_move_constructible_v<asynchronous_task_launcher>,
              "POLARITY: the launcher IS movable");
static_assert(std::is_move_assignable_v<asynchronous_task_launcher>);
static_assert(std::is_default_constructible_v<asynchronous_task_launcher>);
static_assert(!std::is_nothrow_move_constructible_v<asynchronous_task_launcher>
              || std::is_nothrow_move_constructible_v<asynchronous_task_launcher>);

// what the traits say
static_assert(!threadsafe::is_synchronizable_v<asynchronous_task_launcher>);
static_assert(!threadsafe::is_synchronizable_v<const asynchronous_task_launcher>,
              "POLARITY: not even read-shareable");
static_assert(!threadsafe::is_sendable_v<asynchronous_task_launcher>,
              "POLARITY: a launcher cannot be sent to another thread");
static_assert(!threadsafe::is_lifetime_aware_v<asynchronous_task_launcher>,
              "POLARITY: launcher lifetime-aware?");

// std::jthread is what blocks it
static_assert(!threadsafe::is_sendable_v<std::jthread>);
static_assert(!threadsafe::is_sendable_v<std::vector<std::jthread>>);

// so it can be neither moved into a task nor shared with one
static_assert(!threadsafe::launchable_task<void(*)(asynchronous_task_launcher),
                                           asynchronous_task_launcher>);
static_assert(!threadsafe::launchable_scoped_task<
                  void(*)(asynchronous_task_launcher&),
                  std::reference_wrapper<asynchronous_task_launcher>>);

// ... but a pointer laundered through an integer is sendable and lifetime aware
namespace {
struct SmuggledPointer {
    std::uintptr_t address;
    void operator()() const { *reinterpret_cast<int *>(address) = 1; }
};
}
static_assert(threadsafe::launchable_task<SmuggledPointer>,
              "POLARITY: a pointer cast to uintptr_t crosses freely");

int main() {}
