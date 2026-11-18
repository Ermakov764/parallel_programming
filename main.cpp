#include "bmp.hpp"
#include "batch.hpp"
#include <exception>
#include <stdexcept>
#include "filters.hpp"
#include "demo.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>
static void usage()
{
    std::cerr
        << "Использование:\n"
        << "  ./filter --demo\n"
        << "  ./filter --batch INPUT_DIR OUTPUT_DIR file-first|stage-first|pipeline THREADS --csv|--cpu-csv\n"
        << "  ./filter вход.bmp выход.bmp размытие|резкость|серый [потоки]\n"
        << "\nThreads: positive integer, supported by every filter.\n";
}
static int parse_threads(const std::string& value)
{
    size_t used = 0;
    const int threads = std::stoi(value, &used);
    if (used != value.size() || threads < 1)
        throw std::invalid_argument("threads must be a positive integer");
    return threads;
}

static ExecutionMode parse_mode(const std::string& value)
{
    if (value == "file-first") return ExecutionMode::FileFirst;
    if (value == "stage-first") return ExecutionMode::StageFirst;
    if (value == "pipeline") return ExecutionMode::Pipeline;
    throw std::invalid_argument("unknown mode");
}

static int run_batch_command(int argc, char** argv)
{
    if (argc != 7)
        throw std::invalid_argument("usage: --batch INPUT_DIR OUTPUT_DIR file-first|stage-first|pipeline THREADS --csv|--cpu-csv");
    const std::string mode_name = argv[4];
    const ExecutionMode mode = parse_mode(mode_name);
    const int threads = parse_threads(argv[5]);
    const bool measure_cpu = std::string(argv[6]) == "--cpu-csv";
    if (!measure_cpu && std::string(argv[6]) != "--csv") throw std::invalid_argument("expected --csv");

    const auto result = run_batch(std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]),
                                  mode, threads, measure_cpu);
    std::cout << std::fixed << std::setprecision(3) << mode_name << ',' << threads << ','
              << result.images << ',' << result.read_ms << ',' << result.process_ms << ','
              << result.write_ms << ',' << result.total_ms;
    if (measure_cpu) std::cout << ',' << result.cpu_ms << ',' << result.filter_cpu_ms;
    std::cout << '\n';
    return 0;
}

static int run(int argc, char** argv)
{
    if (argc >= 2 && std::string(argv[1]) == "--batch")
        return run_batch_command(argc, argv);
    if (argc >= 2 && std::string(argv[1]) == "--demo")
        return run_demo();

    if (argc != 4 && argc != 5)
    {
        usage();
        return 1;
    }

    const std::string input_path = argv[1];
    const std::string output_path = argv[2];
    Filter filter = Filter::Blur;
    if (!parse_filter(argv[3], filter))
    {
        usage();
        return 1;
    }

    const int threads = argc == 5 ? parse_threads(argv[4]) : 1;

    Image image;
    std::string error;
    if (!load_bmp(input_path, image, error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "кадр " << image.width << "x" << image.height << '\n';

    Image result = Transformation(filter).apply(image, threads);
    if (!save_bmp(output_path, result, error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "записан " << output_path << '\n';
    return 0;
}

int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception& error) { std::cerr << error.what() << "\n"; return 1; }
}
