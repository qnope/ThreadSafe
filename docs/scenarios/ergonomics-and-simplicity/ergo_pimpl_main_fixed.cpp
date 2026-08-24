// (2) The pimpl type, taught to the library. This is exactly what the user must
// add -- and it must sit outside every namespace, next to the header include.
#include <threadsafe/threadsafe.h>

#include "ergo_pimpl/session.h"

template <>
struct threadsafe::is_sendable<Session> : std::true_type {};
template <>
struct threadsafe::is_lifetime_aware<Session> : std::true_type {};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](Session session) { session.send("hello"); },
                         Session{"tcp://localhost:1234"});
}
