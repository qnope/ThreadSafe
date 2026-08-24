// CONTROL: identical program, but the T& never outlives the expression that
// produced it. If the COW design is sound, this must be TSan-clean.
#include <threadsafe/threadsafe.h>
#include <cstddef>
#include <cstdio>
#include <string>

using threadsafe::copy_on_write;

int main() {
    copy_on_write<std::string> document{"shared-text"};
    document.as_mutable().push_back('!');

    copy_on_write<std::string> snapshot = document;

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
        document.as_mutable().push_back('x');
        document.as_mutable().pop_back();
    }
    std::printf("writer done, body size %zu\n", document->size());
    return 0;
}
