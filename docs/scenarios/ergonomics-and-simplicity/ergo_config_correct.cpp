// (b) Share a read-heavy configuration map -- the version the library accepts.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <map>
#include <string>

using Configuration = std::map<std::string, std::string>;

int main() {
    const threadsafe::copy_on_write<Configuration> configuration{
        Configuration{{"host", "localhost"}, {"port", "8080"}}};

    threadsafe::asynchronous_task_launcher launcher;
    for (int reader_index = 0; reader_index < 4; ++reader_index)
        launcher.launch_task(
            [](threadsafe::copy_on_write<Configuration> shared_configuration) {
                std::printf("%s:%s\n", shared_configuration->at("host").c_str(),
                            shared_configuration->at("port").c_str());
            },
            configuration);
}
