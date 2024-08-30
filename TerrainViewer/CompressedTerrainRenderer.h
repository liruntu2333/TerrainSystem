#pragma once

#include <directxtk/BufferHelpers.h>
#include <filesystem>
#include "Renderer.h"

class CompressedTerrainRenderer final :
    public Renderer
{
public:
    CompressedTerrainRenderer(ID3D11Device* device) : Renderer(device) {}
    void Initialize(ID3D11DeviceContext* context, const std::filesystem::path& shaderDir);
    void Render(ID3D11DeviceContext* context,
                ID3D11ShaderResourceView* origin,
                ID3D11ShaderResourceView* compressed,
                const DirectX::SimpleMath::Matrix& viewProj,
                float ratio,
                float maxError, bool wireFrame);

protected:
    struct Constants
    {
        DirectX::SimpleMath::Matrix ViewProjection;
        float ElevationRatio;
        DirectX::SimpleMath::Vector3 Color;
        float InvMaxError;
        float Pad[3];
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps  = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_Ib       = nullptr;

    DirectX::ConstantBuffer<Constants> m_Cb0 {};
};
