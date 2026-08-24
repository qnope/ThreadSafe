// synchronized_value, used the way the API invites.
#include <threadsafe/threadsafe.h>

#include <map>
#include <memory>
#include <print>
#include <string>

int main() {
    auto shared_settings =
        threadsafe::synchronized_value<std::map<std::string, int>>::make();

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int writer_index = 0; writer_index < 4; ++writer_index)
            launcher.launch_task(
                [](std::shared_ptr<threadsafe::synchronized_value<
                       std::map<std::string, int>>> settings,
                   int index) {
                    auto held = settings->lock();
                    (*held)["key" + std::to_string(index)] = index;
                },
                shared_settings, writer_index);
    }

    auto reading = shared_settings->lock_shared();
    std::println("{} entries", reading->size());
}
