#pragma once
#include <algorithm>
#include <fstream>

#include "DirectXTex.h"

class WaveletTransform
{
public:
    template <typename T, typename U>
    static U SaturatedCast(T val)
    {
        if (val > std::numeric_limits<U>::max())
        {
            return std::numeric_limits<U>::max();
        }
        if (val < std::numeric_limits<U>::min())
        {
            return std::numeric_limits<U>::min();
        }
        return static_cast<U>(val);
    }

    template <typename U>
    struct UpperSigned
    {
        static_assert(std::is_arithmetic_v<U>, "U must be an integral type");
    };

    template <>
    struct UpperSigned<uint8_t>
    {
        using Type                          = int16_t;
        static constexpr DXGI_FORMAT Format = DXGI_FORMAT_R16_SINT;
    };

    template <>
    struct UpperSigned<uint16_t>
    {
        using Type                          = int32_t;
        static constexpr DXGI_FORMAT Format = DXGI_FORMAT_R32_SINT;
    };

    template <>
    struct UpperSigned<float>
    {
        using Type                          = float;
        static constexpr DXGI_FORMAT Format = DXGI_FORMAT_R32_FLOAT;
    };

    template <typename U>
    struct LowerUnsigned
    {
        static_assert(std::is_arithmetic_v<U>, "U must be an integral type");
    };

    template <>
    struct LowerUnsigned<int16_t>
    {
        using Type                          = uint8_t;
        static constexpr DXGI_FORMAT Format = DXGI_FORMAT_R8_UNORM;
    };

    template <>
    struct LowerUnsigned<int>
    {
        using Type                          = uint16_t;
        static constexpr DXGI_FORMAT Format = DXGI_FORMAT_R16_SINT;
    };

    template <>
    struct LowerUnsigned<float>
    {
        using Type                          = float;
        static constexpr DXGI_FORMAT Format = DXGI_FORMAT_R32_FLOAT;
    };

    template <typename T>
    struct Image
    {
        int w, h;
        std::vector<T> data;

        Image(const int w, const int h) : w(w), h(h), data(w * h) {}

        T& At(int y, int x)
        {
            // x = std::clamp(x, 0, w - 1);
            // y = std::clamp(y, 0, h - 1);

            return data[y * w + x];
        }
    };

    template <typename T>
    static std::vector<std::vector<T>> InterleaveSubBand(const T* src, size_t width, size_t height, int iteration);

    template <typename T>
    static void Haar(const DirectX::Image& src, DirectX::ScratchImage& dst, int iteration);
    template <typename T>
    static void InvHaar(const DirectX::Image& src, DirectX::ScratchImage& dst, int iteration);

    template <typename T>
    static void LeGall53(const DirectX::Image& src, DirectX::ScratchImage& dst, int iteration, T avg);
    template <typename T>
    static void InvLeGall53(const DirectX::Image& src, DirectX::ScratchImage& dst, int iteration, T avg);

    template <typename T>
    static float FilterOut(const T* src, T* dst, size_t size, T threshold);
};

template <typename T>
std::vector<std::vector<T>> WaveletTransform::InterleaveSubBand(const T* src, size_t width, size_t height, int iteration)
{
    std::vector<std::vector<T>> subbands(iteration * 3 + 1);

    {
        auto& subband = subbands[0];
        subband.reserve((width >> iteration) * (height >> iteration));
        for (int y = 0; y < height >> iteration; ++y)
        {
            for (int x = 0; x < width >> iteration; ++x)
            {
                subband.emplace_back(src[y * width + x]);
            }
        }
    }

    for (int i = iteration - 1; i >= 0; --i)
    {
        int currWidth  = width >> (i + 1);
        int currHeight = height >> (i + 1);

        {
            auto& subband = subbands[3 * (iteration - 1 - i) + 1];
            subband.reserve(currWidth * currHeight);
            for (int y = 0; y < currHeight; ++y)
            {
                for (int x = currWidth; x < currWidth + currWidth; ++x)
                {
                    subband.emplace_back(src[y * width + x]);
                }
            }
        }

        {
            auto& subband = subbands[3 * (iteration - 1 - i) + 2];
            subband.reserve(currWidth * currHeight);
            for (int y = currHeight; y < currHeight + currHeight; ++y)
            {
                for (int x = 0; x < currWidth; ++x)
                {
                    subband.emplace_back(reinterpret_cast<const T*>(src)[y * width + x]);
                }
            }
        }

        {
            auto& subband = subbands[3 * (iteration - 1 - i) + 3];
            subband.reserve(currWidth * currHeight);
            for (int y = currHeight; y < currHeight + currHeight; ++y)
            {
                for (int x = currWidth; x < currWidth + currWidth; ++x)
                {
                    subband.emplace_back(src[y * width + x]);
                }
            }
        }
    }
    return subbands;
}

template <typename T>
void WaveletTransform::Haar(const DirectX::Image& src, DirectX::ScratchImage& dst, int iteration)
{
    int width  = src.width;
    int height = src.height;
    assert(src.slicePitch / (width * height) == sizeof(T));
    const auto* srcData = reinterpret_cast<const T*>(src.pixels);
    using U             = typename UpperSigned<T>::Type;
    Image<U> workspace(width, height);

    std::transform(srcData, srcData + width * height, workspace.data.begin(), [](const T& v) { return static_cast<U>(v); });

    for (int k = 0; k < iteration; k++)
    {
        for (int y = 0; y < height >> k; y++)
        {
            const int currWidth = width >> (k + 1);
            std::vector<U> row(currWidth);
            for (int x = 0; x < currWidth; x++)
            {
                U x0 = static_cast<U>(workspace.At(y, x * 2));
                U x1 = static_cast<U>(workspace.At(y, x * 2 + 1));
                U c  = (x0 + x1);
                U d  = (x0 - x1);

                workspace.At(y, x) = c;
                row[x]             = d;
            }
            for (int x = 0; x < currWidth; ++x)
            {
                workspace.At(y, x + currWidth) = row[x];
            }
        }

        for (int x = 0; x < width >> k; ++x)
        {
            int currHeight = height >> (k + 1);
            std::vector<U> col(currHeight);
            for (int y = 0; y < currHeight; ++y)
            {
                auto x0 = static_cast<U>(workspace.At(y * 2, x));
                auto x1 = static_cast<U>(workspace.At(y * 2 + 1, x));
                U c     = (x0 + x1);
                U d     = (x0 - x1);

                workspace.At(y, x) = c;
                col[y]             = d;
            }
            for (int y = 0; y < currHeight; ++y)
            {
                workspace.At(y + currHeight, x) = col[y];
            }
        }
    }

    dst.Initialize2D(UpperSigned<T>::Format, width, height, 1, 1);
    std::memcpy(dst.GetImages()[0].pixels, workspace.data.data(), workspace.data.size() * sizeof(U));
}

template <typename T>
void WaveletTransform::InvHaar(const DirectX::Image& src, DirectX::ScratchImage& dst, int iteration)
{
    int width  = src.width;
    int height = src.height;
    assert(src.slicePitch / (width * height) == sizeof(T));
    const auto* srcData = reinterpret_cast<const T*>(src.pixels);
    using U             = typename LowerUnsigned<T>::Type;
    Image<T> workspace(width, height);

    std::memcpy(workspace.data.data(), srcData, width * height * sizeof(T));

    for (int k = iteration - 1; k >= 0; k--)
    {
        for (int y = 0; y < height >> k; y++)
        {
            int currWidth = width >> (k + 1);
            std::vector<T> row(width >> k);
            for (int x = 0; x < currWidth; x++)
            {
                T c  = workspace.At(y, x);
                T d  = workspace.At(y, x + currWidth);
                T x1 = (c + d) >> 1;
                T x2 = (c - d) >> 1;

                row[x * 2]     = x1;
                row[x * 2 + 1] = x2;
            }
            for (int x = 0; x < width >> k; ++x)
            {
                workspace.At(y, x) = row[x];
            }
        }

        for (int x = 0; x < width >> k; x++)
        {
            int currHeight = height >> (k + 1);
            std::vector<T> col(height >> k);
            for (int y = 0; y < currHeight; y++)
            {
                T c  = workspace.At(y, x);
                T d  = workspace.At(y + currHeight, x);
                T x1 = (c + d) >> 1;
                T x2 = (c - d) >> 1;

                col[y * 2]     = x1;
                col[y * 2 + 1] = x2;
            }
            for (int y = 0; y < height >> k; ++y)
            {
                workspace.At(y, x) = col[y];
            }
        }
    }

    dst.Initialize2D(LowerUnsigned<T>::Format, width, height, 1, 1);
    U* pixels = dst.GetImages()[0].pixels;
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            pixels[i * width + j] = static_cast<U>(workspace.At(i, j));
        }
    }
}

template <typename T>
void WaveletTransform::LeGall53(const DirectX::Image& src, DirectX::ScratchImage& dst, int iteration, T avg)
{
    int width  = src.width;
    int height = src.height;
    assert(src.slicePitch / (width * height) == sizeof(T));
    const auto* srcData = reinterpret_cast<const T*>(src.pixels);
    using U             = typename UpperSigned<T>::Type;
    Image<U> workspace(width, height);

    std::transform(srcData, srcData + width * height, workspace.data.begin(), [&avg](const T& v)
    {
        return static_cast<U>(v) - avg;
    });

    for (int k = 0; k < iteration; k++)
    {
        for (int y = 0; y < height >> k; y++)
        {
            const int half = width >> (k + 1);
            std::vector<U> row(half);
            for (int n = 0; n < half; n++)
            {
                U x0 = static_cast<U>(workspace.At(y, n * 2));
                U x1 = static_cast<U>(workspace.At(y, n * 2 + 1));
                U x2 = static_cast<U>(workspace.At(y, std::min((width >> k) - 2, n * 2 + 2)));
                U c  = x1 - ((x0 + x2) >> 1);

                row[n] = c;
            }
            for (int n = 0; n < half; ++n)
            {
                U x0 = workspace.At(y, n * 2);
                U c1 = row[n];
                U c0 = row[std::max(0, n - 1)];
                U d  = x0 + ((c0 + c1 + 2) >> 2);

                workspace.At(y, n) = d;
            }
            for (int n = 0; n < half; ++n)
            {
                U c1 = row[n];

                workspace.At(y, n + half) = c1;
            }
        }

        for (int x = 0; x < width >> k; ++x)
        {
            int half = height >> (k + 1);
            std::vector<U> col(half);
            for (int n = 0; n < half; ++n)
            {
                U x0 = static_cast<U>(workspace.At(n * 2, x));
                U x1 = static_cast<U>(workspace.At(n * 2 + 1, x));
                U x2 = static_cast<U>(workspace.At(std::min((height >> k) - 2, n * 2 + 2), x));
                U c  = x1 - ((x0 + x2) >> 1);

                col[n] = c;
            }
            for (int n = 0; n < half; ++n)
            {
                U x0 = workspace.At(n * 2, x);
                U c1 = col[n];
                U c0 = col[std::max(0, n - 1)];
                U d  = x0 + ((c0 + c1 + 2) >> 2);

                workspace.At(n, x) = d;
            }
            for (int n = 0; n < half; ++n)
            {
                U c1 = col[n];

                workspace.At(n + half, x) = c1;
            }
        }
    }

    // std::ifstream ifs("I:/TerrainSystem/coeff.txt");
    // for (int i = 0; i < width * height; ++i)
    // {
    //     int val;
    //     ifs >> val;
    //     U recon = workspace.data[i];
    //     if (recon != val)
    //     {
    //         return;
    //     }
    // }

    dst.Initialize2D(UpperSigned<T>::Format, width, height, 1, 1);
    std::memcpy(dst.GetImages()[0].pixels, workspace.data.data(), workspace.data.size() * sizeof(U));
}

template <typename T>
void WaveletTransform::InvLeGall53(const DirectX::Image& src, DirectX::ScratchImage& dst, int iteration, T avg)
{
    int width  = src.width;
    int height = src.height;
    assert(src.slicePitch / (width * height) == sizeof(T));
    const auto* srcData = reinterpret_cast<const T*>(src.pixels);
    using L             = typename LowerUnsigned<T>::Type;
    Image<T> workspace(width, height);

    std::memcpy(workspace.data.data(), srcData, width * height * sizeof(T));

    for (int k = iteration - 1; k >= 0; k--)
    {
        for (int x = 0; x < width >> k; x++)
        {
            int half = height >> (k + 1);
            std::vector<T> col0(half);
            std::vector<T> col1(half);
            for (int n = 0; n < half; ++n)
            {
                T d  = workspace.At(n, x);
                T c1 = workspace.At(half + n, x);
                T c0 = workspace.At(half + std::max(0, n - 1), x);
                T x0 = d - ((c0 + c1 + 2) >> 2);

                col0[n] = x0;
            }
            for (int n = 0; n < half; ++n)
            {
                T c1 = workspace.At(half + n, x);
                T x0 = col0[n];
                T x2 = col0[std::min(n + 1, half - 1)];
                T x1 = c1 + ((x0 + x2) >> 1);

                col1[n] = x1;
            }
            for (int n = 0; n < half; ++n)
            {
                workspace.At(2 * n, x)     = col0[n];
                workspace.At(2 * n + 1, x) = col1[n];
            }
        }

        for (int y = 0; y < height >> k; y++)
        {
            int half = width >> (k + 1);
            std::vector<T> row0(half);
            std::vector<T> row1(half);
            for (int n = 0; n < half; n++)
            {
                T d  = workspace.At(y, n);
                T c1 = workspace.At(y, half + n);
                T c0 = workspace.At(y, half + std::max(0, n - 1));
                T x0 = d - ((c0 + c1 + 2) >> 2);

                row0[n] = x0;
            }
            for (int n = 0; n < half; ++n)
            {
                T c1 = workspace.At(y, half + n);
                T x0 = row0[n];
                T x2 = row0[std::min(n + 1, half - 1)];
                T x1 = c1 + ((x0 + x2) >> 1);

                row1[n] = x1;
            }
            for (int n = 0; n < half; ++n)
            {
                workspace.At(y, 2 * n)     = row0[n];
                workspace.At(y, 2 * n + 1) = row1[n];
            }
        }
    }

    // std::ifstream ifs("I:/TerrainSystem/reconstruct.txt");
    // for (int i = 0; i < width * height; ++i)
    // {
    //     int val;
    //     ifs >> val;
    //     T recon = workspace.data[i];
    //     int y = i / width;
    //     int x = i % width;
    //     if (recon != val)
    //     {
    //         break;
    //     }
    // }

    dst.Initialize2D(LowerUnsigned<T>::Format, width, height, 1, 1);
    L* pixels = dst.GetImages()[0].pixels;
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            T val = workspace.At(i, j);

            // auto pixel = reinterpret_cast<const L*>(ori.pixels)[i * width + j];
            // if (lVal != pixel)
            // {
            //     printf("OH NO\n");
            // }

            pixels[i * width + j] = SaturatedCast<T, L>(val + avg);
        }
    }
}

template <typename T>
float WaveletTransform::FilterOut(const T* src, T* dst, size_t size, T threshold)
{
    int cnt = 0;
    for (int i = 0; i < size; ++i)
    {
        if (std::abs(src[i]) < threshold)
        {
            ++cnt;
            dst[i] = 0;
        }
        else
        {
            dst[i] = src[i];
        }
    }
    return static_cast<float>(cnt) / size;
}
