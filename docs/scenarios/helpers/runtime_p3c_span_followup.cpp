// Follow the diagnostic's advice for std::span and see where it lands.
#include <threadsafe/threadsafe.h>
#include <span>
#include <vector>

template <>
struct threadsafe::is_sendable<std::span<const double>> : std::true_type {};

struct slice_task {
    void operator()(std::span<const double> slice) const { (void)slice; }
};
int main() {
    std::vector<double> data(1024);
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(slice_task{}, std::span<const double>{data});
}
