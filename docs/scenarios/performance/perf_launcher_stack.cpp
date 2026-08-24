#include <threadsafe/threadsafe.h>
#include <array>
#include <thread>
#include <vector>

struct big_payload {
    std::array<char, 262144> storage;
};

void consume(big_payload payload);

void via_launcher(threadsafe::asynchronous_task_launcher& launcher,
                  const big_payload& argument) {
    launcher.launch_task(consume, argument);
}

void via_emplace(std::vector<std::jthread>& threads,
                 const big_payload& argument) {
    threads.emplace_back(consume, argument);
}
