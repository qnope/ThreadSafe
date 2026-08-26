#include <threadsafe/threadsafe.h>

#include <memory>
#include <print>
#include <span>
#include <vector>

int main() {
    threadsafe::asynchronous_task_launcher launcher;

    // 1. A big vector: move it in. Frictionless.
    std::vector<int> samples(1'000'000, 7);
    launcher.launch_task([](std::vector<int> owned) { std::println("moved {}", owned.size()); },
                         std::move(samples));

    // 2. A slice. std::span is rejected, so the slice must be materialised.
    std::vector<int> source(1'000, 1);
    launcher.launch_task([](std::vector<int> slice) { std::println("copied {}", slice.size()); },
                         std::vector<int>(source.begin(), source.begin() + 100));

    // 3. Shared read-only data. shared_ptr<const T> is rejected; copy_on_write
    //    is the library's own answer, and it is sendable.
    threadsafe::copy_on_write<std::vector<int>> table{std::vector<int>{1, 2, 3}};
    launcher.launch_task([](threadsafe::copy_on_write<std::vector<int>> shared) {
        std::println("shared {}", shared->size());
    }, table);

    // 4. A result channel: the user builds it out of synchronized_value + shared_ptr.
    auto results = threadsafe::synchronized_value<std::vector<int>>::make();
    launcher.launch_task([](std::shared_ptr<threadsafe::synchronized_value<std::vector<int>>> sink) {
        auto held = sink->lock();   // cannot be a one-liner: see value_guard
        held->push_back(42);
    }, results);
}
