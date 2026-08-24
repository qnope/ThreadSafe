#include <threadsafe/threadsafe.h>

template <class T>
struct my_locked_box {
    T value;
};

template <class T>
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(my_locked_box<T>);

int main() {}
