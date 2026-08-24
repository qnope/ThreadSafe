// (2) A realistic third-party value type with a hand-written rule of five.
#include <threadsafe/threadsafe.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>

class SampleBuffer {
public:
    explicit SampleBuffer(std::size_t sample_count)
        : samples_(new double[sample_count]{}), sample_count_(sample_count) {}

    SampleBuffer(const SampleBuffer& other)
        : samples_(new double[other.sample_count_]),
          sample_count_(other.sample_count_) {
        std::copy_n(other.samples_, other.sample_count_, samples_);
    }
    SampleBuffer& operator=(const SampleBuffer& other) {
        SampleBuffer copy{other};
        std::swap(samples_, copy.samples_);
        std::swap(sample_count_, copy.sample_count_);
        return *this;
    }
    SampleBuffer(SampleBuffer&& other) noexcept
        : samples_(other.samples_), sample_count_(other.sample_count_) {
        other.samples_ = nullptr;
        other.sample_count_ = 0;
    }
    SampleBuffer& operator=(SampleBuffer&& other) noexcept {
        std::swap(samples_, other.samples_);
        std::swap(sample_count_, other.sample_count_);
        return *this;
    }
    ~SampleBuffer() { delete[] samples_; }

    double mean() const {
        double total = 0;
        for (std::size_t index = 0; index < sample_count_; ++index)
            total += samples_[index];
        return sample_count_ == 0 ? 0 : total / double(sample_count_);
    }

private:
    double* samples_;
    std::size_t sample_count_;
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(
        [](SampleBuffer buffer) { std::printf("%f\n", buffer.mean()); },
        SampleBuffer{16});
}
