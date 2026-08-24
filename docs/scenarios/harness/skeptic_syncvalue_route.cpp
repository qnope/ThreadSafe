// Route through the LIBRARY's own synchronized_value: the user specializes
// is_sendable for their pimpl handle (documented, legitimate: "specialize
// is_sendable to state the intent"), and never touches
// THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE. get_mutex_type() then asks
// is_synchronizable_v<const T> ALONE and picks shared_mutex.
#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <memory>
#include <thread>
#include <typeinfo>

namespace {
struct Report { virtual ~Report() = default; virtual int total() const = 0; };
struct MemoizingReport final : Report {
    explicit MemoizingReport(int r) : rows(r) {}
    int rows;
    mutable int memo = -1;
    int total() const override { if (memo < 0) memo = rows * 2; return memo; }
};
struct ReportHandle { std::unique_ptr<const Report> owned; };
}

// The user vouches only for SENDABILITY (this handle really is sole owner and
// really can move between threads). Nothing here vouches for shared reads.
template <>
struct threadsafe::is_sendable<ReportHandle> : std::true_type {};
template <>
struct threadsafe::is_lifetime_aware<ReportHandle> : std::true_type {};

int main() {
    std::printf("is_synchronizable_v<const ReportHandle> = %d\n",
                (int)threadsafe::is_synchronizable_v<const ReportHandle>);
    std::printf("mutex chosen by synchronized_value = %s\n",
                typeid(threadsafe::synchronized_value<ReportHandle>::mutex).name());
    std::printf("is shared_mutex = %d\n",
                (int)std::is_same_v<threadsafe::synchronized_value<ReportHandle>::mutex,
                                    std::shared_mutex>);

    threadsafe::synchronized_value<ReportHandle> shared{
        ReportHandle{std::make_unique<const MemoizingReport>(21)}};

    auto hammer = [&shared] {
        long long acc = 0;
        for (int i = 0; i < 20000; ++i) {
            const auto reader_guard = shared.lock_shared();
            acc += reader_guard->owned->total();
        }
        return acc;
    };
    std::thread a{hammer}, b{hammer};
    a.join(); b.join();
    std::printf("done\n");
}
