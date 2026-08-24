// Integrating a third-party C library: an opaque, documented-thread-safe
// handle behind a RAII wrapper. What must a user write to teach ThreadSafe
// about it?
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <print>

// ---- the C API (opaque handle, documented "all calls are thread-safe") ----
extern "C" {
struct c_logger;
c_logger *c_logger_open();
void c_logger_write(c_logger *logger, int value);
void c_logger_close(c_logger *logger);
}

struct c_logger {
    std::atomic<int> written{0};
};

c_logger *c_logger_open() { return new c_logger; }
void c_logger_write(c_logger *logger, int) {
    logger->written.fetch_add(1, std::memory_order_relaxed);
}
void c_logger_close(c_logger *logger) { delete logger; }

// ---- the user's RAII wrapper ----
namespace app {

class logger {
public:
    logger() : handle_(c_logger_open()) {}
    logger(const logger &) = delete;
    logger &operator=(const logger &) = delete;
    ~logger() { c_logger_close(handle_); }

    void write(int value) const { c_logger_write(handle_, value); }
    int written() const { return handle_->written.load(); }

private:
    c_logger *handle_;
};

}


// ---- the whole escape hatch: one line ----
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(app::logger);

static_assert(threadsafe::is_sendable_v<std::shared_ptr<app::logger>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<app::logger>>);

int main() {
    auto shared_logger = std::make_shared<app::logger>();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int thread_index = 0; thread_index < 4; ++thread_index)
            launcher.launch_task(
                [](std::shared_ptr<app::logger> logger) {
                    for (int step = 0; step < 1000; ++step)
                        logger->write(step);
                },
                shared_logger);
    }
    std::println("written = {}", shared_logger->written());
}
