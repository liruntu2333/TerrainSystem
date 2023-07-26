// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ dir + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define NOMINMAX
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>

#include "heightmap.h"
#include "stl.h"
#include "ThreadPool.h"
#include "Triangulator.h"

#include <meshoptimizer.h>
#include <regex>
#include <unordered_set>

#include "BoundTree.h"
#include "SideCutter.h"
#include "DXTexHelper.h"

#define GENERATE_STL

ThreadPool g_ThreadPool(std::thread::hardware_concurrency());

template <typename T>
void SaveBin(const std::filesystem::path& path, const std::vector<T>& data)
{
    std::ofstream ofs(path, std::ios::binary | std::ios::out);
    if (!ofs)
        throw std::runtime_error("failed to open " + path.u8string());
    for (const auto& v : data)
        ofs.write(reinterpret_cast<const char*>(&v), sizeof(T));
    ofs.close();
    std::printf("%s generated\n", path.u8string().c_str());
}

auto OptimizeMeshCache(std::vector<Triangulator::PackedPoint>& vertices, std::vector<uint32_t>& indices)
{
    const auto indexCount = indices.size();
    auto vertexCount = vertices.size();
    std::vector<std::uint32_t> oi(indexCount);
    std::vector<Triangulator::PackedPoint> ov(vertexCount);

    meshopt_optimizeVertexCache(
        oi.data(),
        indices.data(),
        indexCount,
        vertexCount);

    vertexCount = meshopt_optimizeVertexFetch(
        ov.data(),
        oi.data(),
        indexCount,
        vertices.data(),
        vertexCount,
        sizeof(Triangulator::PackedPoint));

    ov.resize(vertexCount);
    vertices = std::move(ov);
    indices = std::move(oi);
}

auto OptimizeMeshRedundant(std::vector<Triangulator::PackedPoint>& vertices, std::vector<uint32_t>& indices)
{
    const size_t indexCount = indices.size();
    size_t vertexCount = vertices.size();

    std::vector<uint32_t> remap(indexCount);
    vertexCount = meshopt_generateVertexRemap(
        remap.data(),
        indices.data(),
        indexCount,
        vertices.data(),
        vertexCount,
        sizeof(Triangulator::PackedPoint));

    std::vector<std::uint32_t> oi(indexCount);
    meshopt_remapIndexBuffer(
        oi.data(),
        indices.data(),
        indexCount,
        remap.data());

    std::vector<Triangulator::PackedPoint> ov(vertexCount);
    meshopt_remapVertexBuffer(
        ov.data(),
        vertices.data(),
        vertexCount,
        sizeof(Triangulator::PackedPoint),
        remap.data());

    vertices = std::move(ov);
    indices = std::move(oi);
}

void SaveErrorStatics(const std::map<float, int>& m)
{
    nlohmann::json jArray;
    for (const auto& [error, triangle] : m)
    {
        nlohmann::json j;
        j["geometry error"] = error;
        j["triangle"] = triangle;
        jArray.push_back(j);
    }
    std::ofstream ofs("asset/error.json");
    ofs << std::setw(4) << jArray;
    ofs.close();
    std::cout << "error.json generated" << std::endl;
}

template <typename T>
void MergeMesh(T& mesh, const T& other)
{
    auto& [vtx, idx] = mesh;
    const auto& [vtx2, idx2] = other;
    const auto offset = vtx.size();
    vtx.insert(vtx.end(), vtx2.begin(), vtx2.end());
    std::vector<typename decltype(idx)::value_type> modifiedIndices(idx2.size());
    std::transform(idx2.begin(), idx2.end(), modifiedIndices.begin(),
        [offset](const auto index) { return index + offset; });
    idx.insert(idx.end(), modifiedIndices.begin(), modifiedIndices.end());
}

template <typename T, typename... Ts>
void MergeMesh(T& mesh, const T& other, const Ts&... others)
{
    MergeMesh(mesh, other);
    MergeMesh(mesh, others...);
}

void GenerateClipmapFootPrints(const std::filesystem::path& path, const int n = 255)
{
    using Mesh = std::pair<std::vector<Triangulator::PackedPoint>, std::vector<unsigned>>;

    //Because the outer perimeter of each level must lie on the grid of the next-coarser level,
    //the grid size n must be odd. Hardware may be optimized for texture sizes that are powers of 2,
    //so we choose n = 2^k - 1, leaving 1 row and 1 column of the textures unused.
    if (n > std::numeric_limits<uint8_t>::max() || (n & (n + 1)) != 0)
        throw std::runtime_error("too large grid size");

    const int m = (n + 1) / 4;
    auto generateGrid = [](const int x, const int y, const int xOffset, const int yOffset)
    {
        std::vector<Triangulator::PackedPoint> vtx;
        vtx.reserve(x * y);
        for (int j = 0; j < y; ++j)
        {
            for (int i = 0; i < x; ++i)
            {
                vtx.emplace_back(i + xOffset, j + yOffset);
            }
        }

        std::vector<uint32_t> idx;
        idx.reserve((x - 1) * (y - 1) * 6);
        for (int j = 0; j < y - 1; ++j)
        {
            for (int i = 0; i < x - 1; ++i)
            {
                const auto a = i + j * x;
                const auto b = a + x;
                const auto c = a + 1;
                const auto d = b + 1;
                idx.emplace_back(a);
                idx.emplace_back(b);
                idx.emplace_back(c);
                idx.emplace_back(c);
                idx.emplace_back(b);
                idx.emplace_back(d);
            }
        }

        return std::make_pair(std::move(vtx), std::move(idx));
    };

    Mesh block = generateGrid(m, m, 0, 0);
    OptimizeMeshCache(block.first, block.second);
    SaveBin(path / "block.vtx", block.first);
    SaveBin(path / "block.idx", std::vector<uint16_t>(block.second.begin(), block.second.end()));

    Mesh ring;
    MergeMesh(ring,
        generateGrid(3, m, 2 * (m - 1), 0),
        generateGrid(m, 3, 3 * (m - 1) + 2, 2 * (m - 1)),
        generateGrid(3, m, 2 * (m - 1), 3 * (m - 1) + 2),
        generateGrid(m, 3, 0, 2 * (m - 1)));
    OptimizeMeshCache(ring.first, ring.second);
    SaveBin(path / "ring_fix_up.vtx", ring.first);
    SaveBin(path / "ring_fix_up.idx", std::vector<uint16_t>(ring.second.begin(), ring.second.end()));

    const auto top = generateGrid(2 * m + 1, 2, m - 1, m - 1);
    const auto rht = generateGrid(2, 2 * m + 1, 3 * (m - 1) + 1, m - 1);
    const auto btm = generateGrid(2 * m + 1, 2, m - 1, 3 * (m - 1) + 1);
    const auto lft = generateGrid(2, 2 * m + 1, m - 1, m - 1);

    auto saveTrim = [](const std::filesystem::path& trimPath, const Mesh& perimeter0, const Mesh& perimeter1)
    {
        Mesh trim;
        MergeMesh(trim, perimeter0, perimeter1);
        OptimizeMeshCache(trim.first, trim.second);
        SaveBin(trimPath.u8string() + ".vtx", trim.first);
        SaveBin(trimPath.u8string() + ".idx", std::vector<uint16_t>(trim.second.begin(), trim.second.end()));
    };

    saveTrim(path / "trim_lt",
        generateGrid(2, 2 * m + 1, m - 1, m - 1),
        generateGrid(2 * m, 2, m, m - 1));
    saveTrim(path / "trim_rt",
        generateGrid(2 * m + 1, 2, m - 1, m - 1),
        generateGrid(2, 2 * m, 3 * (m - 1) + 1, m));
    saveTrim(path / "trim_lb",
        generateGrid(2 * m + 1, 2, m - 1, 3 * (m - 1) + 1),
        generateGrid(2, 2 * m, m - 1, m - 1));
    saveTrim(path / "trim_rb",
        generateGrid(2, 2 * m + 1, 3 * (m - 1) + 1, m - 1),
        generateGrid(2 * m, 2, m - 1, 3 * (m - 1) + 1));
}

void CompositeNorAo(const std::filesystem::path& p)
{
    if (!is_directory(p)) return;

    std::filesystem::path nmPath, aoPath;
    for (auto&& entry : std::filesystem::directory_iterator(p))
    {
        std::regex rn(R"(.*normal_directx.*)");
        std::regex ra(R"(.*ao.*)");
        if (std::regex_match(entry.path().filename().generic_string(), rn))
            nmPath = entry.path();
        else if (std::regex_match(entry.path().filename().generic_string(), ra))
            aoPath = entry.path();
    }

    if (nmPath.empty() || aoPath.empty()) return;
    const auto nm = LoadWic(nmPath, true, false);
    const auto ao = LoadWic(aoPath, false, false);
    if (nm->GetMetadata().width != ao->GetMetadata().width ||
        nm->GetMetadata().height != ao->GetMetadata().height ||
        nm->GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM ||
        ao->GetMetadata().format != DXGI_FORMAT_R8_UNORM)
    {
        std::cerr << "size mismatch" << std::endl;
        return;
    }

    const auto w = nm->GetMetadata().width;
    const auto h = nm->GetMetadata().height;
    const auto nmData = reinterpret_cast<uint32_t*>(nm->GetPixels());
    const auto aoData = ao->GetPixels();
    for (auto y = 0; y < h; ++y)
    {
        for (auto x = 0; x < w; ++x)
        {
            const auto i = y * w + x;
            const auto a = aoData[i];
            nmData[i] = nmData[i] & 0xffffff | a << 24;
        }
    }

    SaveDds(p / "normal.dds",
        *GenerateMip(*nm->GetImage(0, 0, 0), DirectX::TEX_FILTER_SEPARATE_ALPHA));

    std::printf("composite %s normal\n", p.u8string().c_str());
}

void CompositeAlbRf(const std::filesystem::path& p)
{
    if (!is_directory(p)) return;

    std::filesystem::path amPath, rfPath;
    for (auto&& entry : std::filesystem::directory_iterator(p))
    {
        std::regex ra(R"(.*color.*)");
        std::regex rr(R"(.*roughness.*)");
        if (std::regex_match(entry.path().filename().generic_string(), ra))
            amPath = entry.path();
        else if (std::regex_match(entry.path().filename().generic_string(), rr))
            rfPath = entry.path();
    }

    if (amPath.empty() || rfPath.empty()) return;
    const auto am = LoadWic(amPath, true, false);
    const auto rm = LoadWic(rfPath, false, false);
    if (am->GetMetadata().width != rm->GetMetadata().width ||
        am->GetMetadata().height != rm->GetMetadata().height ||
        am->GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM ||
        rm->GetMetadata().format != DXGI_FORMAT_R8_UNORM)
    {
        std::cerr << "size mismatch" << std::endl;
        return;
    }

    const auto w = am->GetMetadata().width;
    const auto h = am->GetMetadata().height;
    const auto alData = reinterpret_cast<uint32_t*>(am->GetPixels());
    const auto rfData = rm->GetPixels();
    for (auto y = 0; y < h; ++y)
    {
        for (auto x = 0; x < w; ++x)
        {
            const auto i = y * w + x;
            const auto a = rfData[i];
            alData[i] = alData[i] & 0xffffff | a << 24;
        }
    }

    SaveDds(p / "albedo.dds",
        *GenerateMip(*am->GetImage(0, 0, 0), DirectX::TEX_FILTER_SEPARATE_ALPHA));
    std::printf("composite %s albedo\n", p.u8string().c_str());
}

// Main code
int main(int argc, char** argv)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
    {
        std::cerr << "failed to initialize COM" << std::endl;
        return 1;
    }
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    if (argc < 2) return 1;
    std::string inFile = argv[1];
    const std::wstring parent = std::filesystem::path(inFile).parent_path().wstring();

    //const auto clipmapPath = parent + L"/clipmap";
    //std::filesystem::create_directories(clipmapPath);
    //GenerateClipmapFootPrints(clipmapPath);

    for (auto&& dir : std::filesystem::directory_iterator(parent + L"\\texture_can"))
    {
        CompositeNorAo(dir);
        CompositeAlbRf(dir);
    }

    return 0;
    // load heightmap
    const auto hm = std::make_shared<Heightmap>(inFile);

    const int w = hm->Width();
    const int h = hm->Height();
    if (w * h == 0)
    {
        std::cerr << "invalid heightmap file (try png, jpg, etc.)" << std::endl;
        std::exit(1);
    }

    printf("  %d x %d = %d pixels\n", w, h, w * h);

    const auto patches = hm->SplitIntoPatches(256);
    std::cout << "patches generated" << std::endl;
    std::vector<std::pair<float, float>> bounds;
    for (const auto& row : patches)
        for (const auto& patch : row)
            bounds.emplace_back(patch->GetBound());
    const BoundTree tree(bounds, patches.front().size());
    tree.SaveJson("asset/bounds.json");
    std::cout << "bounds generated" << std::endl;
    const auto nx = patches.front().size();
    const auto ny = patches.size();

    std::vector meshes(ny, std::vector<std::vector<Triangulator::PackedMesh>>(nx));
    std::vector<std::future<void>> results;

    Triangulator::ErrorHeap errors;
    errors.emplace(0.0006998777389526367f);
    errors.emplace(0.00047141313552856445f);
    errors.emplace(0.0003293752670288086f);

    for (int x = 0; x < nx; ++x)
    {
        for (int y = 0; y < ny; ++y)
        {
            auto& mesh = meshes[y][x];
            const auto& heightMap = patches[y][x];
            results.emplace_back(g_ThreadPool.enqueue([&heightMap, x, y, &mesh, &errors]
            {
                // triangulate
                Triangulator tri(heightMap, 0, 131072, 65536);
                tri.Initialize();
                mesh = tri.RunLod(errors);
                // SaveErrorStatics(tri.AnalyzeLod());

                // auto mesh = tri.RunLod(triangleCounts);
            }));
        }
    }

    for (auto& result : results) result.get();
    results.clear();

    std::printf("Meshes generated.\n");

    std::vector rivetSets(ny, std::vector<std::unordered_set<Triangulator::PackedPoint>>(nx));
    for (int x = 0; x < nx; ++x)
    {
        for (int y = 0; y < ny; ++y)
        {
            auto& rivetSet = rivetSets[y][x];
            results.emplace_back(g_ThreadPool.enqueue([x, y, nx, ny, &meshes, &rivetSet]
            {
                for (const auto& v : meshes[y][x][0].first)
                    if (v.PosX == 0 || v.PosX == 255 || v.PosY == 0 || v.PosY == 255)
                        rivetSet.emplace(v);

                if (x > 0)
                    for (const auto& v : meshes[y][x - 1][0].first)
                        if (v.PosX == 255) rivetSet.emplace(0, v.PosY);

                if (x < nx - 1)
                    for (const auto& v : meshes[y][x + 1][0].first)
                        if (v.PosX == 0) rivetSet.emplace(255, v.PosY);

                if (y > 0)
                    for (const auto& v : meshes[y - 1][x][0].first)
                        if (v.PosY == 255) rivetSet.emplace(v.PosX, 0);

                if (y < ny - 1)
                    for (const auto& v : meshes[y + 1][x][0].first)
                        if (v.PosY == 0) rivetSet.emplace(v.PosX, 255);
            }));
        }
    }

    for (auto& result : results) result.get();
    results.clear();

    std::printf("Neighbors generated.\n");

    for (int x = 0; x < nx; ++x)
    {
        for (int y = 0; y < ny; ++y)
        {
            auto& meshLods = meshes[y][x];
            auto& rivetSet = rivetSets[y][x];
            results.emplace_back(g_ThreadPool.enqueue([x, y, &meshLods, &rivetSet]
            {
                for (auto& lod : meshLods)
                    SideCutter::Cut(lod, 256,
                        [&rivetSet](Triangulator::PackedPoint p)
                        {
                            return rivetSet.count(p) > 0;
                        });


                const std::filesystem::path path = "asset/" + std::to_string(x) + "_" + std::to_string(y);
                create_directories(path);

                for (int lod = 0; lod < meshLods.size(); ++lod)
                {
                    auto& [vb, ib] = meshLods[lod];
                    const auto vbPath = path.u8string() + "/lod" + std::to_string(lod) + ".vtx";
                    const auto ibPath = path.u8string() + "/lod" + std::to_string(lod) + ".idx";

                    OptimizeMeshRedundant(vb, ib);
                    OptimizeMeshCache(vb, ib);
                    SaveBin(vbPath, vb);
                    if (vb.size() > std::numeric_limits<std::uint16_t>::max())
                        SaveBin(ibPath, ib);
                    else
                        SaveBin(ibPath, std::vector<std::uint16_t>(ib.begin(), ib.end()));
                }
            }));
        }
    }

    for (auto& result : results) result.get();

    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Elapsed time in seconds : "
        << std::chrono::duration_cast<std::chrono::seconds>(end - begin).count()
        << " s" << std::endl;
    return 0;
}
