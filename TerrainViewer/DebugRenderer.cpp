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
        CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R16_UNORM, 256, 256);
        m_HTex[i] = std::make_unique<Texture2D>(device, desc);
        m_HTex[i]->CreateViews(device);
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
    ID3D11ShaderResourceView* srv, const Vector2& position, float scale)
{
    s_Sprite->Begin();
    s_Sprite->Draw(srv, position, nullptr, Colors::White, 0, Vector2::Zero, scale);
    s_Sprite->End();
}

void DebugRenderer::DrawClippedHeight(const Texture2D& hm, ID3D11DeviceContext* context) const
{
    for (int i = 0; i < LevelCount; ++i)
    {
        context->CopySubresourceRegion(m_HTex[i]->GetTexture(),
            D3D11CalcSubresource(0, 0, 0), 0, 0, 0,
            hm.GetTexture(), D3D11CalcSubresource(0, i, 1),
            nullptr);
    }
    std::vector<ID3D11ShaderResourceView*> srvs;
    for (auto&& tex : m_HTex)
        srvs.emplace_back(tex->GetSrv());

    s_Sprite->Begin();
    for (int i = 0; i < LevelCount; ++i)
    {
        Vector2 pos(260 * i, 0);
        s_Sprite->Draw(srvs[i], pos, nullptr, Vector4(1, 1, 1, 0.8), 0, Vector2::Zero, 1.0f);
    }
    s_Sprite->End();
}
