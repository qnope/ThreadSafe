#include <threadsafe/threadsafe.h>

struct Cache { int value; mutable int hits; };

int main() { threadsafe::assert_synchronizable<const Cache>(); }
