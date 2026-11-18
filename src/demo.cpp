#include "demo.hpp"
#include "bmp.hpp"
#include "filters.hpp"
#include <iostream>
#include <cstdlib>
static int clamp_int(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
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

int run_demo()
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
