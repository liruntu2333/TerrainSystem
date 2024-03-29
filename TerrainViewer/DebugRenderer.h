#pragma once
#include <directxtk/GeometricPrimitive.h>
#include <directxtk/SpriteBatch.h>
#include "directxtk/SimpleMath.h"
#include "Texture2D.h"

class DebugRenderer
{
public:
    DebugRenderer(ID3D11DeviceContext* context, ID3D11Device* device);
    ~DebugRenderer() = default;

    static void DrawBounding(
        const std::vector<DirectX::BoundingBox>& bbs,
        const DirectX::SimpleMath::Matrix& view,
        const DirectX::SimpleMath::Matrix& proj);
    static void DrawBounding(
        const std::vector<DirectX::BoundingSphere>& bss,
        const DirectX::SimpleMath::Matrix& view,
        const DirectX::SimpleMath::Matrix& proj);
    static void DrawSprite(
        ID3D11ShaderResourceView* srv,
        const DirectX::SimpleMath::Vector2& position,
        float scale = 1.0f);

    void DrawClippedR16(
        DirectX::SimpleMath::Vector2 origin, const DirectX::Texture2D& tex, ID3D11DeviceContext* context) const;

    void DrawClippedRGBA8888(
        DirectX::SimpleMath::Vector2 origin, const DirectX::Texture2D& tex, ID3D11DeviceContext* context) const;

    void DrawClippedBc3(
        DirectX::SimpleMath::Vector2 origin, const DirectX::Texture2D& tex, ID3D11DeviceContext* context) const;

protected:
    inline static std::unique_ptr<DirectX::GeometricPrimitive> s_Cube = nullptr;
    inline static std::unique_ptr<DirectX::GeometricPrimitive> s_Sphere = nullptr;
    inline static std::unique_ptr<DirectX::SpriteBatch> s_Sprite = nullptr;
    static constexpr int LevelCount = 5;

    std::unique_ptr<DirectX::Texture2D> m_HTex[LevelCount] = { nullptr };
    std::unique_ptr<DirectX::Texture2D> m_Rgba8887Tex[LevelCount] = { nullptr };
    std::unique_ptr<DirectX::Texture2D> m_Bc3Tex[LevelCount] = { nullptr };
};
