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

    m_Values.reserve(n);
    std::copy_n(data, n, std::back_inserter(m_Values));
    free(data);
}

HeightMap::HeightMap(std::vector<uint16_t> val, const int w) :
    m_Width(w), m_Height(static_cast<int>(val.size()) / w),
    m_Values(std::move(val)) {}

std::shared_ptr<HeightMap> HeightMap::GetCoarser() const
{
    std::vector<uint16_t> cVal;
    const int w = m_Width / 2;
    const int h = m_Height / 2;
    cVal.reserve(w * h);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            uint32_t sum = 0;
            sum += GetVal(x * 2, y * 2);
            sum += GetVal(x * 2 + 1, y * 2);
            sum += GetVal(x * 2, y * 2 + 1);
            sum += GetVal(x * 2 + 1, y * 2 + 1);
            cVal.emplace_back(sum / 4);
        }
    }

    return std::make_shared<HeightMap>(std::move(cVal), w);
}
