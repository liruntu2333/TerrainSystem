#pragma once

#include <functional>
#include "triangulator.h"

struct SideFitter
{
public:
    static void Fit(
        Triangulator::PackedMesh& mesh, unsigned int gridSize,
        const std::function<bool(Triangulator::PackedPoint)>& f)
    {
        std::vector<uint32_t> outTriangles {};
        auto& [points, triangles] = mesh;
        outTriangles.reserve(triangles.size());
        std::deque<uint32_t> triangleQueue { triangles.begin(), triangles.end() };

        while (!triangleQueue.empty())
        {
            uint32_t i0 = triangleQueue[0];
            uint32_t i1 = triangleQueue[1];
            uint32_t i2 = triangleQueue[2];
            const auto p0 = points[i0];
            const auto p1 = points[i1];
            const auto p2 = points[i2];
            triangleQueue.pop_front();
            triangleQueue.pop_front();
            triangleQueue.pop_front();

            if (ShouldSplitX(p0, p1, gridSize))
            {
                //  1---0           2
                //  |  /           /|
                //  | /     OR    / |
                //  |/           /  |
                //  2           0---1
                const bool p0OnRight = p0.PosX > p1.PosX;
                uint32_t prev = p0OnRight ? i1 : i0;
                const int from = p0OnRight ? p1.PosX + 1 : p0.PosX + 1;
                const int to = p0OnRight ? p0.PosX : p1.PosX;
                for (int j = from; j < to; ++j)
                {
                    Triangulator::PackedPoint p { static_cast<uint8_t>(j), p0.PosY };
                    if (!f(p)) continue;

                    const uint32_t ip = points.size();
                    points.emplace_back(p);
                    if (j == from) // corner case : rectangular triangle
                    {
                        AddTriangle(triangleQueue,
                            prev,
                            p0OnRight ? i2 : ip,
                            p0OnRight ? ip : i2);
                    }
                    else
                    {
                        AddTriangle(outTriangles,
                            prev,
                            p0OnRight ? i2 : ip,
                            p0OnRight ? ip : i2);
                    }
                    prev = ip;
                }

                if (prev == (p0OnRight ? i1 : i0))
                    AddTriangle(outTriangles,
                        prev,
                        p0OnRight ? i2 : i1,
                        p0OnRight ? i0 : i2);
                else// corner case : rectangular triangle
                    AddTriangle(triangleQueue,
                        prev,
                        p0OnRight ? i2 : i1,
                        p0OnRight ? i0 : i2);
            }
            else if (ShouldSplitY(p0, p1, gridSize))
            {
                //    1             0
                //   /|             |\
                //  2 |     OR      | 2
                //   \|             |/
                //    0             1
                const bool p0OnBottom = p0.PosY > p1.PosY;
                uint32_t prev = p0OnBottom ? i1 : i0;
                const int from = p0OnBottom ? p1.PosY + 1 : p0.PosY + 1;
                const int to = p0OnBottom ? p0.PosY : p1.PosY;
                for (int j = from; j < to; ++j)
                {
                    Triangulator::PackedPoint p { p0.PosX, static_cast<uint8_t>(j) };
                    if (!f(p)) continue;

                    uint32_t newPoint = points.size();
                    points.emplace_back(p);
                    if (j == from)  // corner case : rectangular triangle
                    {
                        AddTriangle(triangleQueue,
                            prev,
                            p0OnBottom ? i2 : newPoint,
                            p0OnBottom ? newPoint : i2);
                    }
                    else
                    {
                        AddTriangle(outTriangles,
                            prev,
                            p0OnBottom ? i2 : newPoint,
                            p0OnBottom ? newPoint : i2);
                    }
                    prev = newPoint;
                }
                if (prev == (p0OnBottom ? i1 : i0))
                    AddTriangle(outTriangles,
                        prev,
                        p0OnBottom ? i2 : i1,
                        p0OnBottom ? i0 : i2);
                else // corner case : rectangular triangle
                    AddTriangle(triangleQueue,
                        prev,
                        p0OnBottom ? i2 : i1,
                        p0OnBottom ? i0 : i2);
            }
            else if (ShouldSplit(p1, p2, gridSize))
                AddTriangle(triangleQueue, i1, i2, i0);
            else if (ShouldSplit(p2, p0, gridSize))
                AddTriangle(triangleQueue, i2, i0, i1);
            else AddTriangle(outTriangles, i0, i1, i2);
        }

        triangles = std::move(outTriangles);
    }

private:
    static bool ShouldSplit(Triangulator::PackedPoint p0, Triangulator::PackedPoint p1, unsigned int gridSize)
    {
        return ShouldSplitX(p0, p1, gridSize) || ShouldSplitY(p0, p1, gridSize);
    }

    static bool ShouldSplitX(Triangulator::PackedPoint p0, Triangulator::PackedPoint p1, unsigned int gridSize)
    {
        if (const bool onSameSide =
            (p0.PosY == 0 && p1.PosY == 0) ||
            (p0.PosY == gridSize - 1 && p1.PosY == gridSize - 1); !onSameSide)
            return false;
        const bool spaceBetweenX = std::abs(p0.PosX - p1.PosX) > 1;
        return spaceBetweenX;
    }

    static bool ShouldSplitY(Triangulator::PackedPoint p0, Triangulator::PackedPoint p1, unsigned int gridSize)
    {
        if (const bool onSameSide =
            (p0.PosX == 0 && p1.PosX == 0) ||
            (p0.PosX == gridSize - 1 && p1.PosX == gridSize - 1); !onSameSide)
            return false;
        const bool spaceBetweenY = std::abs(p0.PosY - p1.PosY) > 1;
        return spaceBetweenY;
    }

    template <typename T>
    static void AddTriangle(T& triangles, uint32_t i0, uint32_t i1, uint32_t i2)
    {
        triangles.emplace_back(i0);
        triangles.emplace_back(i1);
        triangles.emplace_back(i2);
    }
};
