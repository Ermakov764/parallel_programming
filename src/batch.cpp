#include "batch.hpp"
#include "bmp.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

BatchResult run_batch(const fs::path& input, const fs::path& output, ExecutionMode mode, int threads) {
    using Clock = std::chrono::steady_clock;
    if (fs::weakly_canonical(input) == fs::weakly_canonical(output))
        throw std::invalid_argument("input and output directories must differ");
    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(input)) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (entry.is_regular_file() && ext == ".bmp") paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) throw std::invalid_argument("no BMP files in input directory");
    fs::create_directories(output);
    for (const auto& path : paths)
        if (fs::exists(output / path.filename())) throw std::invalid_argument("output file already exists: " + path.filename().string());
    const auto start = Clock::now();
    std::vector<Image> images(paths.size());
    std::string error;
    for (size_t i = 0; i < paths.size(); ++i)
        if (!load_bmp(paths[i].u8string(), images[i], error)) throw std::runtime_error(error);
    const auto loaded = Clock::now();
    Pipeline chain({Transformation(Filter::Blur), Transformation(Filter::Sharpen), Transformation(Filter::Gray)});
    chain.run(images, mode, threads);
    const auto processed = Clock::now();
    for (size_t i = 0; i < paths.size(); ++i)
        if (!save_bmp((output / paths[i].filename()).u8string(), images[i], error)) throw std::runtime_error(error);
    const auto saved = Clock::now();
    auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
    return {paths.size(), ms(start, loaded), ms(loaded, processed),
            ms(processed, saved), ms(start, saved)};
}
