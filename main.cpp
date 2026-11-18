#include "bmp.hpp"
#include "filters.hpp"
#include "demo.hpp"
#include <iostream>
#include <cstdlib>
static void usage()
{
    std::cerr
        << "Использование:\n"
        << "  ./filter --demo\n"
        << "  ./filter вход.bmp выход.bmp размытие|резкость|серый [потоки]\n"
        << "\n"
        << "Потоки имеют смысл для размытия: 1 или 4.\n"
        << "Резкость и серый считаются в одном потоке.\n";
}
int main(int argc, char** argv)
{
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

    int threads = 1;
    if (argc == 5)
    {
        threads = std::atoi(argv[4]);
        if (threads < 1)
        {
            std::cerr << "число потоков должно быть >= 1\n";
            return 1;
        }
    }

    Image image;
    std::string error;
    if (!load_bmp(input_path, image, error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "кадр " << image.width << "x" << image.height << '\n';

    Image result = apply_filter(image, filter, threads);
    if (!save_bmp(output_path, result, error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "записан " << output_path << '\n';
    return 0;
}
