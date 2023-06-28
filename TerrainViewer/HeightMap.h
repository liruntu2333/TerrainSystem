#pragma once

#include <filesystem>

class HeightMap
{
public:
    HeightMap(const std::filesystem::path& path);
    HeightMap(std::vector<uint16_t> val, int w);

    [[nodiscard]] uint16_t GetVal(int x, int y) const
    {
        x = Warp(x, m_Width);
        y = Warp(y, m_Height);
        return m_Values[y * m_Width + x];
    }

    [[nodiscard]] const uint16_t* GetData() const
    {
        return m_Values.data();
    }

    [[nodiscard]] int GetWidth() const
    {
        return m_Width;
    }

    [[nodiscard]] int GetHeight() const
    {
        return m_Height;
    }

    [[nodiscard]] float GetHeight(int x, int y) const
    {
        return static_cast<float>(GetVal(x, y)) * (1.0f / 65535.f);
    }

    [[nodiscard]] std::shared_ptr<HeightMap> GetCoarser() const;

private:
    static int Warp(int n, int m)
    {
        // warp a int and get its positive mod
        return (n % m + m) % m;
    }

    int m_Width {};
    int m_Height {};
    std::vector<uint16_t> m_Values {};
};
