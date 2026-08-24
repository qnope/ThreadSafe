// Read-heavy shared configuration, the way the library accepts it.
#include <threadsafe/threadsafe.h>

#include <map>
#include <print>
#include <string>

using configuration = std::map<std::string, std::string>;

int main() {
    threadsafe::copy_on_write<configuration> settings{
        configuration{{"host", "localhost"}, {"port", "8080"}}};

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int reader_index = 0; reader_index < 4; ++reader_index)
            launcher.launch_task(
                [](threadsafe::copy_on_write<configuration> read_only_settings) {
                    std::println("host={} port={}", read_only_settings->at("host"),
                                 read_only_settings->at("port"));
                },
                settings);
    }
}
