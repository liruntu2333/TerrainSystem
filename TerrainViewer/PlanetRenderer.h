#pragma once

#include <directxtk/BufferHelpers.h>
#include <directxtk/VertexTypes.h>
#include <filesystem>

#include "Renderer.h"
#include "Texture2D.h"

class PlanetRenderer final : public Renderer
{
public:
    static constexpr float kRadius       = 50000.0f;
    static constexpr float kElevation    = kRadius * 0.03f;
    static constexpr int kWorldMapWidth  = 512;
    static constexpr int kWorldMapHeight = 256;
    static constexpr int maxInstance     = 128;
    static constexpr int maxBound        = 8;

    struct Uniforms
    {
        DirectX::SimpleMath::Matrix worldViewProj = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix world         = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix worldInvTrans = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix viewProj      = DirectX::SimpleMath::Matrix::Identity;

        DirectX::SimpleMath::Vector4 featureNoiseSeed { 0.0f };
        DirectX::SimpleMath::Vector4 sharpnessNoiseSeed { 0.5f };
        DirectX::SimpleMath::Vector4 slopeErosionNoiseSeed { 0.75f };
        DirectX::SimpleMath::Vector4 perturbNoiseSeed { -0.42f };

        DirectX::SimpleMath::Matrix featureNoiseRotation      = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix sharpnessNoiseRotation    = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix slopeErosionNoiseRotation = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix perturbNoiseRotation      = DirectX::SimpleMath::Matrix::Identity;

        int geometryOctaves = 5;
        float lacunarity    = 2.01f;
        float gain          = 0.5f;
        float radius        = kRadius;

        float elevation       = kElevation;
        float altitudeErosion = 0.0f;
        float ridgeErosion    = 0.0f;
        float baseFrequency   = 1.0f;

        float baseAmplitude = 1.0f;
        int debug           = 0;
        int baseOctaves     = 8u;

        float pad;

        float sharpness[2]           = { -1.0f, 1.0f };
        float sharpnessBaseFrequency = 0.3f;
        float sharpnessLacunarity    = 2.01f;

        float slopeErosion[2]           = { 0.0f, 0.5f };
        float slopeErosionBaseFrequency = 1.0f;
        float slopeErosionLacunarity    = 2.01f;

        float perturb[2]           = { -0.05f, 0.05f };
        float perturbBaseFrequency = 1.0f;
        float perturbLacunarity    = 2.01f;

        DirectX::SimpleMath::Vector3 camPos {};
        float oceanLevel = -3.0f;

        DirectX::SimpleMath::Vector3 camDir {};
        float pad2;

        struct Instance
        {
            DirectX::SimpleMath::Vector2 faceOffset {};
            float gridSize       = 0.0f;
            uint32_t faceOctaves = 0;
        };

        Instance instances[maxInstance];
    };

    struct BoundingUniforms
    {
        DirectX::SimpleMath::Vector4 corners[maxBound + 2][8];
    };

    explicit PlanetRenderer(ID3D11Device* device) : Renderer(device) {}

    ~PlanetRenderer() override = default;

    void Initialize(const std::filesystem::path& shaderDir);

    void Render(
        ID3D11DeviceContext* context,
        Uniforms uniforms,
        const DirectX::BoundingFrustum& frustum,
        const DirectX::SimpleMath::Quaternion& rot,
        const DirectX::SimpleMath::Vector3& trans,
        bool wireFrame = false,
        bool freeze    = false,
        bool bound     = false);

    void CreateWorldMap(ID3D11DeviceContext* context, const Uniforms& uniforms);

    ID3D11ShaderResourceView* GetWorldMapSrv() const { return m_WorldMap->GetSrv(); }

private:
    void CreateSphere(uint16_t tesselation);
    void CreateTexture();
    void RenderOcean(ID3D11DeviceContext* context, Uniforms& uniforms);

    int m_Tesselation    = 0;
    int m_IndicesPerFace = 0;

    DirectX::ConstantBuffer<Uniforms> m_Cb0;
    DirectX::ConstantBuffer<BoundingUniforms> m_Cb1;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_TileIb;
    // Microsoft::WRL::ComPtr<ID3D11InputLayout> m_VertexLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_PlanetVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PlanetPs;
    Microsoft::WRL::ComPtr<ID3D11Texture1D> m_AlbedoRoughness;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_AlbedoRoughnessSrv;
    Microsoft::WRL::ComPtr<ID3D11Texture1D> m_F0Metallic;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_F0MetallicSrv;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_OceanVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_OceanPs;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_WorldMapCs;
    std::unique_ptr<DirectX::Texture2D> m_WorldMap;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_BoundVb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_BoundIb;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_BoundVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_BoundPs;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_VertPosLayout;
};
