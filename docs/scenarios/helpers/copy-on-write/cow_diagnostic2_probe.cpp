#include <threadsafe/threadsafe.h>
#include <string>

using threadsafe::copy_on_write;

namespace {
struct DocumentWithCowMember {
    copy_on_write<std::string> body;
};

consteval bool why_document_is_not_cowable() {
    threadsafe::assert_sendable<copy_on_write<DocumentWithCowMember>>();
    return true;
}
}

static_assert(why_document_is_not_cowable());

int main() {}
