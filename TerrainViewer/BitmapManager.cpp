#define NOMINMAX
#include "BitmapManager.h"

#include "BitMap.h"
#include "ClipmapLevel.h"
#include "ispc_texcomp.h"
#include "MaterialBlender.h"

void BitmapManager::BindSource(
    const std::shared_ptr<DirectX::TiledMap<DirectX::HeightMap>>& heightSrc,
    const std::shared_ptr<DirectX::TiledMap<DirectX::SplatMap>>& splatSrc,
    const std::shared_ptr<DirectX::TiledMap<DirectX::NormalMap>>& normalBase)
{
    m_HeightTiles = heightSrc;
    m_SplatTiles = splatSrc;
    m_NormalTiles = normalBase;
}

void BitmapManager::BindMaterial(
    const std::vector<std::shared_ptr<DirectX::AlbedoMap>>& alb,
    const std::vector<std::shared_ptr<DirectX::NormalMap>>& nor)
{
    m_AlbAtlas = alb;
    m_NorAtlas = nor;
}

std::vector<uint16_t> BitmapManager::CopyElevation(int x, int y, unsigned w, unsigned h, int mip) const
{
    return m_HeightTiles->CopyRectangle(x, y, w, h, mip);
}

std::vector<uint32_t> BitmapManager::BlendAlbedoRoughness(int x, int y, unsigned w, unsigned h, int mip) const
{
    return AlbedoBlender(m_SplatTiles, m_AlbAtlas).Blend(x, y, w, h, mip);
}

std::vector<uint8_t> BitmapManager::BlendAlbedoRoughnessBc3(int x, int y, unsigned w, unsigned h, int mip) const
{
    return CompressRgba8ToBc3(BlendAlbedoRoughness(x, y, w, h, mip).data(),
        w * AlbedoBlender::SampleRatio, h * AlbedoBlender::SampleRatio);
}

std::vector<uint32_t> BitmapManager::BlendNormalOcclusion(
    int x, int y, unsigned w, unsigned h, int mip, int method) const
{
    return NormalBlender(m_SplatTiles, m_NormalTiles, m_NorAtlas).Blend(x, y, w, h, mip,
        static_cast<NormalBlender::BlendMethod>(method));
}

std::vector<uint8_t> BitmapManager::BlendNormalOcclusionBc3(
    int x, int y, unsigned w, unsigned h, int mip, int method) const
{
    return CompressRgba8ToBc3(BlendNormalOcclusion(x, y, w, h, mip, method).data(),
        w * NormalBlender::SampleRatio, h * NormalBlender::SampleRatio);
}

float BitmapManager::GetPixelHeight(int x, int y, int mip) const
{
    return static_cast<float>(m_HeightTiles->GetVal(x, y, mip)) * (1.0f / 65535);
}

std::vector<uint8_t> BitmapManager::CompressRgba8ToBc3(uint32_t* src, unsigned w, unsigned h)
{
    // 16 bytes per 4x4 pixel <=> 1 bytes per pixel
    std::vector<std::uint8_t> dst(w * h);
    rgba_surface surface;
    surface.width = w;
    surface.height = h;
    surface.stride = w * sizeof(uint32_t);
    surface.ptr = reinterpret_cast<uint8_t*>(src);
    CompressBlocksBC3(&surface, dst.data());
    return dst;
}

std::vector<uint8_t> BitmapManager::CompressRgba8ToBc7(uint32_t* src, unsigned w, unsigned h)
{
    std::vector<std::uint8_t> dst(w * h);
    rgba_surface surface;
    surface.width = w;
    surface.height = h;
    surface.stride = w * sizeof(uint32_t);
    surface.ptr = reinterpret_cast<uint8_t*>(src);
    bc7_enc_settings settings;
    GetProfile_ultrafast(&settings);
    CompressBlocksBC7(&surface, dst.data(), &settings);
    return dst;
}
