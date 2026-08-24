#include <threadsafe/threadsafe.h>
#include <memory>

namespace {
struct NoopDeleter { void operator()(const int *) const noexcept {} };
struct Report { virtual ~Report() = default; virtual int total() const = 0; };
struct FinalReport final : Report { int total() const override { return 0; } };
struct BorrowedReading { std::unique_ptr<const int, NoopDeleter> observed; };
struct BadDeleter { BadDeleter(const BadDeleter &); void operator()(int *) const; };
}

using threadsafe::is_synchronizable_v;

static_assert(is_synchronizable_v<const std::unique_ptr<const int>>,
              "default_delete: sole ownership, the pointee's const is trusted");
static_assert(!is_synchronizable_v<const std::unique_ptr<int>>);
static_assert(!is_synchronizable_v<const std::unique_ptr<const int, BadDeleter>>);
static_assert(!is_synchronizable_v<const std::unique_ptr<const int, NoopDeleter>>,
              "FIXED: a deleter that does not delete does not own");
static_assert(!is_synchronizable_v<const std::unique_ptr<const int, void (*)(const int *)>>,
              "FIXED: nor does a function-pointer deleter, which may be a no-op");
static_assert(!is_synchronizable_v<const BorrowedReading>,
              "FIXED: and the walk propagates the refusal");
static_assert(!is_synchronizable_v<const std::unique_ptr<const Report>>,
              "FIXED: the unknown dynamic type is guarded like is_sendable's");
static_assert(is_synchronizable_v<const std::unique_ptr<const FinalReport>>,
              "a final pointee is still fine");
static_assert(is_synchronizable_v<const std::unique_ptr<const int[]>>,
              "the array form keeps working");
