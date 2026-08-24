// A registry of forward-declared plugin objects -- a realistic header-only
// query that reaches unique_ptr<Incomplete>.
#include <threadsafe/threadsafe.h>

#include <memory>
#include <vector>

class Plugin;

using PluginRegistry = std::vector<std::unique_ptr<Plugin>>;

static_assert(!threadsafe::is_sendable_v<PluginRegistry>,
              "expected a plain false, not a compile error");
int main() {}
