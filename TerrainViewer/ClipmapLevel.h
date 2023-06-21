#pragma once
#include <directxtk/SimpleMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <filesystem>

#include "Vertex.h"

static constexpr int ClipMapN = 255;
static constexpr int ClipMapM = (ClipMapN + 1) / 4;

class ClipmapLevelBase
{
public:
    struct HollowRing
    {
        GridInstance Blocks[12];
        GridInstance RingFixUp;
        GridInstance Trim;
        int TrimId;
    };

    struct SolidSquare
    {
        GridInstance Blocks[16];
        GridInstance RingFixUp;
        GridInstance Trim[2];
        int TrimId[2];
    };

    static void LoadFootprintGeometry(const std::filesystem::path& path, ID3D11Device* device);

    friend class TerrainSystem;

protected:
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> BlockVb = nullptr;
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> BlockIb = nullptr;
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> RingFixUpVb = nullptr;
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> RingFixUpIb = nullptr;
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> TrimVb[4] { nullptr };
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> TrimIb[4] = { nullptr };
    inline static int BlockIdxCnt;
    inline static int RingIdxCnt;
    inline static int TrimIdxCnt;
};

class ClipmapLevel : public ClipmapLevelBase
{
public:
    ClipmapLevel(int l);
    ~ClipmapLevel() = default;

    void Update(const DirectX::SimpleMath::Vector2& dView);
    [[nodiscard]] HollowRing GetHollowRing(float txlScl) const;
    [[nodiscard]] SolidSquare GetSolidSquare(float txlScl) const;

protected:
    const int m_Level;
    const float m_Scale;
    DirectX::SimpleMath::Vector2 m_WorldOffset;
    DirectX::SimpleMath::Vector2 m_TexelOffset;
    DirectX::SimpleMath::Vector2 m_Ticker;
    int m_TrimId = 0;
};
