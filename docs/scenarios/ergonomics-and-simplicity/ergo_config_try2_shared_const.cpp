// (b) attempt 2: the documented promise is "a const T may be read from several
// threads at once", so hold the immutable configuration in a
// shared_ptr<const Configuration> and pass it by value.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <map>
#include <memory>
#include <string>

using Configuration = std::map<std::string, std::string>;

static_assert(threadsafe::is_synchronizable_v<const Configuration>,
              "a const configuration map IS readable from several threads");

int main() {
    auto configuration = std::make_shared<const Configuration>(
        Configuration{{"host", "localhost"}, {"port", "8080"}});

    threadsafe::asynchronous_task_launcher launcher;
    for (int reader_index = 0; reader_index < 4; ++reader_index)
        launcher.launch_task(
            [](std::shared_ptr<const Configuration> shared_configuration) {
                std::printf("%s\n", shared_configuration->at("host").c_str());
            },
            configuration);
}
