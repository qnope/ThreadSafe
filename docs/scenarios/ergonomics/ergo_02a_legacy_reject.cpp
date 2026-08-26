#include <threadsafe/threadsafe.h>

#include <print>
#include <string>
#include <vector>

// A realistic legacy type: a logging destructor, a string, a vector.
class ReportBuffer {
public:
    explicit ReportBuffer(std::string report_name) : name_(std::move(report_name)) {}
    ~ReportBuffer() { std::println("closing {}", name_); }

    void append(int sample) { samples_.push_back(sample); }

private:
    std::string name_;
    std::vector<int> samples_;
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](ReportBuffer buffer) { buffer.append(1); },
                         ReportBuffer{"nightly"});
}
