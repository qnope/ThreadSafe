// The library's central trait is is_synchronizable. Its two neighbours have a
// concept; it does not.
#include <threadsafe/threadsafe.h>

template <threadsafe::sendable T>
void ship(T) {}

template <threadsafe::lifetime_aware T>
void keep(T) {}

template <threadsafe::synchronizable T>
void share(T &) {}

int main() {}
