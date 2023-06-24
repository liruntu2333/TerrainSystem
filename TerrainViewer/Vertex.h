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
    DirectX::SimpleMath::Vector2 ScaleFactor; // x : grid spacing     y : texture size
    DirectX::SimpleMath::Vector2 OffsetInLevel; // footprint offset in level
    DirectX::SimpleMath::Vector2 OffsetInWorld; // footprint offset in world
    DirectX::SimpleMath::Vector2 TextureOffset;
    uint32_t Color;
    uint32_t Level;

    static constexpr unsigned int InputElementCount = 3;
    static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};
