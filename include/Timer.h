#ifndef TIMER_H
#define TIMER_H

class Timer {
  public:
    Timer() : accumulated_(0), running_(false) {}

    void start() {
        accumulated_ = std::chrono::microseconds(0);
        start_ = std::chrono::high_resolution_clock::now();
        running_ = true;
    }

    void stop() {
        if (running_) {
            auto now = std::chrono::high_resolution_clock::now();
            accumulated_ += std::chrono::duration_cast<std::chrono::microseconds>(now - start_);
            running_ = false;
        }
    }

    void resume() {
        if (!running_) {
            start_ = std::chrono::high_resolution_clock::now();
            running_ = true;
        }
    }

    long long elapsedMicroseconds() const {
        if (running_) {
            auto now = std::chrono::high_resolution_clock::now();
            return accumulated_.count() +
                   std::chrono::duration_cast<std::chrono::microseconds>(now - start_).count();
        }
        return accumulated_.count();
    }

  private:
    std::chrono::high_resolution_clock::time_point start_{};
    std::chrono::microseconds accumulated_{};
    bool running_ = false;
};

#endif  // TIMER_H
