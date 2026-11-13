// Коллективный проект, чекпоинт: последовательные фильтры и начатый параллельный вариант.
//
//   make demo
//
// Свой файл:
//   ./filter вход.bmp выход.bmp размытие|резкость|серый [потоки]
//
// Резкость и серый считаются в одном потоке.
// Размытие при 1 потоке идёт по всем строкам, при 4 — полосами строк.
// Результат пишется в отдельный буфер, исходная картинка только читается.

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Кадр в памяти. Фильтры читают src и пишут в другой Image, исходник не портят.
struct Image
{
    int width = 0;
    int height = 0;
    // Строки сверху вниз. В пикселе три байта в порядке BMP: B, G, R.
    std::vector<uint8_t> data;

    Image() = default;
    Image(int w, int h) : width(w), height(h), data(static_cast<size_t>(w) * h * 3, 0) {}

    // Индекс пикселя: строка y, столбец x, дальше 3 байта цвета.
    uint8_t* pixel(int x, int y)
    {
        return data.data() + (static_cast<size_t>(y) * width + x) * 3;
    }

    const uint8_t* pixel(int x, int y) const
    {
        return data.data() + (static_cast<size_t>(y) * width + x) * 3;
    }
};

enum class Filter
{
    Blur,     // размытие, окно 3×3
    Sharpen,  // резкость, окно 3×3
    Gray      // серый, соседние пиксели не нужны
};

// Полоса строк для одного потока: [y0, y1).
struct Band
{
    int y0;
    int y1;
};

// Ядро 3×3 в одну строку, слева направо, сверху вниз.
// div — на сколько делим сумму, чтобы вернуть обычную яркость.
struct Kernel
{
    int w[9];
    int div;
};

// BMP хранит числа little-endian: младший байт идёт первым.

static uint16_t read_u16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

static uint32_t read_u32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static void append_u16(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back(static_cast<uint8_t>(v & 255));
    b.push_back(static_cast<uint8_t>((v >> 8) & 255));
}

static void append_u32(std::vector<uint8_t>& b, uint32_t v)
{
    b.push_back(static_cast<uint8_t>(v & 255));
    b.push_back(static_cast<uint8_t>((v >> 8) & 255));
    b.push_back(static_cast<uint8_t>((v >> 16) & 255));
    b.push_back(static_cast<uint8_t>((v >> 24) & 255));
}

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

// В BMP длина строки в файле кратна 4 байтам. Лишние байты в конце строки — пустые.
static int row_stride(int width)
{
    int raw = width * 3;
    return (raw + 3) & ~3;
}

// Заголовок BMP разбираем по смещениям, без готовой библиотеки картинок.
bool load_bmp(const std::string& path, Image& out, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        error = "не удалось открыть " + path;
        return false;
    }

    uint8_t header[54];
    file.read(reinterpret_cast<char*>(header), 54);
    if (!file)
    {
        error = "файл короче заголовка BMP";
        return false;
    }

    if (header[0] != 'B' || header[1] != 'M')
    {
        error = "это не BMP (нет сигнатуры BM)";
        return false;
    }

    // Смещения от начала файла, заголовок 54 байта (14 файловых + 40 информационных).
    const uint32_t pixel_offset = read_u32(header + 10);                 // где начинаются пиксели
    const int32_t width = static_cast<int32_t>(read_u32(header + 18));   // ширина
    const int32_t height = static_cast<int32_t>(read_u32(header + 22));  // высота
    const uint16_t planes = read_u16(header + 26);                       // должно быть 1
    const uint16_t bit_count = read_u16(header + 28);                    // бит на пиксель
    const uint32_t compression = read_u32(header + 30);                  // 0 — без сжатия

    if (width <= 0 || height <= 0)
    {
        error = "нужна положительная ширина и высота (кадр снизу вверх)";
        return false;
    }
    if (planes != 1 || bit_count != 24 || compression != 0)
    {
        error = "нужен несжатый BMP, 24 бита на пиксель";
        return false;
    }
    if (pixel_offset < 54)
    {
        error = "некорректное смещение пикселей";
        return false;
    }

    file.seekg(pixel_offset, std::ios::beg);
    Image image(width, height);
    const int stride = row_stride(width);
    const int padding = stride - width * 3;
    std::vector<char> pad(static_cast<size_t>(padding), 0);

    // В файле первая строка — нижняя. В памяти y = 0 — верх кадра.
    for (int y = height - 1; y >= 0; --y)
    {
        file.read(reinterpret_cast<char*>(image.pixel(0, y)), width * 3);
        if (padding > 0)
            file.read(pad.data(), padding);
        if (!file)
        {
            error = "файл обрезан на строке пикселей";
            return false;
        }
    }

    out = std::move(image);
    return true;
}

// Запись того же 24-битного BMP. Диск здесь последовательный: один файл целиком.
bool save_bmp(const std::string& path, const Image& image, std::string& error)
{
    if (image.width <= 0 || image.height <= 0)
    {
        error = "пустая картинка";
        return false;
    }

    const int stride = row_stride(image.width);
    const int padding = stride - image.width * 3;
    const uint32_t pixel_bytes = static_cast<uint32_t>(stride) * static_cast<uint32_t>(image.height);
    const uint32_t file_size = 54u + pixel_bytes;

    std::vector<uint8_t> header;
    header.reserve(54);
    header.push_back('B');
    header.push_back('M');
    append_u32(header, file_size);                                  // размер всего файла
    append_u16(header, 0);                                          // зарезервировано
    append_u16(header, 0);
    append_u32(header, 54);                                         // смещение пикселей
    append_u32(header, 40);                                         // размер инфозаголовка
    append_u32(header, static_cast<uint32_t>(image.width));
    append_u32(header, static_cast<uint32_t>(image.height));
    append_u16(header, 1);                                          // плоскости
    append_u16(header, 24);                                         // 24 бита, три байта на пиксель
    append_u32(header, 0);                                          // без сжатия
    append_u32(header, pixel_bytes);
    append_u32(header, 2835);                                       // 72 точки на дюйм по горизонтали
    append_u32(header, 2835);
    append_u32(header, 0);                                          // палитра не используется
    append_u32(header, 0);

    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        error = "не удалось записать " + path;
        return false;
    }

    file.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
    const char pad[3] = {0, 0, 0};
    // В файл строка пишется снизу вверх: так устроен BMP.
    for (int y = image.height - 1; y >= 0; --y)
    {
        file.write(reinterpret_cast<const char*>(image.pixel(0, y)), image.width * 3);
        if (padding > 0)
            file.write(pad, padding);
    }

    if (!file)
    {
        error = "ошибка записи " + path;
        return false;
    }
    return true;
}

// Размытие: каждый сосед входит с весом 1, сумма делится на 9.
// Это среднее девяти пикселей окна, включая сам центр.
// Край кадра ниже не отбрасываем, а повторяем крайний пиксель, чтобы рамка не темнела.
//   1 1 1
//   1 1 1
//   1 1 1   / 9
static Kernel blur_kernel()
{
    Kernel k = {{1, 1, 1, 1, 1, 1, 1, 1, 1}, 9};
    return k;
}

// Резкость: центр берём пять раз и вычитаем четырёх прямых соседей.
// Сумма весов равна 1, поэтому ровный цвет не меняется, а перепад становится контрастнее.
//   0 -1  0
//  -1  5 -1
//   0 -1  0   / 1
static Kernel sharpen_kernel()
{
    Kernel k = {{0, -1, 0, -1, 5, -1, 0, -1, 0}, 1};
    return k;
}

// Серый — не свёртка. Для каждого пикселя одно среднее трёх каналов.
// y0 и y1 задают полосу строк: ту же функцию вызывает и один поток, и каждый из нескольких.

static void gray_rows(const Image& src, Image& dst, int y0, int y1)
{
    for (int y = y0; y < y1; ++y)
    {
        for (int x = 0; x < src.width; ++x)
        {
            const uint8_t* p = src.pixel(x, y);
            const int b = p[0]; // в файле сначала синий
            const int g = p[1];
            const int r = p[2]; // последний байт пикселя — красный
            const uint8_t gray = static_cast<uint8_t>((r + g + b) / 3);
            uint8_t* q = dst.pixel(x, y);
            q[0] = gray;
            q[1] = gray;
            q[2] = gray;
        }
    }
}

// Свёртка 3×3 только по строкам [y0, y1). Соседей можно читать и за пределами полосы:
// верхний поток смотрит строку нижнего, но пишет только в свои строки dst.
static void conv_rows(const Image& src, Image& dst, const Kernel& kernel, int y0, int y1)
{
    for (int y = y0; y < y1; ++y)
    {
        for (int x = 0; x < src.width; ++x)
        {
            int sum_b = 0;
            int sum_g = 0;
            int sum_r = 0;
            int k = 0;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    // Сосед за границей кадра заменяется крайним пикселем.
                    const int xx = clamp_int(x + dx, 0, src.width - 1);
                    const int yy = clamp_int(y + dy, 0, src.height - 1);
                    const uint8_t* p = src.pixel(xx, yy);
                    sum_b += p[0] * kernel.w[k];
                    sum_g += p[1] * kernel.w[k];
                    sum_r += p[2] * kernel.w[k];
                    ++k;
                }
            }
            // После резкости сумма может выйти за 0..255, поэтому обрезаем.
            uint8_t* q = dst.pixel(x, y);
            q[0] = static_cast<uint8_t>(clamp_int(sum_b / kernel.div, 0, 255));
            q[1] = static_cast<uint8_t>(clamp_int(sum_g / kernel.div, 0, 255));
            q[2] = static_cast<uint8_t>(clamp_int(sum_r / kernel.div, 0, 255));
        }
    }
}

// Один и тот же проход по полосе. Меняется только фильтр, не способ нарезки строк.
static void process_rows(const Image& src, Image& dst, Filter filter, int y0, int y1)
{
    if (filter == Filter::Gray)
        gray_rows(src, dst, y0, y1);
    else if (filter == Filter::Blur)
        conv_rows(src, dst, blur_kernel(), y0, y1);
    else
        conv_rows(src, dst, sharpen_kernel(), y0, y1);
}

// Делим высоту на threads полос. Остаток строк раздаём первым полосам,
// чтобы все строки попали в какой-то поток и полосы не пересекались.
// Пример для 1080 строк и 4 потоков: [0, 270) [270, 540) [540, 810) [810, 1080).
static std::vector<Band> make_bands(int height, int threads)
{
    std::vector<Band> bands;
    const int base = height / threads;
    int extra = height % threads;
    int y = 0;
    for (int t = 0; t < threads; ++t)
    {
        const int rows = base + (extra > 0 ? 1 : 0);
        if (extra > 0)
            --extra;
        bands.push_back(Band{y, y + rows});
        y += rows;
    }
    return bands;
}

// Последовательный вариант — threads == 1: один вызов на весь кадр.
// Параллельный вариант пока используется для размытия: те же process_rows по полосам строк.
static Image apply_filter(const Image& src, Filter filter, int threads)
{
    if (threads < 1)
        threads = 1;
    if (threads > src.height)
        threads = src.height;
    // Резкость и серый на этом этапе считаются одним потоком.
    if (filter != Filter::Blur)
        threads = 1;

    Image dst(src.width, src.height); // отдельный выходной буфер

    if (threads == 1)
    {
        process_rows(src, dst, filter, 0, src.height);
    }
    else
    {
        // Запись только в свою полосу dst. src потоки не меняют, мьютекс не нужен.
        const std::vector<Band> bands = make_bands(src.height, threads);
        std::vector<std::thread> pool;
        pool.reserve(static_cast<size_t>(threads));
        for (Band band : bands)
        {
            pool.emplace_back([&src, &dst, filter, band]() {
                process_rows(src, dst, filter, band.y0, band.y1);
            });
        }
        // Ждём все полосы: дальше dst уже можно сохранять.
        for (std::thread& worker : pool)
            worker.join();
    }
    return dst;
}

static bool parse_filter(const std::string& name, Filter& filter)
{
    if (name == "размытие" || name == "blur")
        filter = Filter::Blur;
    else if (name == "резкость" || name == "sharpen")
        filter = Filter::Sharpen;
    else if (name == "серый" || name == "gray")
        filter = Filter::Gray;
    else
        return false;
    return true;
}

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

static void fill_rect(Image& image, int x0, int y0, int x1, int y1, int r, int g, int b)
{
    x0 = clamp_int(x0, 0, image.width);
    x1 = clamp_int(x1, 0, image.width);
    y0 = clamp_int(y0, 0, image.height);
    y1 = clamp_int(y1, 0, image.height);
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            uint8_t* p = image.pixel(x, y);
            p[0] = static_cast<uint8_t>(b);
            p[1] = static_cast<uint8_t>(g);
            p[2] = static_cast<uint8_t>(r);
        }
    }
}

// Простой кадр, чтобы на картинке было видно границу фильтра. Высота 1080 — как в примере с полосами.
static Image make_demo_image()
{
    const int width = 1920;
    const int height = 1080;
    Image image(width, height);
    fill_rect(image, 0, 0, width, height, 180, 190, 200);
    fill_rect(image, 200, 180, 700, 700, 200, 40, 40);
    fill_rect(image, 760, 180, 1260, 700, 40, 150, 50);
    fill_rect(image, 1320, 180, 1720, 700, 40, 70, 190);
    fill_rect(image, 200, 760, 1720, 960, 20, 20, 20);
    return image;
}

// Пять картинок для показа: исходник, три фильтра в одном потоке и размытие в четыре.
static int run_demo()
{
    Image demo = make_demo_image();
    std::string error;
    Image blur = apply_filter(demo, Filter::Blur, 1);
    Image sharp = apply_filter(demo, Filter::Sharpen, 1);
    Image gray = apply_filter(demo, Filter::Gray, 1);
    Image blur4 = apply_filter(demo, Filter::Blur, 4);

    const struct SaveItem
    {
        const char* path;
        const Image* image;
    } items[] = {
        {"samples/input.bmp", &demo},
        {"samples/blur.bmp", &blur},
        {"samples/sharpen.bmp", &sharp},
        {"samples/gray.bmp", &gray},
        {"samples/blur_4.bmp", &blur4},
    };

    for (const SaveItem& item : items)
    {
        if (!save_bmp(item.path, *item.image, error))
        {
            std::cerr << error << '\n';
            return 1;
        }
    }

    std::cout << "Откройте samples/demo.html\n";
    if (std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr)
    {
        const int opened = std::system("xdg-open samples/demo.html >/dev/null 2>&1");
        if (opened != 0)
            std::cerr << "не удалось открыть браузер, файл лежит в samples/demo.html\n";
    }
    return 0;
}

// Либо --demo, либо обработка одного файла. Диск читается и пишется в этом потоке.
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
