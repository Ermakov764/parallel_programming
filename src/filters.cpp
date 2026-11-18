#include "filters.hpp"
#include <thread>
#include <stdexcept>
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

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}
static Kernel blur_kernel()
{
    Kernel k = {{1, 1, 1, 1, 1, 1, 1, 1, 1}, 9};
    return k;
}

static Kernel sharpen_kernel()
{
    Kernel k = {{0, -1, 0, -1, 5, -1, 0, -1, 0}, 1};
    return k;
}

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

static void process_rows(const Image& src, Image& dst, Filter filter, int y0, int y1)
{
    if (filter == Filter::Gray)
        gray_rows(src, dst, y0, y1);
    else if (filter == Filter::Blur)
        conv_rows(src, dst, blur_kernel(), y0, y1);
    else
        conv_rows(src, dst, sharpen_kernel(), y0, y1);
}

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

Image Transformation::apply(const Image& src, int threads) const
{
    const Filter filter = filter_;
    if (src.width <= 0 || src.height <= 0)
        throw std::invalid_argument("empty source image");
    if (threads < 1)
        threads = 1;
    if (threads > src.height)
        threads = src.height;
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
        try {
        for (Band band : bands)
        {
            pool.emplace_back([&src, &dst, filter, band]() {
                process_rows(src, dst, filter, band.y0, band.y1);
            });
        }
        } catch (...) {
            for (auto& worker : pool) worker.join();
            throw;
        }
        // Ждём все полосы: дальше dst уже можно сохранять.
        for (std::thread& worker : pool)
            worker.join();
    }
    return dst;
}

bool parse_filter(const std::string& name, Filter& filter)
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
