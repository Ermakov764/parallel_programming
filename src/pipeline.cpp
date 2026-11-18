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
    // Для изображения доступен только один этап: следующий появляется
    // в очереди после завершения предыдущего. Разные изображения независимы.
    struct Task { size_t image; size_t stage; };
    std::deque<Task> ready;
    for (size_t i = 0; i < images.size(); ++i) ready.push_back({i, 0});
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
                // Расчёт выполняется без блокировки общей очереди.
                images[task.image] = stages_[task.stage].apply(images[task.image], 1);
                lock.lock();
                if (task.stage + 1 < stages_.size())
                    ready.push_front({task.image, task.stage + 1});
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
