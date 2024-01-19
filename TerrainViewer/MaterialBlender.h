#pragma once

#include <vector>
#include "BitMap.h"

class MaterialBlender
{
public:
    MaterialBlender(std::shared_ptr<DirectX::TiledMap<DirectX::SplatMap>> splat) :
        m_Splat(std::move(splat)) {}

    ~MaterialBlender() = default;

    [[nodiscard]] std::vector<uint32_t> Blend(
        int x, int y, unsigned w, unsigned h, unsigned mip) const { return {}; }

protected:
    std::shared_ptr<DirectX::TiledMap<DirectX::SplatMap>> m_Splat;
};

class AlbedoBlender : public MaterialBlender
{
public:
    AlbedoBlender(
        std::shared_ptr<DirectX::TiledMap<DirectX::SplatMap>> splat,
        std::vector<std::shared_ptr<DirectX::AlbedoMap>> atlas,
        std::vector<std::shared_ptr<DirectX::HeightMap>> hgtAtlas) :
        MaterialBlender(std::move(splat)), m_Atlas(std::move(atlas)),
        m_HgtAtlas(std::move(hgtAtlas)) {}

    ~AlbedoBlender() = default;

    [[nodiscard]] std::vector<uint32_t> Blend(
        int splatX, int splatY, unsigned splatW, unsigned splatH, unsigned mip) const;

    static constexpr unsigned SampleRatio = 8;

protected:
    std::vector<std::shared_ptr<DirectX::AlbedoMap>> m_Atlas {};
    std::vector<std::shared_ptr<DirectX::HeightMap>> m_HgtAtlas {};
};

class NormalBlender : public MaterialBlender
{
public:
    NormalBlender(
        std::shared_ptr<DirectX::TiledMap<DirectX::SplatMap>> splat,
        std::shared_ptr<DirectX::TiledMap<DirectX::NormalMap>> base,
        std::vector<std::shared_ptr<DirectX::NormalMap>> atlas,
        std::vector<std::shared_ptr<DirectX::HeightMap>> hgtAtlas) :
        MaterialBlender(std::move(splat)), m_Base(std::move(base)), m_Detail(std::move(atlas)),
        m_HgtAtlas(std::move(hgtAtlas)), HgtAtlas(std::move(hgtAtlas)) {}

    ~NormalBlender() = default;

    enum BlendMethod
    {
        BaseOnly = 0,
        DetailOnly,
        Linear,
        PartialDerivative,
        Whiteout,
        Unreal,
        Reorient,
        Unity,
    };

    [[nodiscard]] std::vector<uint32_t> Blend(
        int splatX, int splatY, unsigned splatW, unsigned splatH, unsigned mip, BlendMethod method) const;

    static constexpr unsigned SampleRatio = 8;

protected:
    std::shared_ptr<DirectX::TiledMap<DirectX::NormalMap>> m_Base;
    std::vector<std::shared_ptr<DirectX::NormalMap>> m_Detail {};
    std::vector<std::shared_ptr<DirectX::HeightMap>> m_HgtAtlas {};
    std::vector<std::shared_ptr<DirectX::HeightMap>> HgtAtlas;
};
