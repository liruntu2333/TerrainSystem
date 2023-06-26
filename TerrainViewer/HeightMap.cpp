#include "HeightMap.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

HeightMap::HeightMap(const std::filesystem::path& path)
{
    int w, h, c;
    uint16_t* data = stbi_load_16(path.generic_u8string().c_str(), &w, &h, &c, 1);
    if (!data) return;

    m_Width = w;
    m_Height = h;
    const int n = w * h;
    constexpr float m = 1.f / 65535.f;
    m_Values.reserve(n);
    std::transform(data, data + n, std::back_inserter(m_Values),
        [m](uint16_t v) { return v * m; });
    free(data);
}
