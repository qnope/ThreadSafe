// Read-heavy shared configuration, written the way every C++ codebase writes it:
// build the map once, freeze it, hand a shared_ptr<const Map> to each worker.
#include <threadsafe/threadsafe.h>

#include <map>
#include <memory>
#include <print>
#include <string>

using configuration = std::map<std::string, std::string>;

int main() {
    std::shared_ptr<const configuration> shared_configuration =
        std::make_shared<const configuration>(
            configuration{{"threads", "4"}, {"mode", "fast"}});

    threadsafe::asynchronous_task_launcher launcher;
    for (int worker_index = 0; worker_index < 4; ++worker_index)
        launcher.launch_task(
            [](std::shared_ptr<const configuration> configuration_to_read) {
                std::println("mode = {}", configuration_to_read->at("mode"));
            },
            shared_configuration);
}
