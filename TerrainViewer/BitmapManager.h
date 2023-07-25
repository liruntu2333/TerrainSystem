#pragma once
#include <memory>
#include <vector>

#include "BitMap.h"
#include "Texture2D.h"

class BitmapManager
{
public:
    using Rect = DirectX::ClipmapTexture::Rectangle;
    BitmapManager() = default;
    ~BitmapManager() = default;

    void BindSource(
        const std::shared_ptr<DirectX::TiledMap<DirectX::HeightMap>>& heightSrc,
        const std::shared_ptr<DirectX::TiledMap<DirectX::SplatMap>>& splatSrc,
        const std::shared_ptr<DirectX::TiledMap<DirectX::NormalMap>>& normalBase,
        const std::vector<std::shared_ptr<DirectX::AlbedoMap>>& alb,
        const std::vector<std::shared_ptr<DirectX::NormalMap>>& nor
        );

    [[nodiscard]] std::vector<uint16_t> CopyElevation(
        int x, int y, unsigned w, unsigned h, int mip) const;
    [[nodiscard]] std::vector<uint32_t> BlendAlbedoRoughness(
        int x, int y, unsigned w, unsigned h, int mip) const;
    [[nodiscard]] std::vector<uint8_t> BlendAlbedoRoughnessBc3(
        int x, int y, unsigned w, unsigned h, int mip) const;
    [[nodiscard]] std::vector<uint32_t> BlendNormalOcclusion(
        int x, int y, unsigned w, unsigned h, int mip, int method) const;
    [[nodiscard]] std::vector<uint8_t> BlendNormalOcclusionBc3(
        int x, int y, unsigned w, unsigned h, int mip, int method) const;
    [[nodiscard]] float GetPixelHeight(int x, int y, int mip) const;

protected:
    [[nodiscard]] static std::vector<uint8_t> CompressRgba8ToBc3(uint32_t* src, unsigned w, unsigned h);
    [[nodiscard]] static std::vector<uint8_t> CompressRgba8ToBc7(uint32_t* src, unsigned w, unsigned h);

    std::shared_ptr<DirectX::TiledMap<DirectX::HeightMap>> m_HeightTiles = nullptr;
    std::shared_ptr<DirectX::TiledMap<DirectX::SplatMap>> m_SplatTiles = nullptr;
    std::shared_ptr<DirectX::TiledMap<DirectX::NormalMap>> m_NormalTiles = nullptr;
    std::vector<std::shared_ptr<DirectX::AlbedoMap>> m_AlbAtlas { nullptr };
    std::vector<std::shared_ptr<DirectX::NormalMap>> m_NorAtlas { nullptr };
};
