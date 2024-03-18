#pragma once

#include <directxtk/BufferHelpers.h>
#include <directxtk/VertexTypes.h>
#include <filesystem>

#include "Renderer.h"
#include "Texture2D.h"

class PlanetRenderer : public Renderer
{
public:
    using Vertex = DirectX::VertexPosition;
    static constexpr float kRadius       = 173710.0;
    static constexpr float kElevation    = kRadius * 0.025f;
    static constexpr int kWorldMapWidth  = 512;
    static constexpr int kWorldMapHeight = 256;

    struct Uniforms
    {
        DirectX::SimpleMath::Vector3 faceUp {};
        float gridSize = 0.0f;
        DirectX::SimpleMath::Vector3 faceRight {};
        float gridOffsetU = 0.0f;
        DirectX::SimpleMath::Vector3 faceBottom {};
        float gridOffsetV = 0.0f;

        DirectX::SimpleMath::Matrix worldViewProj = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix world         = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix worldInvTrans = DirectX::SimpleMath::Matrix::Identity;

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
        float gain          = 0.6f;
        float radius        = kRadius;

        float elevation       = kElevation;
        float altitudeErosion = 0.0f;
        float ridgeErosion    = 0.0f;
        float baseFrequency   = 1.0f;

        float baseAmplitude = 1.0f;
        float pad[3];

        float sharpness[2]           = { -1.0f, 1.0f };
        float sharpnessBaseFrequency = 1.0;
        float sharpnessLacunarity    = 2.01f;

        float slopeErosion[2]           = { 0.0f, 1.0f };
        float slopeErosionBaseFrequency = 1.0f;
        float slopeErosionLacunarity    = 2.01f;

        float perturb[2]           = { 0.0f, 0.0f };
        float perturbBaseFrequency = 1.0f;
        float perturbLacunarity    = 2.01f;

        DirectX::SimpleMath::Vector3 camPos {};
        float oceanLevel = -3.0f;

        DirectX::SimpleMath::Vector4 debugColor {};
    };

    explicit PlanetRenderer(ID3D11Device* device) : Renderer(device) {}

    ~PlanetRenderer() override = default;

    void Initialize(const std::filesystem::path& shaderDir, int sphereTess);

    void Render(ID3D11DeviceContext* context, Uniforms uniforms, bool wireFrame = false);

    void CreateWorldMap(ID3D11DeviceContext* context, const Uniforms& uniforms);

    ID3D11ShaderResourceView* GetWorldMapSrv() const { return m_WorldMap->GetSrv(); }

private:
    void CreateSphere(uint16_t tesselation);
    void CreateTexture();

    int m_Tesselation    = 0;
    int m_IndicesPerFace = 0;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndexBuffer;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_VertexLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_PlanetVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PlanetPs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_OceanVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_OceanPs;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_WorldMapCs;

    Microsoft::WRL::ComPtr<ID3D11Texture1D> m_AlbedoRoughness;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_AlbedoRoughnessSrv;

    Microsoft::WRL::ComPtr<ID3D11Texture1D> m_F0Metallic;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_F0MetallicSrv;

    DirectX::ConstantBuffer<Uniforms> m_Cb0;

    std::unique_ptr<DirectX::Texture2D> m_WorldMap;
};
