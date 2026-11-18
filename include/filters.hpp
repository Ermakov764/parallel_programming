#pragma once
#include "image.hpp"
#include <string>
enum class Filter
{
    Blur,     // размытие, окно 3×3
    Sharpen,  // резкость, окно 3×3
    Gray      // серый, соседние пиксели не нужны
};

// A transformation owns its filter; execution policy is supplied by the caller.
class Transformation {
public:
    explicit Transformation(Filter filter) : filter_(filter) {}
    Image apply(const Image& source, int threads = 1) const;
private:
    Filter filter_;
};
bool parse_filter(const std::string&, Filter&);
