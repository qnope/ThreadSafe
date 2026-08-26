#include <threadsafe/threadsafe.h>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <latch>
#include <semaphore>
using threadsafe::is_synchronizable_v;
static_assert(!is_synchronizable_v<std::mutex>, "mutex TRUE");
static_assert(!is_synchronizable_v<std::shared_mutex>, "shared_mutex TRUE");
static_assert(!is_synchronizable_v<std::once_flag>, "once_flag TRUE");
static_assert(!is_synchronizable_v<std::atomic_flag>, "atomic_flag TRUE");
static_assert(!is_synchronizable_v<std::condition_variable>, "cv TRUE");
static_assert(!is_synchronizable_v<std::latch>, "latch TRUE");
static_assert(!is_synchronizable_v<std::counting_semaphore<4>>, "sem TRUE");
static_assert(!is_synchronizable_v<const std::mutex>, "const mutex TRUE");
