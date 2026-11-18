#pragma once
#include "pipeline.hpp"
#include <cstddef>
#include <filesystem>

struct BatchResult {
    std::size_t images;
    double read_ms;
    double process_ms;
    double write_ms;
    double total_ms;
};

BatchResult run_batch(const std::filesystem::path& input, const std::filesystem::path& output,
                      ExecutionMode mode, int threads);
