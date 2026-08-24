#include <threadsafe/threadsafe.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
using threadsafe::copy_on_write;
int main() {
    std::vector<copy_on_write<std::string>> documents;
    documents.emplace_back(std::string("alpha"));
    documents.emplace_back(std::string("beta"));
    documents.emplace_back(std::string("gamma"));

    // std::remove_if MOVES the survivors forward; the tail holds moved-from
    // handles. Nothing here is unusual C++.
    auto tail = std::remove_if(documents.begin(), documents.end(),
                               [](const copy_on_write<std::string>& doc) {
                                   return *doc == "beta";
                               });
    std::printf("kept %zu of 3\n", static_cast<std::size_t>(tail - documents.begin()));
    std::printf("moved-from operator-> == %p\n",
                static_cast<const void*>(documents.back().operator->()));
    std::printf("about to call as_mutable() on a moved-from handle\n");
    std::fflush(stdout);
    documents.back().as_mutable() += "!";     // <-- boom
    std::printf("survived\n");
}
