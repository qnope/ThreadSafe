#include <threadsafe/threadsafe.h>

struct Vouched { int value; };
template <>
struct threadsafe::is_sendable<Vouched> : std::false_type {};

int main() { threadsafe::assert_sendable<Vouched>(); }
