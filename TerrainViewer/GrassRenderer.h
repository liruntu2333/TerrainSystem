#pragma once
#include "Renderer.h"
#include <filesystem>
#include <directxtk/BufferHelpers.h>
#include <directxtk/VertexTypes.h>

#include "StructuredBuffer.h"

class GrassRenderer : public Renderer
{
public:
    using BaseVertex = DirectX::VertexPositionNormalTexture;
    explicit GrassRenderer(ID3D11Device* device);
    ~GrassRenderer() override = default;

    GrassRenderer(const GrassRenderer&)            = delete;
    GrassRenderer& operator=(const GrassRenderer&) = delete;
    GrassRenderer(GrassRenderer&&)                 = delete;
    GrassRenderer& operator=(GrassRenderer&&)      = delete;

    void Initialize(const std::filesystem::path& shaderDir);
    void Render(
        ID3D11DeviceContext* context,
        const DirectX::StructuredBuffer<DirectX::VertexPositionNormalTexture>& baseVb,
        const DirectX::StructuredBuffer<unsigned>& baseIb,
        ID3D11ShaderResourceView* grassAlbedo,
        const DirectX::SimpleMath::Matrix& baseRotTrans,
        const DirectX::SimpleMath::Matrix& viewProj,
        const float baseArea, float density, bool wireFrame);

protected:
    struct InstanceData
    {
        DirectX::SimpleMath::Vector3 Pos;
        DirectX::SimpleMath::Vector3 V1;
        DirectX::SimpleMath::Vector3 V2;
        float MaxWidth;
        uint32_t Hash;
    };

    struct Uniforms
    {
        DirectX::SimpleMath::Matrix ViewProj;
        DirectX::SimpleMath::Matrix BaseWorld;
        DirectX::XMFLOAT3X3 InvTransBaseWorld;
        uint32_t GrassIdxCnt;
        uint32_t NumBaseTriangle;
        float InvSumBaseArea;
        float Density;
        float Padding[3];
    };

    DirectX::ConstantBuffer<Uniforms> m_Cb0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndirectDrawArg          = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_HighLodIb                = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_DrawArgUav  = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_InstanceBuffer           = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_InstanceUav = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_InstanceSrv  = nullptr;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_Cs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps;
};
