#include <threadsafe/threadsafe.h>
#include <memory>
#include <vector>
struct slice_task {
    void operator()(std::shared_ptr<const std::vector<double>> data,
                    std::size_t first, std::size_t last) const {
        (void)data; (void)first; (void)last;
    }
};
int main() {
    auto data = std::make_shared<const std::vector<double>>(1024);
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(slice_task{}, data, std::size_t{0}, std::size_t{1024});
}
