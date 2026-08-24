// is_sendable<unique_ptr<T,D>> guards the unknown dynamic type with
// detail::dynamic_type_is_known.  is_synchronizable<const unique_ptr<T,D>>,
// written 25 lines below it in the same header, does not -- so the const
// question answers "read-safe" for a handle whose pointee is a derived object
// carrying a mutable cache.

#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <memory>
#include <shared_mutex>
#include <thread>

namespace {

struct Report {
    virtual ~Report() = default;
    virtual int total() const = 0;
};

struct MemoizingReport final : Report {
    explicit MemoizingReport(int row_count) : rows(row_count) {}

    int rows;
    mutable int memo = -1;
    int total() const override {
        if (memo < 0)
            memo = rows * 2;
        return memo;
    }
};

}

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(!is_synchronizable_v<const MemoizingReport>,
              "the walk sees the mutable memo and says no");
static_assert(!is_sendable_v<std::unique_ptr<const Report>>,
              "is_sendable DOES guard the unknown dynamic type");
static_assert(is_synchronizable_v<const std::unique_ptr<const Report>>,
              "but is_synchronizable<const unique_ptr<...>> does NOT: it says "
              "this handle is readable from several threads at once");

// Exactly the shape synchronized_value derives from the trait:
// is_synchronizable_v<const T> -> shared_mutex + shared_lock for const access.
template <class T>
class read_shared_box {
    static_assert(is_synchronizable_v<const T>,
                  "gate is the library's own const question");

public:
    explicit read_shared_box(T value) : value_(std::move(value)) {}

    template <class Reader>
    decltype(auto) read(Reader reader) const {
        const std::shared_lock<std::shared_mutex> reader_lock{mutex_};
        return reader(value_);
    }

private:
    mutable std::shared_mutex mutex_;
    T value_;
};

int main() {
    std::unique_ptr<const Report> owned =
        std::make_unique<const MemoizingReport>(21);
    read_shared_box<std::unique_ptr<const Report>> box{std::move(owned)};

    auto hammer = [&box] {
        int accumulated = 0;
        for (int iteration = 0; iteration < 20000; ++iteration)
            accumulated += box.read(
                [](const std::unique_ptr<const Report> &handle) {
                    return handle->total();
                });
        return accumulated;
    };

    std::thread first_reader{hammer};
    std::thread second_reader{hammer};
    first_reader.join();
    second_reader.join();

    std::printf("done\n");
    return 0;
}
