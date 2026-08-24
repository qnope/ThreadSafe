// copy_on_write carries no static_assert: a T it can never share is accepted
// silently, and only the *use site* complains -- about copy_on_write, not T.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <string>

namespace {
struct BorrowedView {
    const std::string* text;
};
}

int main() {
    threadsafe::copy_on_write<BorrowedView> shared_view{BorrowedView{nullptr}};
    std::printf("constructed, is_sendable=%d\n",
                (int) threadsafe::is_sendable_v<decltype(shared_view)>);

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(
        [](threadsafe::copy_on_write<BorrowedView> view) { (void) view; },
        shared_view);
}
