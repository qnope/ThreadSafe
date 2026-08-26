#include <threadsafe/threadsafe.h>
#include <chrono>
int main() { threadsafe::assert_sendable<std::chrono::milliseconds>(); }
