// Read-heavy shared configuration, the obvious way: one immutable map, several
// reader threads, shared by const reference for the duration of the call.
#include <threadsafe/threadsafe.h>

#include <functional>
#include <map>
#include <print>
#include <string>

using configuration = std::map<std::string, std::string>;

int main() {
    const configuration settings{{"host", "localhost"}, {"port", "8080"}};

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(
        [](const configuration &readable) {
            std::println("host = {}", readable.at("host"));
        },
        std::cref(settings));
}
