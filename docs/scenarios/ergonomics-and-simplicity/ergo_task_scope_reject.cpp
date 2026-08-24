#include <threadsafe/threadsafe.h>
#include <functional>
#include <string>

int main() {
    std::string shared_text;
    threadsafe::task_scope scope;
    scope.launch([](std::string &text) { text.push_back('x'); },
                 std::ref(shared_text));
}
