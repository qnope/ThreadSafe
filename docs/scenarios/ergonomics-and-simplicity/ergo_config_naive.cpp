// (b) Share a read-heavy configuration map -- the naive version: the map is
// const, several readers, no writer.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <map>
#include <string>

int main() {
    const std::map<std::string, std::string> configuration{
        {"host", "localhost"}, {"port", "8080"}};

    threadsafe::asynchronous_task_launcher launcher;
    for (int reader_index = 0; reader_index < 4; ++reader_index)
        launcher.launch_task([&configuration] {
            std::printf("%s\n", configuration.at("host").c_str());
        });
}
