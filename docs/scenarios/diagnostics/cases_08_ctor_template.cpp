#include <threadsafe/threadsafe.h>

struct HasConstructorTemplate {
    int value;
    template <class U>
    explicit HasConstructorTemplate(U&& source) : value(static_cast<int>(source)) {}
};

int main() {
    threadsafe::assert_sendable<HasConstructorTemplate>();
}
