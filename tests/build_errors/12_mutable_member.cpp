#include <threadsafe/threadsafe.h>

struct CachesOnRead {
    int value = 0;
    mutable int read_count = 0;
};

int main() {
    threadsafe::copy_on_write<CachesOnRead> shared{};
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](threadsafe::copy_on_write<CachesOnRead>) {}, shared);
}
