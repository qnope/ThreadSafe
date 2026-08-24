#include <threadsafe/threadsafe.h>
namespace {
struct NonSendable { NonSendable(const NonSendable&); };
template <class T>
concept wrappable = requires { typename threadsafe::synchronized_value<T>; };
}
static_assert(wrappable<int>);
static_assert(!wrappable<NonSendable>,
              "synchronized_value must refuse a non-sendable T detectably");
