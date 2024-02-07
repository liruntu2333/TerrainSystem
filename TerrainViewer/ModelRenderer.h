#pragma once
#include <filesystem>
#include <directxtk/BufferHelpers.h>
#include <directxtk/VertexTypes.h>

#include "Renderer.h"
#include "StructuredBuffer.h"
#include "Texture2D.h"

class ModelRenderer : public Renderer
{
public:
    using Vertex = DirectX::VertexPositionNormalTexture;

    struct Uniforms
    {
        DirectX::SimpleMath::Matrix WorldViewProj;
        DirectX::SimpleMath::Matrix ViewProj;
        DirectX::SimpleMath::Matrix World;
        DirectX::SimpleMath::Vector3 CamPos;
        float Padding;
    };

    explicit ModelRenderer(ID3D11Device* device);
    ~ModelRenderer() override = default;

    ModelRenderer(const ModelRenderer&)            = delete;
    ModelRenderer& operator=(const ModelRenderer&) = delete;
    ModelRenderer(ModelRenderer&&)                 = delete;
    ModelRenderer& operator=(ModelRenderer&&)      = delete;

    void Initialize(const std::filesystem::path& shaderDir);
    void Render(
        ID3D11DeviceContext* context,
        const DirectX::SimpleMath::Matrix& world,
        const DirectX::SimpleMath::Matrix& viewProj,
        const DirectX::StructuredBuffer<Vertex>& vb,
        const DirectX::StructuredBuffer<unsigned>& ib,
        const DirectX::Texture2D& albedo, 
        const DirectX::SimpleMath::Vector3& camPos, 
        bool wireFrame = false);

private:
    DirectX::ConstantBuffer<Uniforms> m_Cb0 {};

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps  = nullptr;
};
