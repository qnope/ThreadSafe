// A RAII wrapper over a C API: the single most common "awkward but legitimate"
// third-party type. It owns its handle, it moves, it closes on destruction.
// Sending it to another thread is perfectly safe.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <print>
#include <utility>

class text_file {
public:
    explicit text_file(const char *path) : stream_(std::fopen(path, "r")) {}
    text_file(text_file &&other) noexcept
        : stream_(std::exchange(other.stream_, nullptr)) {}
    text_file &operator=(text_file &&other) noexcept {
        std::swap(stream_, other.stream_);
        return *this;
    }
    text_file(const text_file &) = delete;
    text_file &operator=(const text_file &) = delete;
    ~text_file() {
        if (stream_ != nullptr)
            std::fclose(stream_);
    }

    bool is_open() const { return stream_ != nullptr; }

private:
    std::FILE *stream_;
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](text_file opened) { std::println("{}", opened.is_open()); },
                         text_file{"/etc/hosts"});
}
