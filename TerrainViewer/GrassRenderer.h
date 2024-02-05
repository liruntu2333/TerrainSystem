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
        const DirectX::SimpleMath::Matrix& baseWorld,
        const DirectX::SimpleMath::Matrix& viewProj,
        const float baseArea, float density, bool wireFrame);

protected:
    struct InstanceData
    {
        DirectX::SimpleMath::Vector3 Pos;
        DirectX::SimpleMath::Vector3 PosV1;
        DirectX::SimpleMath::Vector3 PosV2;
        float MaxWidth;
        uint32_t Hash;
    };

    struct Uniforms
    {
        DirectX::SimpleMath::Matrix ViewProj;
        DirectX::SimpleMath::Matrix BaseWorld;
        uint32_t NumBaseTriangle;
        float Density;
        float Pad[2];
    };

    DirectX::ConstantBuffer<Uniforms> m_Cb0;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_SamIdx                 = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_SamIdxUav = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SamIdxSrv  = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndirectDispArg        = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_InstData             = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_InstUav = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_InstSrv  = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndirectArg         = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_ArgUav = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_HighLodIb = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_GenGrassCs;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_AssignSamCs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps;
};
