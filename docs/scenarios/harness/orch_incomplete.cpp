#include <threadsafe/threadsafe.h>
#include <memory>
// The library's own diagnostic tells users to do exactly this (the pimpl idiom).
namespace { class Implementation; }
struct Widget { std::unique_ptr<Implementation> impl; };
static_assert(!threadsafe::is_sendable_v<Widget>);   // we only ASK the question
