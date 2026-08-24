// (2) The pimpl type as a *user* sees it: only the header, Implementation
// incomplete. No opt-in written.
#include <threadsafe/threadsafe.h>

#include "ergo_pimpl/session.h"

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](Session session) { session.send("hello"); },
                         Session{"tcp://localhost:1234"});
}
