#include <threadsafe/threadsafe.h>
namespace { int slab[64];
struct SlabHandle { int index; void bump() const { ++slab[index]; } }; }
static_assert(threadsafe::is_sendable_v<threadsafe::copy_on_write<SlabHandle>>,
              "copy_on_write<SlabHandle> is sendable too: cow_is_sendable() is "
              "is_sendable_v<T> && is_synchronizable_v<const T>");
static_assert(threadsafe::launchable_task<
                  decltype([](threadsafe::copy_on_write<SlabHandle> handle) {
                      handle->bump();
                  }),
                  threadsafe::copy_on_write<SlabHandle>>,
              "so launch_task hands copies of it to as many threads as you like");
