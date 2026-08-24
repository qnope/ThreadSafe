// A moved-from copy_on_write holds a null shared_ptr.  use_count() is then 0,
// never 1, so as_mutable() takes the *copying* branch and dereferences null.
#include <threadsafe/threadsafe.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using threadsafe::copy_on_write;

int main() {
    std::vector<copy_on_write<std::string>> documents;
    documents.emplace_back(copy_on_write<std::string>{"alpha"});
    documents.emplace_back(copy_on_write<std::string>{"beta"});
    documents.emplace_back(copy_on_write<std::string>{"gamma"});

    // std::remove leaves moved-from elements in [new_end, end()) — the classic
    // erase-remove shape, here stopped one step short of erase().
    auto new_end = std::remove_if(documents.begin(), documents.end(),
                                  [](const copy_on_write<std::string>& document) {
                                      return *document == "beta";
                                  });
    std::printf("kept %ld of %zu\n", new_end - documents.begin(), documents.size());

    copy_on_write<std::string>& moved_from = documents.back();
    std::printf("about to call as_mutable() on a moved-from handle\n");
    std::fflush(stdout);
    moved_from.as_mutable().push_back('!');
    std::printf("survived: %s\n", moved_from->c_str());
    return 0;
}
