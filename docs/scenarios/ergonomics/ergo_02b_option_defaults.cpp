#include <threadsafe/threadsafe.h>

#include <print>
#include <string>
#include <vector>

// OPTION A: write out every special member as defaulted, keeping the logging
// destructor. Does spelling the other five out rescue the type?
class ReportBuffer {
public:
    explicit ReportBuffer(std::string report_name) : name_(std::move(report_name)) {}

    ReportBuffer(const ReportBuffer&) = default;
    ReportBuffer(ReportBuffer&&) = default;
    ReportBuffer& operator=(const ReportBuffer&) = default;
    ReportBuffer& operator=(ReportBuffer&&) = default;
    ~ReportBuffer() { std::println("closing {}", name_); }

private:
    std::string name_;
    std::vector<int> samples_;
};

static_assert(threadsafe::is_sendable_v<ReportBuffer>, "OPTION A works");
