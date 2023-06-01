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

#define GENERATE_STL 0

template <typename T>
void SaveBin(const std::filesystem::path& path, const std::vector<T>& data)
{
    std::ofstream ofs(path, std::ios::binary | std::ios::out);
    if (!ofs)
        throw std::runtime_error("failed to open " + path.u8string());
    for (const auto& v : data)
        ofs.write(reinterpret_cast<const char*>(&v), sizeof(T));
    ofs.close();
    std::cout << path << " generated" << std::endl;
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

    hm->SaveDds(L"asset", 2000);
    //return 0;
    const auto patches = hm->SplitIntoPatches(256);

    std::cout << "patches generated" << std::endl;
    const auto nx = patches.front().size();
    const auto ny = patches.size();

    ThreadPool pool(std::thread::hardware_concurrency());
    std::vector<std::future<void>> results;

    for (int i = 0; i < nx; ++i)
    {
        for (int j = 0; j < ny; ++j)
        {
            results.emplace_back(pool.enqueue([&patches, i, j]
            {
                const auto& heightMap = patches[i][j];
                const std::string patchName = "asset/" + std::to_string(i) + "_" + std::to_string(j);
                const std::filesystem::path path = patchName;

                create_directories(path);
                // triangulate
                Triangulator tri(heightMap, 0, 0, 0);
                tri.Initialize();
                Triangulator::MaxHeap errors;
                errors.emplace(0.001f);
                errors.emplace(0.0005f);
                errors.emplace(0.0002f);
                errors.emplace(0.0001f);
                errors.emplace(0.0f);

#if GENERATE_STL
                auto meshes = tri.RunLod(std::move(errors), 2000);
                const auto stlPath = path.u8string() + "/lod" + std::to_string(lod) + ".stl";
                for (int lod = 0; lod < meshes.size(); ++lod)
                {
                    const auto& [p, t] = meshes[lod];
                    SaveBinarySTL(stlPath.u8string() + ".stl", p, t);
                    std::cout << path << " lod " << lod << " generated" << std::endl;
                }
#endif

                auto packedMeshes = tri.RunLod(std::move(errors));
                for (int lod = 0; lod < packedMeshes.size(); ++lod)
                {
                    auto& [vb, ib] = packedMeshes[lod];
                    const auto vbPath = path.u8string() + "/lod" + std::to_string(lod) + ".vtx";
                    const auto ibPath = path.u8string() + "/lod" + std::to_string(lod) + ".idx";

                    OptimizeMeshRedundant(vb, ib);
                    OptimizeMeshCache(vb, ib);
                    SaveBin(vbPath, vb);
                    if (vb.size() > std::numeric_limits<std::uint16_t>::max())
                    {
                        SaveBin(ibPath, ib);
                    }
                    else
                    {
                        SaveBin(ibPath, std::vector<std::uint16_t>(ib.begin(), ib.end()));
                    }
                }
            }));
        }
    }

    for (auto& result : results)
        result.get();

    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Elapsed time in seconds : "
        << std::chrono::duration_cast<std::chrono::seconds>(end - begin).count()
        << " s" << std::endl;
    return 0;
}
