// The SAME race, against the proposed modify() fix. decltype(auto) returns the
// mutation's own return type, so one identity lambda reproduces as_mutable().
#include <threadsafe/threadsafe.h>
#include <cstddef>
#include <cstdio>
#include <string>

using threadsafe::copy_on_write;

int main() {
    copy_on_write<std::string> document{"shared-text"};

    std::string& writable_body =
        document.modify([](std::string& body) -> std::string& { return body; });

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
