#pragma once
#include "image.hpp"
#include <string>
enum class Filter
{
    Blur,     // размытие, окно 3×3
    Sharpen,  // резкость, окно 3×3
    Gray      // серый, соседние пиксели не нужны
};

// Полоса строк для одного потока: [y0, y1).
Image apply_filter(const Image&, Filter, int);
bool parse_filter(const std::string&, Filter&);
