#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
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

