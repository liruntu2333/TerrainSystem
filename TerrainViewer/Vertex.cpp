#include "Vertex.h"

const D3D11_INPUT_ELEMENT_DESC MeshVertex::InputElements[InputElementCount] =
{
    {
        "SV_Position",
        0,
        DXGI_FORMAT_R8G8_UNORM,
        0,
        D3D11_APPEND_ALIGNED_ELEMENT,
        D3D11_INPUT_PER_VERTEX_DATA,
        0
    }
};

const D3D11_INPUT_ELEMENT_DESC GridVertex::InputElements[InputElementCount] =
{
    {
        "SV_Position",
        0,
        DXGI_FORMAT_R8G8_UINT,
        0,
        D3D11_APPEND_ALIGNED_ELEMENT,
        D3D11_INPUT_PER_VERTEX_DATA,
        0
    }
};

const D3D11_INPUT_ELEMENT_DESC GridInstance::InputElements[InputElementCount] =
{
    {
        "TEXCOORD",
        0,
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        1,
        0,
        D3D11_INPUT_PER_INSTANCE_DATA,
        1
    },
    {
        "TEXCOORD",
        1,
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        1,
        16,
        D3D11_INPUT_PER_INSTANCE_DATA,
        1
    },
    {
        "TEXCOORD",
        2,
        DXGI_FORMAT_R32G32B32A32_UINT,
        1,
        32,
        D3D11_INPUT_PER_INSTANCE_DATA,
        1
    }
};
