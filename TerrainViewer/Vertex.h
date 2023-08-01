#pragma once
#include <cstdint>
#include <d3d11.h>
#include <directxtk/SimpleMath.h>

struct MeshVertex
{
    uint8_t PositionX {};
    uint8_t PositionY {};
    //uint8_t MorphX;
    //uint8_t MorphZ;

    static constexpr unsigned int InputElementCount = 1;
    static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];

    MeshVertex(const uint8_t& PositionX, const uint8_t& PositionY)
        : PositionX(PositionX), PositionY(PositionY) { }

    MeshVertex() = default;
};

struct GridVertex
{
    DirectX::PackedVector::XMUBYTE2 Position {};

    static constexpr unsigned int InputElementCount = 1;
    static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];

    GridVertex(uint8_t x, uint8_t y) : Position(x, y) { }
};

struct GridInstance // TODO
{
    float GridScale;             // grid spacing
    float CoarserStepRate;          // 1 / 256 when at the coarsest level
    DirectX::SimpleMath::Vector2 WorldOffset;           // offset in world
    DirectX::SimpleMath::Vector2 TextureOffsetFiner;    // offset in fine texture
    DirectX::SimpleMath::Vector2 TextureOffsetCoarser;  // offset in coarse texture
    DirectX::XMUINT2 LocalOffset;                       // offset in level
    uint32_t Color;
    uint32_t Level;

    static constexpr unsigned int InputElementCount = 3;
    static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};
