#include "bmp.hpp"
#include <fstream>
#include <utility>
#include <limits>
static uint16_t read_u16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

static uint32_t read_u32(const uint8_t* p)
{
    return static_cast<uint32_t>(uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24));
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

static int row_stride(int width)
{
    int raw = width * 3;
    return (raw + 3) & ~3;
}

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
    const uint32_t dib_size = read_u32(header + 14);
    if (dib_size < 40 || uint64_t(pixel_offset) < 14u + uint64_t(dib_size))
    {
        error = "некорректное смещение пикселей";
        return false;
    }

    const uint64_t stride64 = (uint64_t(width) * 3 + 3) & ~uint64_t(3);
    file.seekg(0, std::ios::end);
    const auto length = file.tellg();
    if (stride64 > uint64_t(std::numeric_limits<int>::max()) ||
        length < 0 || uint64_t(length) < uint64_t(pixel_offset) + stride64 * uint64_t(height)) {
        error = "invalid BMP dimensions or truncated pixel data";
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

    file.close();
    if (!file)
    {
        error = "ошибка записи " + path;
        return false;
    }
    return true;
}
