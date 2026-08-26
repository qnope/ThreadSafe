#include <threadsafe/threadsafe.h>

#include <print>
#include <string>
#include <type_traits>
#include <vector>

namespace acme {
class ReportBuffer {
public:
    explicit ReportBuffer(std::string report_name) : name_(std::move(report_name)) {}
    ~ReportBuffer() { std::println("closing {}", name_); }

    void append(int sample) { samples_.push_back(sample); }

private:
    std::string name_;
    std::vector<int> samples_;
};
}

// OPTION B: vouch for it. The message says "specialize is_sendable" but not how.
template <>
struct threadsafe::is_sendable<acme::ReportBuffer> : std::true_type {};

static_assert(threadsafe::is_sendable_v<acme::ReportBuffer>);
static_assert(threadsafe::is_lifetime_aware_v<acme::ReportBuffer>,
              "the destructor rule is a sendable rule only");

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](acme::ReportBuffer buffer) { buffer.append(1); },
                         acme::ReportBuffer{"nightly"});
}
