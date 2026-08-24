#include <threadsafe/threadsafe.h>

#include <memory>
#include <string>
#include <vector>

namespace {
struct Report { virtual ~Report() = default; virtual int total() const = 0; };
struct FinalReport final : Report { int total() const override { return 0; } };
struct Body { std::string text; };
struct Document { threadsafe::copy_on_write<Body> body; int revision; };
}

using threadsafe::copy_on_write;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// fix 1
static_assert(!is_synchronizable_v<const std::unique_ptr<const Report>>,
              "the polymorphic pointee is now guarded like is_sendable's");
static_assert(is_synchronizable_v<const std::unique_ptr<const FinalReport>>,
              "a final pointee still passes");
static_assert(is_synchronizable_v<const std::unique_ptr<const int>>,
              "the existing accepted cases are untouched");
static_assert(!is_synchronizable_v<const std::unique_ptr<int>>);

// fix 2
static_assert(is_synchronizable_v<const copy_on_write<Body>>);
static_assert(is_sendable_v<copy_on_write<copy_on_write<Body>>>,
              "copy_on_write nests now");
static_assert(is_sendable_v<copy_on_write<Document>>);
static_assert(is_synchronizable_v<const std::vector<copy_on_write<Body>>>);
static_assert(!is_synchronizable_v<const copy_on_write<int *>>,
              "and it still propagates: a borrowing payload is refused");
