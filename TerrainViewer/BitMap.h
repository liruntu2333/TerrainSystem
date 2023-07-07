#pragma once

#include <filesystem>
#include <DirectXTex.h>
#include "D3DHelper.h"

namespace DirectX
{
    template <class T>
    class BitMap : public ScratchImage
    {
    public:
        BitMap() = default;
        ~BitMap() = default;
        BitMap(const std::filesystem::path& path);

        [[nodiscard]] T GetVal(int x, int y, int m) const
        {
            x = WarpMod(x, GetHeight());
            y = WarpMod(y, GetHeight());
            return GetData(m)[y * GetHeight() + x];
        }

        [[nodiscard]] const T* GetData(int m) const
        {
            return reinterpret_cast<const T*>(GetImage(m, 0, 0)->pixels);
        }

        [[nodiscard]] size_t GetWidth() const
        {
            return GetMetadata().width;
        }

        [[nodiscard]] size_t GetHeight() const
        {
            return GetMetadata().height;
        }
    };

    template <class T>
    BitMap<T>::BitMap(const std::filesystem::path& path)
    {
        ThrowIfFailed(LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, nullptr, *this));
    }

    class HeightMap : public BitMap<uint16_t>
    {
    public:
        HeightMap() = default;
        ~HeightMap() = default;
        HeightMap(const std::filesystem::path& path) : BitMap(path) { }

        [[nodiscard]] float GetPixelHeight(int x, int y, int m) const
        {
            return static_cast<float>(GetVal(x, y, m)) * (1.0f / 65535.f);
        }
    };

    class ColorMap : public BitMap<uint32_t>
    {
    public:
        ColorMap() = default;
        ~ColorMap() = default;
        ColorMap(const std::filesystem::path& path) : BitMap(path) { }
    };
}
