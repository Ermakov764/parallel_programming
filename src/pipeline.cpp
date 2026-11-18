#include "pipeline.hpp"
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

Pipeline::Pipeline(std::vector<Transformation> stages) : stages_(std::move(stages)) {}

void Pipeline::run(std::vector<Image>& images, ExecutionMode mode, int threads) const {
    if (threads < 1) throw std::invalid_argument("threads must be positive");
    if (images.empty() || stages_.empty()) return;
    if (mode == ExecutionMode::Image) {
        for (auto& image : images)
            for (const auto& stage : stages_) image = stage.apply(image, threads);
        return;
    }
    if (mode == ExecutionMode::Stage) {
        for (const auto& stage : stages_)
            for (auto& image : images) image = stage.apply(image, threads);
        return;
    }
    // Only one task per image can be active. Different images may occupy
    // different stages concurrently, with a fixed total worker budget.
    std::deque<std::pair<size_t, size_t>> ready;
    for (size_t i = 0; i < images.size(); ++i) ready.emplace_back(i, 0);
    std::mutex mutex;
    std::condition_variable changed;
    size_t remaining = images.size();
    bool stopped = false;
    std::exception_ptr failure;
    auto work = [&]() {
        try {
            for (;;) {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return stopped || !ready.empty() || remaining == 0; });
                if (stopped || remaining == 0) return;
                const auto task = ready.front();
                ready.pop_front();
                lock.unlock();
                images[task.first] = stages_[task.second].apply(images[task.first], 1);
                lock.lock();
                if (task.second + 1 < stages_.size())
                    ready.emplace_front(task.first, task.second + 1);
                else --remaining;
                changed.notify_all();
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);
            if (!failure) failure = std::current_exception();
            stopped = true;
            changed.notify_all();
        }
    };
    std::vector<std::thread> workers;
    try {
        for (size_t i = 0; i < std::min(images.size(), static_cast<size_t>(threads)); ++i)
            workers.emplace_back(work);
    } catch (...) {
        { std::lock_guard<std::mutex> lock(mutex); stopped = true; }
        changed.notify_all();
        for (auto& worker : workers) worker.join();
        throw;
    }
    for (auto& worker : workers) worker.join();
    if (failure) std::rethrow_exception(failure);
}
