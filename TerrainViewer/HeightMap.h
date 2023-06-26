#pragma once

#include <filesystem>

class HeightMap
{
public:
    HeightMap(const std::filesystem::path& path);

    [[nodiscard]] float Get(int x, int y) const
    {
        if (x < 0) x = (x % m_Width + m_Width) % m_Width;
        else x %= m_Width;
        if (y < 0) y = (y % m_Height + m_Height) % m_Height;
        else y %= m_Height;
        return m_Values[y * m_Width + x];
    }

private:
    int m_Width {};
    int m_Height {};
    std::vector<float> m_Values {};
};
