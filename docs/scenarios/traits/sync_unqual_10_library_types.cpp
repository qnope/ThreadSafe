#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
#include <string>
#include <mutex>

using threadsafe::asynchronous_task_launcher;
using threadsafe::copy_on_write;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::synchronized_value;

#define PROBE(EXPR, NAME) constexpr bool NAME = (EXPR)

struct HoldsCow { copy_on_write<std::string> text; };

PROBE(is_synchronizable_v<asynchronous_task_launcher>, sync_launcher);
PROBE(is_synchronizable_v<const asynchronous_task_launcher>, sync_const_launcher);
PROBE(is_synchronizable_v<synchronized_value<int>>, sync_sv_int);
PROBE(is_synchronizable_v<const synchronized_value<int>>, sync_const_sv_int);
PROBE(is_synchronizable_v<synchronized_value<std::string>>, sync_sv_string);
PROBE(is_synchronizable_v<copy_on_write<std::string>>, sync_cow);
PROBE(is_synchronizable_v<const copy_on_write<std::string>>, sync_const_cow);
PROBE(is_sendable_v<copy_on_write<std::string>>, send_cow);
PROBE(is_synchronizable_v<const HoldsCow>, sync_const_holds_cow);
PROBE(is_sendable_v<HoldsCow>, send_holds_cow);
using Guard = threadsafe::value_guard<int, std::unique_lock<std::mutex>>;
PROBE(is_synchronizable_v<Guard>, sync_guard);
PROBE(is_synchronizable_v<const Guard>, sync_const_guard);

int main() {
#define SHOW(NAME) std::printf("%-24s = %s\n", #NAME, NAME ? "true" : "false")
    SHOW(sync_launcher); SHOW(sync_const_launcher);
    SHOW(sync_sv_int); SHOW(sync_const_sv_int); SHOW(sync_sv_string);
    SHOW(sync_cow); SHOW(sync_const_cow); SHOW(send_cow);
    SHOW(sync_const_holds_cow); SHOW(send_holds_cow);
    SHOW(sync_guard); SHOW(sync_const_guard);
}
