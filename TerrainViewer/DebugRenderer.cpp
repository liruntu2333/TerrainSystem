#include "DebugRenderer.h"
using namespace DirectX;
using namespace SimpleMath;

DebugRenderer::DebugRenderer(ID3D11DeviceContext* context, ID3D11Device* device)
{
    if (s_Cube == nullptr)
        s_Cube = GeometricPrimitive::CreateCube(context);
    if (s_Sphere == nullptr)
        s_Sphere = GeometricPrimitive::CreateSphere(context);
    if (s_Sprite == nullptr)
        s_Sprite = std::make_unique<SpriteBatch>(context);

    for (int i = 0; i < LevelCount; ++i)
    {
        m_HTex[i] = std::make_unique<Texture2D>(device,
            CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16_UNORM, 256, 256));
        m_HTex[i]->CreateViews(device);

        m_Rgba8887Tex[i] = std::make_unique<Texture2D>(device,
            CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, 256, 256));
        m_Rgba8887Tex[i]->CreateViews(device);

        m_Bc3Tex[i] = std::make_unique<Texture2D>(device,
            CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_BC3_UNORM, 256, 256));
        m_Bc3Tex[i]->CreateViews(device);
    }
}

void DebugRenderer::DrawBounding(
    const std::vector<BoundingBox>& bbs, const Matrix& view, const Matrix& proj)
{
    for (auto&& bb : bbs)
        s_Cube->Draw(Matrix::CreateScale(bb.Extents * 2) * Matrix::CreateTranslation(bb.Center),
            view, proj);
}

void DebugRenderer::DrawBounding(
    const std::vector<BoundingSphere>& bss, const Matrix& view, const Matrix& proj)
{
    for (auto&& bs : bss)
        s_Sphere->Draw(Matrix::CreateScale(bs.Radius) * Matrix::CreateTranslation(bs.Center),
            view, proj);
}

void DebugRenderer::DrawSprite(
    ID3D11ShaderResourceView* srv, const Vector2& position, const float scale)
{
    s_Sprite->Begin();
    s_Sprite->Draw(srv, position, nullptr, Colors::White, 0, Vector2::Zero, scale);
    s_Sprite->End();
}

void DebugRenderer::DrawClippedR16(
    const Vector2 origin, const Texture2D& tex, ID3D11DeviceContext* context) const
{
    for (int i = 0; i < LevelCount; ++i)
    {
        D3D11_BOX box = { 0, 0, 0, 256, 256, 1 };
        context->CopySubresourceRegion(m_HTex[i]->GetTexture(),
            D3D11CalcSubresource(0, 0, 0), 0, 0, 0,
            tex.GetTexture(), D3D11CalcSubresource(0, i, tex.GetDesc().MipLevels),
            &box);
    }
    std::vector<ID3D11ShaderResourceView*> srvs;
    for (auto&& t : m_HTex) srvs.emplace_back(t->GetSrv());

    s_Sprite->Begin();
    for (int i = 0; i < LevelCount; ++i)
    {
        auto pos = origin + Vector2(260 * i, 0);
        s_Sprite->Draw(srvs[i], pos, nullptr, Colors::Red, 0, Vector2::Zero, 1.0f);
    }
    s_Sprite->End();
}

void DebugRenderer::DrawClippedRGBA8888(
    const Vector2 origin, const Texture2D& tex, ID3D11DeviceContext* context) const
{
    for (int i = 0; i < LevelCount; ++i)
    {
        D3D11_BOX box = { 896, 896, 0, 1152, 1152, 1 };
        context->CopySubresourceRegion(m_Rgba8887Tex[i]->GetTexture(),
            D3D11CalcSubresource(0, 0, 0), 0, 0, 0,
            tex.GetTexture(), D3D11CalcSubresource(0, i, tex.GetDesc().MipLevels),
            &box);
    }
    std::vector<ID3D11ShaderResourceView*> srvs;
    for (auto&& t : m_Rgba8887Tex) srvs.emplace_back(t->GetSrv());

    s_Sprite->Begin();
    for (int i = 0; i < LevelCount; ++i)
    {
        auto pos = origin + Vector2(260 * i, 0);
        RECT r = { 0, 0, 256, 256 };
        s_Sprite->Draw(srvs[i], pos, &r);
    }
    s_Sprite->End();
}

void DebugRenderer::DrawClippedBc3(
    const Vector2 origin, const Texture2D& tex, ID3D11DeviceContext* context) const
{
    for (int i = 0; i < LevelCount; ++i)
    {
        D3D11_BOX box = { 896, 896, 0, 1152, 1152, 1 };
        context->CopySubresourceRegion(m_Bc3Tex[i]->GetTexture(),
            D3D11CalcSubresource(0, 0, 0), 0, 0, 0,
            tex.GetTexture(), D3D11CalcSubresource(0, i, tex.GetDesc().MipLevels),
            &box);
    }
    std::vector<ID3D11ShaderResourceView*> srvs;
    for (auto&& t : m_Bc3Tex) srvs.emplace_back(t->GetSrv());

    s_Sprite->Begin();
    for (int i = 0; i < LevelCount; ++i)
    {
        auto pos = origin + Vector2(260 * i, 0);
        RECT r = { 0, 0, 256, 256 };
        s_Sprite->Draw(srvs[i], pos, &r);
    }
    s_Sprite->End();
}
