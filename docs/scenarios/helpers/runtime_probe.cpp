#include <threadsafe/threadsafe.h>
#include <atomic>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <deque>
#include <map>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

struct Config { std::string name; std::vector<int> weights; };
struct Job { int id; std::string payload; };

// shared_ptr<const T> -- the canonical immutable-share idiom
static_assert(!is_sendable_v<std::shared_ptr<const std::vector<double>>>);
static_assert(!is_sendable_v<std::shared_ptr<const Config>>);
static_assert(is_synchronizable_v<const std::vector<double>>);
static_assert(!is_synchronizable_v<std::vector<double>>);

// atomic<shared_ptr<T>>
static_assert(!is_synchronizable_v<std::atomic<std::shared_ptr<Config>>>);
static_assert(!is_sendable_v<std::atomic<std::shared_ptr<Config>>>);

// span
static_assert(is_sendable_v<std::span<double>> == false);
static_assert(!is_lifetime_aware_v<std::span<const double>>);

// deque<Job> const-synchronizable -> shared_mutex chosen for a pure-write queue
static_assert(is_synchronizable_v<const std::deque<Job>>);
static_assert(std::is_same_v<threadsafe::synchronized_value<std::deque<Job>>::mutex, std::shared_mutex>);
static_assert(std::is_same_v<threadsafe::synchronized_value<std::map<int,int>>::mutex, std::shared_mutex>);

// copy_on_write publication
static_assert(!is_synchronizable_v<const threadsafe::copy_on_write<Config>>);
static_assert(!is_synchronizable_v<threadsafe::copy_on_write<Config>>);
static_assert(is_sendable_v<threadsafe::copy_on_write<Config>>);
static_assert(std::is_same_v<threadsafe::synchronized_value<threadsafe::copy_on_write<Config>>::mutex, std::mutex>);
