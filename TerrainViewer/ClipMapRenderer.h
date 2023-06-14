#pragma once

#include <filesystem>
#include <directxtk/BufferHelpers.h>
#include <wrl/client.h>
#include "Renderer.h"

class ClipMapRenderer : public Renderer
{
public:
    ClipMapRenderer(
        ID3D11Device* device, const std::shared_ptr<DirectX::ConstantBuffer<PassConstants>>& passConst);

    ~ClipMapRenderer() override = default;
    ClipMapRenderer(const ClipMapRenderer&) = delete;
    ClipMapRenderer& operator=(const ClipMapRenderer&) = delete;
    ClipMapRenderer(ClipMapRenderer&&) = delete;
    ClipMapRenderer& operator=(ClipMapRenderer&&) = delete;

    void Initialize(ID3D11DeviceContext* context, const std::filesystem::path& path);
    void Render(ID3D11DeviceContext* context);

protected:
    struct ObjectConstants
    {
        DirectX::SimpleMath::Vector4 ScaleFactor;
        DirectX::SimpleMath::Vector4 Color;
    };

    static constexpr int N = 255;
    static constexpr int M = (N + 1) / 4;

    // ((N - 1) / 2 + 1) * ((N - 1) / 2 + 1)
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_CenterVb = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_CenterIb = nullptr;

    // M * M
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_BlockVb = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_BlockIb = nullptr;

    // M * 3 (t + b + l + r)
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_RingVb = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_RingIb = nullptr;

    // L shape (lt, rt, lb, rb)
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_TrimVb[4] { nullptr };
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_TrimIb[4] = { nullptr };

    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout = nullptr;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps = nullptr;

    std::shared_ptr<DirectX::ConstantBuffer<PassConstants>> m_Cb0;
    DirectX::ConstantBuffer<ObjectConstants> m_Cb1;

    int m_CenterIdxCnt = 0;
    int m_BlockIdxCnt = 0;
    int m_RingIdxCnt = 0;
    int m_TrimIdxCnt = 0;
};
