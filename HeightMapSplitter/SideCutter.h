#pragma once

#include <functional>
#include <iterator>

#include "triangulator.h"

struct SideCutter
{
public:
    static void Cut(
        Triangulator::PackedMesh& mesh, const unsigned int gridSize,
        const std::function<bool(Triangulator::PackedPoint)>& shouldAdd)
    {
        auto& [points, triangles] = mesh;
        std::deque<uint32_t> exteriorTriangles;
        std::vector<uint32_t> interiorTriangles;
        interiorTriangles.reserve(triangles.size());
        for (int t = 0; t < triangles.size(); t += 3)
        {
            const uint32_t i0 = triangles[t];
            const uint32_t i1 = triangles[t + 1];
            const uint32_t i2 = triangles[t + 2];
            const auto p0 = points[i0];
            const auto p1 = points[i1];
            const auto p2 = points[i2];
            if (CanSplit(p0, p1, gridSize))
                AddTriangle(exteriorTriangles, i0, i1, i2);
            else if (CanSplit(p1, p2, gridSize))
                AddTriangle(exteriorTriangles, i1, i2, i0);
            else if (CanSplit(p2, p0, gridSize))
                AddTriangle(exteriorTriangles, i2, i0, i1);
            else AddTriangle(interiorTriangles, i0, i1, i2);
        }

        for (int i = 0; i < 3; ++i)
        {
            const int triangleCount = exteriorTriangles.size() / 3;
            for (int t = 0; t < triangleCount; ++t)
            {
                const uint32_t i0 = exteriorTriangles[0];
                const uint32_t i1 = exteriorTriangles[1];
                const uint32_t i2 = exteriorTriangles[2];
                const auto p0 = points[i0];
                const auto p1 = points[i1];
                exteriorTriangles.pop_front();
                exteriorTriangles.pop_front();
                exteriorTriangles.pop_front();

                if (CanSplitX(p0, p1, gridSize))
                {
                    //  1---0               2           2               1---0
                    //  |  /               /|           |\               \  |
                    //  | /               / |           | \               \ |
                    //  |/               /  |           |  \               \|
                    //  2               0---1           0---1               2
                    const bool high0 = p0.PosX > p1.PosX;
                    uint32_t a = high0 ? i1 : i0;
                    const int begin = high0 ? p1.PosX : p0.PosX;
                    const int end = high0 ? p0.PosX : p1.PosX;
                    for (int k = begin + 1; k < end; ++k)
                    {
                        Triangulator::PackedPoint p { static_cast<uint8_t>(k), p0.PosY };
                        if (!shouldAdd(p)) continue;

                        const uint32_t ip = points.size();
                        points.emplace_back(p);
                        const uint32_t b = high0 ? i2 : ip;
                        const uint32_t c = high0 ? ip : i2;

                        if (CanSplitY(points[a], points[b], gridSize))
                            AddTriangle(exteriorTriangles, a, b, c);
                        else if (CanSplitY(points[b], points[c], gridSize))
                            AddTriangle(exteriorTriangles, b, c, a);
                        else if (CanSplitY(points[c], points[a], gridSize))
                            AddTriangle(exteriorTriangles, c, a, b);
                        else AddTriangle(interiorTriangles, a, b, c);

                        a = ip;
                    }

                    const uint32_t b = high0 ? i2 : i1;
                    const uint32_t c = high0 ? i0 : i2;
                    if (CanSplitY(points[a], points[b], gridSize))
                        AddTriangle(exteriorTriangles, a, b, c);
                    else if (CanSplitY(points[b], points[c], gridSize))
                        AddTriangle(exteriorTriangles, b, c, a);
                    else if (CanSplitY(points[c], points[a], gridSize))
                        AddTriangle(exteriorTriangles, c, a, b);
                    else AddTriangle(interiorTriangles, a, b, c);
                }
                else if (CanSplitY(p0, p1, gridSize))
                {
                    //  0---2               1           0               2---1
                    //  |  /               /|           |\               \  |
                    //  | /               / |           | \               \ |
                    //  |/               /  |           |  \               \|
                    //  1               2---0           1---2               0
                    const bool high0 = p0.PosY > p1.PosY;
                    uint32_t a = high0 ? i1 : i0;
                    const int begin = high0 ? p1.PosY : p0.PosY;
                    const int end = high0 ? p0.PosY : p1.PosY;
                    for (int k = begin + 1; k < end; ++k)
                    {
                        Triangulator::PackedPoint p { p0.PosX, static_cast<uint8_t>(k) };
                        if (!shouldAdd(p)) continue;

                        const uint32_t ip = points.size();
                        points.emplace_back(p);
                        const uint32_t b = high0 ? i2 : ip;
                        const uint32_t c = high0 ? ip : i2;

                        if (CanSplitX(points[a], points[b], gridSize))
                            AddTriangle(exteriorTriangles, a, b, c);
                        else if (CanSplitX(points[b], points[c], gridSize))
                            AddTriangle(exteriorTriangles, b, c, a);
                        else if (CanSplitX(points[c], points[a], gridSize))
                            AddTriangle(exteriorTriangles, c, a, b);
                        else AddTriangle(interiorTriangles, a, b, c);

                        a = ip;
                    }
                    const uint32_t b = high0 ? i2 : i1;
                    const uint32_t c = high0 ? i0 : i2;

                    if (CanSplitX(points[a], points[b], gridSize))
                        AddTriangle(exteriorTriangles, a, b, c);
                    else if (CanSplitX(points[b], points[c], gridSize))
                        AddTriangle(exteriorTriangles, b, c, a);
                    else if (CanSplitX(points[c], points[a], gridSize))
                        AddTriangle(exteriorTriangles, c, a, b);
                    else AddTriangle(interiorTriangles, a, b, c);
                }
                else
                {
                    AddTriangle(exteriorTriangles, i1, i2, i0);
                }
            }
        }

        triangles = std::move(interiorTriangles);
        triangles.reserve(triangles.size() + exteriorTriangles.size());
        std::copy(exteriorTriangles.begin(), exteriorTriangles.end(), std::back_inserter(triangles));
    }

private:
    static bool CanSplit(
        const Triangulator::PackedPoint p0, const Triangulator::PackedPoint p1, const unsigned int gridSize)
    {
        return CanSplitX(p0, p1, gridSize) || CanSplitY(p0, p1, gridSize);
    }

    static bool CanSplitX(
        const Triangulator::PackedPoint p0, const Triangulator::PackedPoint p1, const unsigned int gridSize)
    {
        if ((p0.PosY == 0 && p1.PosY == 0) || (p0.PosY == gridSize - 1 && p1.PosY == gridSize - 1))
            return std::abs(p0.PosX - p1.PosX) > 1;
        return false;
    }

    static bool CanSplitY(
        const Triangulator::PackedPoint p0, const Triangulator::PackedPoint p1, const unsigned int gridSize)
    {
        if ((p0.PosX == 0 && p1.PosX == 0) || (p0.PosX == gridSize - 1 && p1.PosX == gridSize - 1))
            return std::abs(p0.PosY - p1.PosY) > 1;
        return false;
    }

    template <typename T>
    static void AddTriangle(T& triangles, uint32_t i0, uint32_t i1, uint32_t i2)
    {
        triangles.emplace_back(i0);
        triangles.emplace_back(i1);
        triangles.emplace_back(i2);
    }
};
