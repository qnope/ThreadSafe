// Program 2, attempt A: publish by sharing ONE copy_on_write object between the
// writer and the readers. This is what a user reaches for first.
#include <threadsafe/threadsafe.h>
#include <functional>
#include <string>
#include <vector>

struct Config { std::string name; std::vector<int> weights; };

struct reader_task {
    void operator()(threadsafe::copy_on_write<Config>& shared_config) const {
        (void)shared_config->name;
    }
};

int main() {
    threadsafe::copy_on_write<Config> shared_config{Config{"v1", {1, 2, 3}}};
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(reader_task{}, std::ref(shared_config));
}
