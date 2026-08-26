#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <string>
#include <vector>

using threadsafe::copy_on_write;

int main() {
    // ---- 1. the reference silently detaches from the handle ----
    copy_on_write<std::vector<int>> document(std::vector<int>{1, 2, 3});
    int& first_element = document.as_mutable()[0];

    copy_on_write<std::vector<int>> snapshot = document;

    document.as_mutable()[0] = 99;

    std::printf("document[0]        = %d   (expected 99)\n", (*document)[0]);
    std::printf("snapshot[0]        = %d   (expected 1)\n", (*snapshot)[0]);
    first_element = 4242;
    std::printf("after writing 4242 through the reference:\n");
    std::printf("document[0]        = %d\n", (*document)[0]);
    std::printf("snapshot[0]        = %d   <-- the SNAPSHOT was mutated\n",
                (*snapshot)[0]);

    // ---- 2. the reference dangles outright ----
    copy_on_write<std::string> text(std::string(64, 'a'));
    std::string& borrowed = text.as_mutable();
    text = copy_on_write<std::string>(std::string(64, 'b'));
    std::printf("borrowed.size()    = %zu   <-- read of a destroyed std::string\n",
                borrowed.size());
    return 0;
}
