// The claim's "second, nastier shape", entirely single-threaded: after the
// detach, the escaped reference writes into the *snapshot's* object.
#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <string>

using threadsafe::copy_on_write;

int main() {
    copy_on_write<std::string> document{"original"};
    std::string& escaped = document.as_mutable();

    copy_on_write<std::string> snapshot = document;
    document.as_mutable();  // use_count()==2 -> detach: document moves elsewhere

    escaped += "-MUTATED";

    std::printf("document : %s\n", document->c_str());
    std::printf("snapshot : %s\n", snapshot->c_str());
    std::printf("escaped aliases snapshot: %d\n", &escaped == snapshot.operator->());
    std::printf("escaped aliases document: %d\n", &escaped == document.operator->());
    return 0;
}
