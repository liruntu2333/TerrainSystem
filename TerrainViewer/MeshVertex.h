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
    DirectX::SimpleMath::Vector2 Position {};

    static constexpr unsigned int InputElementCount = 1;
    static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];

    GridVertex(const DirectX::SimpleMath::Vector2& Position)
        : Position(Position) { }

    GridVertex();
};
