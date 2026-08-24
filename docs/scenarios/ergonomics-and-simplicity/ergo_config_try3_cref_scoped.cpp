// (b) attempt 3: the configuration never changes and the launcher waits, so
// borrow it with std::cref through launch_scoped_task.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <functional>
#include <map>
#include <string>

using Configuration = std::map<std::string, std::string>;

int main() {
    const Configuration configuration{{"host", "localhost"}, {"port", "8080"}};

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(
        [](const Configuration& shared_configuration) {
            std::printf("%s\n", shared_configuration.at("host").c_str());
        },
        std::cref(configuration));
}
