// Every ThreadSafe check passes, yet two threads touch the same std::string at
// the same time: as_mutable() hands out a T& whose justification (the block was
// unique) expires the moment the handle is copied.
#include <threadsafe/threadsafe.h>

#include <cstddef>
#include <cstdio>
#include <string>

using threadsafe::copy_on_write;

static_assert(threadsafe::is_sendable_v<copy_on_write<std::string>>,
              "the library says a copy_on_write<std::string> may cross threads");
static_assert(threadsafe::launchable_task<
                  decltype([](copy_on_write<std::string>) {}),
                  copy_on_write<std::string>>,
              "and the launcher accepts it as an argument");

int main() {
    copy_on_write<std::string> document{"shared-text"};

    // Legitimate: the block is genuinely unique here, so as_mutable() writes in
    // place and hands back a reference into that block.
    std::string& writable_body = document.as_mutable();

    // Also legitimate: copying a copy_on_write is the documented way to share it.
    copy_on_write<std::string> snapshot = document;

    std::printf("aliased: %d\n", &writable_body == snapshot.operator->());

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(
        [](copy_on_write<std::string> received) {
            std::size_t accumulated_size = 0;
            for (int iteration = 0; iteration != 400000; ++iteration)
                accumulated_size += received->size();
            std::printf("reader saw %zu total\n", accumulated_size);
        },
        snapshot);

    for (int iteration = 0; iteration != 400000; ++iteration) {
        writable_body.push_back('x');
        writable_body.pop_back();
    }

    std::printf("writer done, body size %zu\n", writable_body.size());
    return 0;
}
