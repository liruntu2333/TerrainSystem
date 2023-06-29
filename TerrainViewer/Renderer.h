#pragma once

#include <directxtk/CommonStates.h>
#include <directxtk/SimpleMath.h>

struct PassConstants
{
    DirectX::SimpleMath::Matrix ViewProjection;
    DirectX::SimpleMath::Vector4 Light;
    DirectX::SimpleMath::Vector3 ViewPosition;
    float OneOverTransition;
    DirectX::SimpleMath::Vector2 AlphaOffset;
    float HeightScale;
    float pad;
};

class Renderer
{
public:
    Renderer(ID3D11Device* device);
    virtual ~Renderer() = default;

    static std::unique_ptr<DirectX::CommonStates> s_CommonStates;

protected:
    ID3D11Device* m_Device = nullptr;
};
