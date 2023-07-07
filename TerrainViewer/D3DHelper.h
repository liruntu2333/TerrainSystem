#pragma once

#include <exception>
#include <cstdio>
#include <fstream>

namespace DirectX
{
    // Helper class for COM exceptions
    class com_exception : public std::exception
    {
    public:
        com_exception(HRESULT hr) : result(hr) {}

        [[nodiscard]] const char* what() const noexcept override
        {
            static char s_str[64] = {};
            sprintf_s(s_str, "Failure with HRESULT of %08X",
                static_cast<unsigned int>(result));
            return s_str;
        }

    private:
        HRESULT result;
    };

    // Helper utility converts D3D API failures into exceptions.
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw com_exception(hr);
        }
    }

    constexpr UINT CalculateBufferSize(UINT byteSize)
    {
        return (byteSize + 255) & ~255;
    }
}

template <class T>
std::vector<T> LoadBinary(const std::filesystem::path& path)
{
    std::vector<T> data;
    std::ifstream ifs(path, std::ios::binary | std::ios::in);
    if (!ifs)
        throw std::runtime_error("failed to open " + path.string());
    const auto fileSize = file_size(path);
    data.resize(fileSize / sizeof(T));
    ifs.read(reinterpret_cast<char*>(data.data()), fileSize);
    ifs.close();
    return data;
}

template <class T, class U>
T WarpMod(T n, U m)
{
    // warp a int and get its positive mod
    if (n > 0) return n % m;
    return (n % m + m) % m;
}
