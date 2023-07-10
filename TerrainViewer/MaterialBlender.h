#pragma once

#include <vector>
#include "BitMap.h"

class MaterialBlender
{
public:
    explicit MaterialBlender(const std::shared_ptr<DirectX::SplatMap>& splat) : m_Splat(splat) {}
    virtual ~MaterialBlender() = default;

    [[nodiscard]] virtual std::vector<uint32_t> Blend(
        int x, int y, unsigned w, unsigned h, unsigned mip) const = 0;

protected:
    static constexpr unsigned SampleRatio = 8;
    std::shared_ptr<DirectX::SplatMap> m_Splat;
};

class AlbedoBlender : public MaterialBlender
{
public:
    AlbedoBlender(
        const std::shared_ptr<DirectX::SplatMap>& splat,
        const std::vector<std::shared_ptr<DirectX::AlbedoMap>>& atlas) :
        MaterialBlender(splat), m_Atlas(atlas) {}

    ~AlbedoBlender() override = default;

    [[nodiscard]] std::vector<uint32_t> Blend(
        int x, int y, unsigned w, unsigned h, unsigned mip) const override;

protected:
    std::vector<std::shared_ptr<DirectX::AlbedoMap>> m_Atlas {};
};
