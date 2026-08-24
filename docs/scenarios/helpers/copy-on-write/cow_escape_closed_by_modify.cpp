// The same program as cow_escaping_reference_race.cpp, written against the
// scoped modify() form. The natural spelling no longer has anywhere to keep the
// T& past the detach, so the race is unreachable.
#include <threadsafe/threadsafe.h>

#include <cstddef>
#include <cstdio>
#include <string>

using threadsafe::copy_on_write;

int main() {
    copy_on_write<std::string> document{"shared-text"};
    document.modify([](std::string& body) { body += "-v1"; });

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

    for (int iteration = 0; iteration != 400000; ++iteration)
        document.modify([](std::string& body) {
            body.push_back('x');
            body.pop_back();
        });

    std::printf("writer done, body=%s snapshot=%s aliased=%d\n",
                document->c_str(), snapshot->c_str(),
                document.operator->() == snapshot.operator->());
    return 0;
}
