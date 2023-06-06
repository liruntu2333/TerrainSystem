#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include <queue>
#include <vector>
#include <glm/common.hpp>
#include "heightmap.h"

class Triangulator
{
public:
    struct PackedPoint // Image space (0 ~ 255)
    {
        uint8_t PosX {};
        uint8_t PosY {};

        //uint8_t MorphPosX{}; // higher lod
        //uint8_t MorphPosY{};

        PackedPoint() = default;

        PackedPoint(uint8_t px, uint8_t py/*, uint8_t mpx, uint8_t mpy*/) :
            PosX(px), PosY(py)/*, MorphPosX(mpx), MorphPosY(mpy)*/ {}
    };

    Triangulator(
        const std::shared_ptr<Heightmap>& heightmap,
        float error, int nTri, int nVert);

    void Initialize();
    void RunStep();

    using ErrorHeap = std::priority_queue<float, std::vector<float>, std::less<>>;
    using TriangleCountHeap = std::priority_queue<int, std::vector<int>, std::greater<>>;
    using Mesh = std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>>;
    using PackedMesh = std::pair<std::vector<PackedPoint>, std::vector<uint32_t>>;
    std::vector<Mesh> RunLod(ErrorHeap errors, float zScale);
    std::vector<PackedMesh> RunLod(TriangleCountHeap triangleMax);
    std::vector<PackedMesh> RunLod(ErrorHeap errors);

    void Run();
    void Morph(float target);

    int NumPoints() const
    {
        return m_Points.size();
    }

    int NumTriangles() const
    {
        return m_Queue.size();
    }

    float Error() const;
    std::vector<glm::vec3> Points(const float zScale) const;
    std::vector<glm::ivec3> Triangles() const;

private:

    void Flush();

    void Step();

    int AddPoint(const glm::ivec2 point);

    int AddTriangle(
        const int a, const int b, const int c,
        const int ab, const int bc, const int ca,
        int e);

    void Legalize(const int a);

    void QueuePush(const int t);
    int QueuePop();
    int QueuePopBack();
    void QueueRemove(const int t);
    bool QueueLess(const int i, const int j) const;
    void QueueSwap(const int i, const int j);
    void QueueUp(const int j0);
    bool QueueDown(const int i0, const int n);

    std::shared_ptr<Heightmap> m_Heightmap;

    std::vector<glm::ivec2> m_Points;
    std::vector<int> m_Triangles;
    std::vector<int> m_Halfedges;
    std::vector<glm::ivec2> m_Candidates;
    std::vector<float> m_Errors;
    std::vector<int> m_QueueIndexes;
    std::vector<int> m_Queue;
    std::vector<int> m_Pending;

    //std::vector<int> m_MorphTarget;
    //std::vector<int> m_MorphTargetTmp;

    const float m_MaxError;
    const int m_MaxTriangles;
    const int m_MaxPoints;
};
