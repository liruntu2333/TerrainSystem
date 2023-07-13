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
        using TexelFormat = T;

        BitMap() = default;
        ~BitMap() = default;
        BitMap(const std::filesystem::path& path);

        [[nodiscard]] T& GetVal(int x, int y, const int m) const
        {
            x = WarpMod(x, GetMipWidth(m));
            y = WarpMod(y, GetMipHeight(m));
            return GetData(m)[y * GetMipWidth(m) + x];
        }

        [[nodiscard]] T* GetData(const int m) const
        {
            return reinterpret_cast<T*>(GetImage(m, 0, 0)->pixels);
        }

        [[nodiscard]] size_t GetMipWidth(const size_t mip) const
        {
            return GetMetadata().width >> mip;
        }

        [[nodiscard]] size_t GetMipHeight(const size_t mip) const
        {
            return GetMetadata().height >> mip;
        }

        std::vector<T> CopyRectangle(const int x, const int y, const size_t w, const size_t h, const size_t m)
        {
            std::vector<T> data;
            const auto src = GetData(m);
            const auto sw = GetMipWidth(m);
            const auto sh = GetMipHeight(m);
            data.resize(w * h);
            for (size_t i = 0; i < h; ++i)
            {
                for (size_t j = 0; j < w; ++j)
                {
                    data[i * w + j] = src[WarpMod(y + i, sh) * sw + WarpMod(x + j, sw)];
                }
            }
            return data;
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

    class AlbedoMap : public BitMap<uint32_t>
    {
    public:
        AlbedoMap() = default;
        ~AlbedoMap() = default;
        AlbedoMap(const std::filesystem::path& path) : BitMap(path) { }
    };

    class SplatMap : public BitMap<uint32_t>
    {
    public:
        SplatMap() = default;
        ~SplatMap() = default;
        SplatMap(const std::filesystem::path& path) : BitMap(path) { }
    };

    class NormalMap : public BitMap<uint32_t>
    {
    public:
        NormalMap() = default;
        ~NormalMap() = default;
        NormalMap(const std::filesystem::path& path) : BitMap(path) { }
    };
}
