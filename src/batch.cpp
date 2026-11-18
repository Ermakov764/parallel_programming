#include "batch.hpp"
#include "bmp.hpp"
#include "pipeline.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

int run_batch(int argc, char** argv) {
    namespace fs = std::filesystem;
    using Clock = std::chrono::steady_clock;
    if (argc != 7) throw std::invalid_argument("usage: --batch INPUT_DIR OUTPUT_DIR image|stage|pipeline THREADS");
    const fs::path input = fs::u8path(argv[2]);
    const fs::path output = fs::u8path(argv[3]);
    if (fs::weakly_canonical(input) == fs::weakly_canonical(output))
        throw std::invalid_argument("input and output directories must differ");
    const std::string mode_name = argv[4];
    ExecutionMode mode;
    if (mode_name == "image") mode = ExecutionMode::Image;
    else if (mode_name == "stage") mode = ExecutionMode::Stage;
    else if (mode_name == "pipeline") mode = ExecutionMode::Pipeline;
    else throw std::invalid_argument("unknown mode");
    size_t used = 0;
    const int threads = std::stoi(argv[5], &used);
    if (used != std::string(argv[5]).size() || threads < 1)
        throw std::invalid_argument("threads must be a positive integer");
    if (std::string(argv[6]) != "--csv") throw std::invalid_argument("expected --csv");
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
    std::cout << std::fixed << std::setprecision(3) << mode_name << ',' << threads << ',' << paths.size() << ','
              << ms(start, loaded) << ',' << ms(loaded, processed) << ',' << ms(processed, saved) << ',' << ms(start, saved) << '\n';
    return 0;
}
