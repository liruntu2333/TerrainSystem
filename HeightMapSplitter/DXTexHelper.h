#pragma once
#include <DirectXTex.h>
#include <filesystem>
#include <memory>

void TIF(HRESULT hr);
std::shared_ptr<DirectX::ScratchImage> LoadDds(const std::filesystem::path& path);
std::shared_ptr<DirectX::ScratchImage> LoadWic(const std::filesystem::path& path, 
    bool rgba8888 = true, bool generateMip = true);
std::shared_ptr<DirectX::ScratchImage> LoadTga(const std::filesystem::path& path);
std::shared_ptr<DirectX::ScratchImage> GenerateMip(
    const DirectX::Image& src, DirectX::TEX_FILTER_FLAGS flag = DirectX::TEX_FILTER_DEFAULT);
void SaveDds(const std::filesystem::path& path, const DirectX::ScratchImage& img);
void SavePng(const std::filesystem::path& path, const DirectX::ScratchImage& img);

void TIF(HRESULT hr)
{
    if (FAILED(hr))
        throw std::exception();
}

inline std::shared_ptr<DirectX::ScratchImage> LoadDds(const std::filesystem::path& path)
{
    auto tex = std::make_shared<DirectX::ScratchImage>();
    TIF(LoadFromDDSFile(path.wstring().c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *tex));
    return tex;
}

inline std::shared_ptr<DirectX::ScratchImage> LoadWic(const std::filesystem::path& path, bool rgba8888, bool generateMip)
{
    auto tex = std::make_shared<DirectX::ScratchImage>();
    TIF(LoadFromWICFile(path.wstring().c_str(), DirectX::WIC_FLAGS_FORCE_RGB, nullptr, *tex));
    if (rgba8888 && tex->GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        DirectX::ScratchImage conv;
        TIF(Convert(*tex->GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT,
            DirectX::TEX_THRESHOLD_DEFAULT,
            conv));
        tex = std::make_shared<DirectX::ScratchImage>(std::move(conv));
    }
    if (generateMip)
        tex = GenerateMip(*tex->GetImage(0, 0, 0));
    return tex;
}

inline std::shared_ptr<DirectX::ScratchImage> LoadTga(const std::filesystem::path& path)
{
    auto tex = std::make_shared<DirectX::ScratchImage>();
    TIF(LoadFromTGAFile(path.wstring().c_str(), DirectX::TGA_FLAGS_NONE, nullptr, *tex));
    return tex;
}

inline std::shared_ptr<DirectX::ScratchImage> GenerateMip(const DirectX::Image& src, DirectX::TEX_FILTER_FLAGS flag)
{
    auto mip = std::make_shared<DirectX::ScratchImage>();
    TIF(GenerateMipMaps(src, flag, 0, *mip));
    return mip;
}

inline void SaveDds(const std::filesystem::path& path, const DirectX::ScratchImage& img)
{
    TIF(SaveToDDSFile(img.GetImages(), img.GetImageCount(), img.GetMetadata(), DirectX::DDS_FLAGS_NONE,
        path.wstring().c_str()));
}

inline void SavePng(const std::filesystem::path& path, const DirectX::ScratchImage& img)
{
    TIF(SaveToWICFile(*img.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE, GetWICCodec(DirectX::WIC_CODEC_PNG),
        path.wstring().c_str()));
}

inline auto GetLuminance(const uint16_t r, const uint16_t g, const uint16_t b)
{
    return static_cast<uint16_t>(0.2126f * r + 0.7152f * g + 0.0722f * b);
};
