// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ dir + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs


#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>

#include "heightmap.h"
#include "stl.h"
#include "ThreadPool.h"
#include "Triangulator.h"

#include <meshoptimizer.h>
#include <unordered_set>

#include "BoundTree.h"
#include "SideFitter.h"

#define GENERATE_STL 0

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

// Main code
int main(int argc, char** argv)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    if (argc < 2) return 1;
    std::string inFile = argv[1];
    const std::wstring parent = std::filesystem::path(inFile).parent_path().wstring();
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

    // auto level heightmap
    hm->AutoLevel();
    hm->SaveDds(parent);
    std::cout << "dds generated" << std::endl;

    // return 0;
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

    for (int x = 0; x < nx; ++x)
    {
        for (int y = 0; y < ny; ++y)
        {
            results.emplace_back(g_ThreadPool.enqueue([&patches, x, y, &meshes]
            {
                const auto& heightMap = patches[x][y];
                // triangulate
                Triangulator tri(heightMap, 0, 131072, 65536);
                tri.Initialize();
                Triangulator::ErrorHeap errors;
                errors.emplace(0.005f);
                errors.emplace(0.004f);
                errors.emplace(0.003f);
                errors.emplace(0.002f);
                errors.emplace(0.001f);
                meshes[y][x] = tri.RunLod(std::move(errors));
                // Triangulator::TriangleCountHeap triangleCounts;
                // triangleCounts.emplace(131072);
                // triangleCounts.emplace(32768);
                // triangleCounts.emplace(8192);
                // triangleCounts.emplace(2048);
                // triangleCounts.emplace(512);
                // auto packedMeshes = tri.RunLod(std::move(triangleCounts));
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
            results.emplace_back(g_ThreadPool.enqueue([x, y, nx, ny, &meshes, &rivetSets]()
            {
                auto& rivetSet = rivetSets[y][x];

                for (const auto & v : meshes[y][x][0].first)
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
            results.emplace_back(g_ThreadPool.enqueue([x, y, &meshes, &rivetSets]()
            {
                const auto& rivetSet = rivetSets[y][x];
                auto& meshLods = meshes[y][x];

                for (auto& lod : meshLods)
                    SideFitter::Fit(lod, 256,
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
