// (2) The user follows the diagnostic literally: "specialize is_sendable".
#include <threadsafe/threadsafe.h>

#include "ergo_pimpl/session.h"

template <>
struct threadsafe::is_sendable<Session> : std::true_type {};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](Session session) { session.send("hello"); },
                         Session{"tcp://localhost:1234"});
}
