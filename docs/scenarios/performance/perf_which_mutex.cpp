// How often does the automatic selection land on shared_mutex for ordinary types?
#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <map>
#include <shared_mutex>
#include <string>
#include <vector>

struct plain_aggregate { int a; double b; };

template <class T>
void report(const char* name) {
    std::printf("  %-34s -> %s\n", name,
        std::is_same_v<typename threadsafe::synchronized_value<T>::mutex, std::shared_mutex>
            ? "shared_mutex" : "mutex");
}

int main() {
    std::printf("automatic mutex selection:\n");
    report<int>("int");
    report<double>("double");
    report<plain_aggregate>("plain_aggregate");
    report<std::vector<int>>("std::vector<int>");
    report<std::string>("std::string");
    report<std::map<int, int>>("std::map<int,int>");
}
