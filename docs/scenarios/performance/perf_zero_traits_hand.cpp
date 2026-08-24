#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

struct plain_aggregate {
    int identifier;
    double weight;
    std::string label;
    std::vector<int> samples;
};

int consume(plain_aggregate value) {
    return value.identifier + int(value.samples.size());
}
