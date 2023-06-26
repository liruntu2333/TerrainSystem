#pragma once
#include <cstdint>
#include <d3d11.h>
#include <directxtk/SimpleMath.h>

struct MeshVertex
{
    uint8_t PositionX{};
    uint8_t PositionY{};
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
    DirectX::PackedVector::XMUBYTE2 Position{};

    static constexpr unsigned int InputElementCount = 1;
    static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];

    GridVertex(uint8_t x, uint8_t y) : Position(x, y) { }
};

struct GridInstance
{
    DirectX::SimpleMath::Vector2 GridScale;         // grid spacing
    DirectX::SimpleMath::Vector2 TexelScale;        // texel spacing
    DirectX::SimpleMath::Vector2 WorldOffset;       // footprint offset in world
    DirectX::SimpleMath::Vector2 TextureOffset;     // footprint offset in texture
    DirectX::XMUINT2 LocalOffset;                   // footprint offset in level
    uint32_t Color;
    uint32_t Level;

    static constexpr unsigned int InputElementCount = 3;
    static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};
