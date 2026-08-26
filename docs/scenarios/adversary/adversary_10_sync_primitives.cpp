#include <threadsafe/threadsafe.h>
#include <atomic>
#include <barrier>
#include <condition_variable>
#include <latch>
#include <mutex>
#include <semaphore>
#include <shared_mutex>
#include <stop_token>

template <class T> constexpr bool sync = threadsafe::is_synchronizable_v<T>;
