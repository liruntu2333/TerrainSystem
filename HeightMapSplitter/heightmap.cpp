#include "heightmap.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/normal.hpp>
#include <glm/gtx/polar_coordinates.hpp>

#include "blur.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"
#include "ThreadPool.h"

#define NOMINMAX
#include <DirectXTex.h>

Heightmap::Heightmap(const std::string& path) :
    m_Width(0),
    m_Height(0)
{
    int w, h, c;
    uint16_t* data = stbi_load_16(path.c_str(), &w, &h, &c, 1);
    if (!data)
    {
        return;
    }
    m_Width = w;
    m_Height = h;
    const int n = w * h;
    const float m = 1.f / 65535.f;
    m_Data.resize(n);
    for (int i = 0; i < n; i++)
    {
        m_Data[i] = data[i] * m;
    }
    free(data);
}

Heightmap::Heightmap(
    const int width,
    const int height,
    const std::vector<float>& data) :
    m_Width(width),
    m_Height(height),
    m_Data(data) {}

void Heightmap::AutoLevel()
{
    float lo = m_Data[0];
    float hi = m_Data[0];
    for (int i = 0; i < m_Data.size(); i++)
    {
        lo = std::min(lo, m_Data[i]);
        hi = std::max(hi, m_Data[i]);
    }
    if (hi == lo)
    {
        return;
    }
    for (int i = 0; i < m_Data.size(); i++)
    {
        m_Data[i] = (m_Data[i] - lo) / (hi - lo);
    }
}

void Heightmap::Invert()
{
    for (int i = 0; i < m_Data.size(); i++)
    {
        m_Data[i] = 1.f - m_Data[i];
    }
}

void Heightmap::GammaCurve(const float gamma)
{
    for (int i = 0; i < m_Data.size(); i++)
    {
        m_Data[i] = std::pow(m_Data[i], gamma);
    }
}

void Heightmap::AddBorder(const int size, const float z)
{
    const int w = m_Width + size * 2;
    const int h = m_Height + size * 2;
    std::vector<float> data(w * h, z);
    int i = 0;
    for (int y = 0; y < m_Height; y++)
    {
        int j = (y + size) * w + size;
        for (int x = 0; x < m_Width; x++)
        {
            data[j++] = m_Data[i++];
        }
    }
    m_Width = w;
    m_Height = h;
    m_Data = data;
}

void Heightmap::GaussianBlur(const int r)
{
    m_Data = ::GaussianBlur(m_Data, m_Width, m_Height, r);
}

std::vector<glm::vec3> Heightmap::Normalmap(const float zScale) const
{
    const int w = m_Width - 1;
    const int h = m_Height - 1;
    std::vector<glm::vec3> result(w * h);
    int i = 0;
    for (int y0 = 0; y0 < h; y0++)
    {
        const int y1 = y0 + 1;
        const float yc = y0 + 0.5f;
        for (int x0 = 0; x0 < w; x0++)
        {
            const int x1 = x0 + 1;
            const float xc = x0 + 0.5f;
            const float z00 = At(x0, y0) * -zScale;
            const float z01 = At(x0, y1) * -zScale;
            const float z10 = At(x1, y0) * -zScale;
            const float z11 = At(x1, y1) * -zScale;
            const float zc = (z00 + z01 + z10 + z11) / 4.f;
            const glm::vec3 p00(x0, y0, z00);
            const glm::vec3 p01(x0, y1, z01);
            const glm::vec3 p10(x1, y0, z10);
            const glm::vec3 p11(x1, y1, z11);
            const glm::vec3 pc(xc, yc, zc);
            const glm::vec3 n0 = glm::triangleNormal(pc, p00, p10);
            const glm::vec3 n1 = glm::triangleNormal(pc, p10, p11);
            const glm::vec3 n2 = glm::triangleNormal(pc, p11, p01);
            const glm::vec3 n3 = glm::triangleNormal(pc, p01, p00);
            result[i] = glm::normalize(n0 + n1 + n2 + n3);
            i++;
        }
    }
    return result;
}

void Heightmap::SaveNormalmap(
    const std::string& path,
    const float zScale) const
{
    const std::vector<glm::vec3> nm = Normalmap(zScale);
    std::vector<uint8_t> data(nm.size() * 3);
    int i = 0;
    for (glm::vec3 n : nm)
    {
        n = (n + 1.f) / 2.f;
        data[i++] = uint8_t(n.x * 255);
        data[i++] = uint8_t(n.y * 255);
        data[i++] = uint8_t(n.z * 255);
    }
    stbi_write_png(
        path.c_str(), m_Width - 1, m_Height - 1, 3,
        data.data(), (m_Width - 1) * 3);
}

void Heightmap::SaveHillshade(
    const std::string& path,
    const float zScale,
    const float altitude,
    const float azimuth) const
{
    const glm::vec3 light = glm::euclidean(glm::vec2(
        glm::radians(altitude), glm::radians(-azimuth))).xzy();
    const std::vector<glm::vec3> nm = Normalmap(zScale);
    std::vector<uint8_t> data(nm.size() * 3);
    int i = 0;
    for (glm::vec3 n : nm)
    {
        const uint8_t d = glm::clamp(glm::dot(n, light), 0.f, 1.f) * 255;
        data[i++] = d;
        data[i++] = d;
        data[i++] = d;
    }
    stbi_write_png(
        path.c_str(), m_Width - 1, m_Height - 1, 3,
        data.data(), (m_Width - 1) * 3);
}

void Heightmap::SaveDds(const std::wstring& path) const
{
    // save hm
    std::vector<uint16_t> normalized;
    normalized.reserve(m_Data.size());
    for (const float h : m_Data)
        normalized.emplace_back(h * UINT16_MAX);

    DirectX::Image img {};
    img.width = m_Width;
    img.height = m_Height;
    img.format = DXGI_FORMAT_R16_UNORM;
    img.rowPitch = m_Width * sizeof(uint16_t);
    img.slicePitch = m_Data.size() * sizeof(uint16_t);
    img.pixels = reinterpret_cast<uint8_t*>(normalized.data());

    SaveToDDSFile(img, DirectX::DDS_FLAGS_NONE, (path + L"/height.dds").c_str());
}

std::vector<std::vector<std::shared_ptr<Heightmap>>> Heightmap::SplitIntoPatches(int patchSize) const
{
    const int nx = m_Width / (patchSize - 1);
    const int ny = m_Height / (patchSize - 1);
    std::vector patches(nx, std::vector<std::shared_ptr<Heightmap>>(ny));
    for (int i = 0; i < nx; ++i)
    {
        for (int j = 0; j < ny; ++j)
        {
            std::vector<float> data(patchSize * patchSize);
            for (int k = 0; k < patchSize * patchSize; ++k)
            {
                const int x = k % patchSize + i * (patchSize - 1);
                const int y = k / patchSize + j * (patchSize - 1);
                data[k] = At(x, y);
            }
            patches[j][i] = std::make_shared<Heightmap>(patchSize, patchSize, data);
        }
    }
    return patches;
}

std::pair<glm::ivec2, float> Heightmap::FindCandidate(
    const glm::ivec2 p0,
    const glm::ivec2 p1,
    const glm::ivec2 p2) const
{
    const auto edge = [](
        const glm::ivec2 a, const glm::ivec2 b, const glm::ivec2 c)
    {
        return (b.x - c.x) * (a.y - c.y) - (b.y - c.y) * (a.x - c.x);
    };

    // triangle bounding box
    const glm::ivec2 min = glm::min(glm::min(p0, p1), p2);
    const glm::ivec2 max = glm::max(glm::max(p0, p1), p2);

    // forward differencing variables
    int w00 = edge(p1, p2, min);
    int w01 = edge(p2, p0, min);
    int w02 = edge(p0, p1, min);
    const int a01 = p1.y - p0.y;
    const int b01 = p0.x - p1.x;
    const int a12 = p2.y - p1.y;
    const int b12 = p1.x - p2.x;
    const int a20 = p0.y - p2.y;
    const int b20 = p2.x - p0.x;

    // pre-multiplied z values at vertices
    const float a = edge(p0, p1, p2);
    const float z0 = At(p0) / a;
    const float z1 = At(p1) / a;
    const float z2 = At(p2) / a;

    // iterate over pixels in bounding box
    float maxError = 0;
    glm::ivec2 maxPoint(0);
    for (int y = min.y; y <= max.y; y++)
    {
        // compute starting offset
        int dx = 0;
        if (w00 < 0 && a12 != 0)
        {
            dx = std::max(dx, -w00 / a12);
        }
        if (w01 < 0 && a20 != 0)
        {
            dx = std::max(dx, -w01 / a20);
        }
        if (w02 < 0 && a01 != 0)
        {
            dx = std::max(dx, -w02 / a01);
        }

        int w0 = w00 + a12 * dx;
        int w1 = w01 + a20 * dx;
        int w2 = w02 + a01 * dx;

        bool wasInside = false;

        for (int x = min.x + dx; x <= max.x; x++)
        {
            // check if inside triangle
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                wasInside = true;

                // compute z using barycentric coordinates
                const float z = z0 * w0 + z1 * w1 + z2 * w2;
                const float dz = std::abs(z - At(x, y));
                if (dz > maxError)
                {
                    maxError = dz;
                    maxPoint = glm::ivec2(x, y);
                }
            }
            else if (wasInside)
            {
                break;
            }

            w0 += a12;
            w1 += a20;
            w2 += a01;
        }

        w00 += b12;
        w01 += b20;
        w02 += b01;
    }

    if (maxPoint == p0 || maxPoint == p1 || maxPoint == p2)
    {
        maxError = 0;
    }

    return std::make_pair(maxPoint, maxError);
}

std::pair<float, float> Heightmap::GetBound() const
{
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::min();
    for (float h : m_Data)
    {
        min = std::min(min, h);
        max = std::max(max, h);
    }
    return { min, max };
}
