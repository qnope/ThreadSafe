// Read-heavy shared configuration, written the canonical modern-C++ way:
// an immutable map behind a shared_ptr<const T>.
#include <threadsafe/threadsafe.h>

#include <map>
#include <memory>
#include <print>
#include <string>

using configuration = std::map<std::string, std::string>;

int main() {
    std::shared_ptr<const configuration> settings =
        std::make_shared<const configuration>(
            configuration{{"host", "localhost"}, {"port", "8080"}});

    threadsafe::asynchronous_task_launcher launcher;
    for (int reader_index = 0; reader_index < 4; ++reader_index)
        launcher.launch_task(
            [](std::shared_ptr<const configuration> read_only_settings) {
                std::println("{}", read_only_settings->at("host"));
            },
            settings);
}
