// Was: is_sendable<unique_ptr<T,D>> guarded the unknown dynamic type with
// detail::dynamic_type_is_known, while is_synchronizable<const unique_ptr<T,D>>,
// written 25 lines below it in the same header, did not -- so the const question
// answered "read-safe" for a handle whose pointee is a derived object carrying a
// mutable cache, and read_shared_box below handed it a shared_lock.
//
// Fixed: the guard is now applied at every site that answers *structurally* about
// a pointee -- the const question and is_lifetime_aware as well as is_sendable.
// This file is the compile-only proof; it no longer races because it no longer
// compiles a racing box.

#include <threadsafe/threadsafe.h>

#include <memory>
#include <shared_mutex>

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

struct FrozenReport final : Report {
    explicit FrozenReport(int row_count) : rows(row_count) {}

    int rows;
    int total() const override { return rows * 2; }
};

}

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(!is_synchronizable_v<const MemoizingReport>,
              "the walk sees the mutable memo and says no");
static_assert(!is_sendable_v<std::unique_ptr<const Report>>,
              "is_sendable guards the unknown dynamic type");
static_assert(!is_synchronizable_v<const std::unique_ptr<const Report>>,
              "and so does the const question now: the pointee may be the "
              "MemoizingReport, whose mutable memo the walk never sees through "
              "the base");
static_assert(is_synchronizable_v<const std::unique_ptr<const FrozenReport>>,
              "a final pointee has a known dynamic type, so the const of owned "
              "storage is trusted as before");
static_assert(is_synchronizable_v<const std::unique_ptr<const int>>,
              "non-polymorphic pointees are unaffected");

// Exactly the shape synchronized_value derives from the trait:
// is_synchronizable_v<const T> -> shared_mutex + shared_lock for const access.
// Instantiating it on unique_ptr<const Report> is now a compile error, which is
// the whole point.
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

template class read_shared_box<std::unique_ptr<const FrozenReport>>;
