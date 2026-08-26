#include <threadsafe/threadsafe.h>
#include <span>
#include <vector>
struct slice_task {
    void operator()(std::span<const double> slice) const { (void)slice; }
};
int main() {
    std::vector<double> data(1024);
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(slice_task{}, std::span<const double>{data});
}
