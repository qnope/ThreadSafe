#include <threadsafe/threadsafe.h>

#include <functional>
#include <string>

int main() {
    std::string shared = "hello";
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task([](std::string &text) { text += '!'; },
                                std::ref(shared));
}
