#pragma once
#include "Renderer.h"
#include <filesystem>
#include <directxtk/BufferHelpers.h>
#include <directxtk/VertexTypes.h>

#include "StructuredBuffer.h"

class GrassRenderer : public Renderer
{
public:
    struct Uniforms
    {
        DirectX::SimpleMath::Matrix ViewProj;
        DirectX::SimpleMath::Matrix BaseWorld;
        DirectX::SimpleMath::Matrix VPForCull;
        uint32_t NumBaseTriangle;
        float Density;
        DirectX::SimpleMath::Vector2 Height;
        DirectX::SimpleMath::Vector2 Width;
        DirectX::SimpleMath::Vector2 BendFactor;
        DirectX::SimpleMath::Vector4 Gravity;
        DirectX::SimpleMath::Vector4 Wind;
        float WindWave;
        DirectX::SimpleMath::Vector2 NearFar;
        int DistCulling;
        int OrientCulling;
        int FrustumCulling;
        int OcclusionCulling;
        int Debug;
        DirectX::SimpleMath::Vector3 CamPos;
        float OrientThreshold;
        //DirectX::SimpleMath::Vector4 Planes[6];
        float Lod0Dist;
        int pad[3];
    };

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
        ID3D11ShaderResourceView* depth,
        const Uniforms& uniforms,
        bool wireFrame);

protected:
    struct InstanceData
    {
        DirectX::SimpleMath::Vector3 Pos;
        DirectX::SimpleMath::Vector3 PosV1;
        DirectX::SimpleMath::Vector3 PosV2;
        DirectX::SimpleMath::Vector3 BladeDir;
        float MaxWidth;
        uint32_t Hash;
    };

    DirectX::ConstantBuffer<Uniforms> m_Cb0;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_SamIdx                 = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_SamIdxUav = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SamIdxSrv  = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndirectDispArg        = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_Lod0Inst             = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_Lod0Uav = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_Lod0Srv  = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_Lod1Inst             = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_Lod1Uav = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_Lod1Srv  = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndirectArg         = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_ArgUav = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_HighLodIb = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_GenGrassCs;
    //Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_AssignSamCs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps;
};