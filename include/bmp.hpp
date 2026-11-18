#pragma once
#include "image.hpp"
#include <string>
bool load_bmp(const std::string&, Image&, std::string&);
bool save_bmp(const std::string&, const Image&, std::string&);
