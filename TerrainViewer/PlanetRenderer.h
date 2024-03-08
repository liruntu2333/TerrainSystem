#pragma once

#include <array>
#include <directxtk/BufferHelpers.h>
#include <directxtk/VertexTypes.h>
#include <filesystem>
#include <wrl/client.h>

#include "Renderer.h"

class PlanetRenderer : public Renderer
{
public:
    using Vertex = DirectX::VertexPosition;

    struct Uniforms
    {
        DirectX::SimpleMath::Matrix worldViewProj = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix world         = DirectX::SimpleMath::Matrix::Identity;

        int geometryOctaves = 5;
        int normalOctaves   = 8;
        float lacunarity    = 1.98f;
        float gain          = 0.5f;

        float radius          = 8000.0f;
        float elevation       = 500.0f;
        int interpolateNormal = 0;
        float billowy       = 0.0f;

        DirectX::SimpleMath::Vector3 camPos {};
        float baseAmplitude = 1.0;

        DirectX::SimpleMath::Vector4 debugColor {};
    };

    explicit PlanetRenderer(ID3D11Device* device) : Renderer(device) {}

    ~PlanetRenderer() override = default;

    void Initialize(const std::filesystem::path& shaderDir, int sphereTess);

    void Render(ID3D11DeviceContext* context, Uniforms uniforms, bool wireFrame = false);

private:
    void CreateSphere(uint16_t tesselation);
    void CreateTexture();

    int m_IndicesPerFace = 0;
    std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 6> m_VertexBuffers;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndexBuffer;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_VertexLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps;

    Microsoft::WRL::ComPtr<ID3D11Texture1D> m_AlbedoRoughness;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_AlbedoRoughnessSrv;

    Microsoft::WRL::ComPtr<ID3D11Texture1D> m_F0Metallic;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_F0MetallicSrv;

    DirectX::ConstantBuffer<Uniforms> m_Cb0;
};