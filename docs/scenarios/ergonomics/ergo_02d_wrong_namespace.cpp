#include <threadsafe/threadsafe.h>
#include <string>
#include <type_traits>

namespace acme {
struct ReportBuffer {
    std::string name;
    ~ReportBuffer() {}
};

// The natural place a user puts it: next to their type, inside their namespace.
template <>
struct threadsafe::is_sendable<ReportBuffer> : std::true_type {};
}
