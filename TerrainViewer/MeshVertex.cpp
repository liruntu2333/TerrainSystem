#include "MeshVertex.h"

const D3D11_INPUT_ELEMENT_DESC MeshVertex::InputElements[InputElementCount] =
{
    {
        "SV_Position",
        0,
        DXGI_FORMAT_R8G8_UNORM,
        0,
        0,
        D3D11_INPUT_PER_VERTEX_DATA,
        0
    }
};

const D3D11_INPUT_ELEMENT_DESC GridVertex::InputElements[InputElementCount] =
{
    {
        "SV_Position",
        0,
        DXGI_FORMAT_R32G32_FLOAT,
        0,
        0,
        D3D11_INPUT_PER_VERTEX_DATA,
        0
    }
};