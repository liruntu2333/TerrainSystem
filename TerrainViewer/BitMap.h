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
        using PixelFormat = T;

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

        std::vector<T> CopyRectangle(const int x, const int y, const size_t w, const size_t h, const size_t m) const
        {
            std::vector<T> data(w * h);
            const auto src = GetData(m);
            const auto sw = GetMipWidth(m);
            const auto sh = GetMipHeight(m);
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

    template <class T>
    class TiledMap
    {
    public:
        TiledMap(std::vector<std::shared_ptr<T>> src) : m_Sources(std::move(src)),
            m_Width(m_Sources[0]->GetMetadata().width), m_Height(m_Sources[0]->GetMetadata().height)
        {
            if (m_Sources.size() != TILE_X * TILE_Y)
                throw std::exception("Tiled map size mismatch");
            for (auto&& map : m_Sources)

                if (map->GetMetadata().width != m_Width || map->GetMetadata().height != m_Height)
                    throw std::exception("Height map size mismatch");
        }

        ~TiledMap() = default;

        [[nodiscard]] auto& GetVal(int x, int y, const int m) const
        {
            const auto tx = WarpMod(x / GetMipWidth(m), TILE_X);
            const auto ty = WarpMod(y / GetMipHeight(m), TILE_Y);
            const auto ti = ty * TILE_X + tx;
            return m_Sources[ti]->GetVal(x, y, m);
        }

        [[nodiscard]] size_t GetMipWidth(const size_t mip) const
        {
            return m_Width >> mip;
        }

        [[nodiscard]] size_t GetMipHeight(const size_t mip) const
        {
            return m_Height >> mip;
        }

        [[nodiscard]] auto CopyRectangle(
            const int x, const int y, const size_t w, const size_t h, const size_t m) const
        {
            std::vector<typename T::PixelFormat> data(w * h);
            for (size_t i = 0; i < h; ++i)
            {
                for (size_t j = 0; j < w; ++j)
                {
                    data[i * w + j] = GetVal(x + j, y + i, m);
                }
            }
            return data;
        }

    protected:
        static constexpr unsigned TILE_X = 2;
        static constexpr unsigned TILE_Y = 2;
        std::vector<std::shared_ptr<T>> m_Sources {};
        unsigned m_Width = 0;
        unsigned m_Height = 0;
    };
}
