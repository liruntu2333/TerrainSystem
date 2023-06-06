#include "triangulator.h"

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <unordered_set>

struct SideStitcher
{
public:
    static void Stitch(Triangulator::PackedMesh& mesh, unsigned int gridSize)
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
                    const uint32_t ip = points.size();
                    points.emplace_back(j, p0.PosY);
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
                // corner case : rectangular triangle
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
                    uint32_t newPoint = points.size();
                    points.emplace_back(p0.PosX, j);
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
                // corner case : rectangular triangle
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


Triangulator::Triangulator(
    const std::shared_ptr<Heightmap>& heightmap,
    float error, int nTri, int nVert) :
    m_Heightmap(heightmap), m_MaxError(error), m_MaxTriangles(nTri), m_MaxPoints(nVert) {}

void Triangulator::RunStep()
{
    //Snapshot();

    // helper function to check if triangulation is complete
    const auto done = [this]()
    {
        const float e = Error();
        if (e <= m_MaxError)
        {
            return true;
        }
        if (m_MaxTriangles > 0 && NumTriangles() >= m_MaxTriangles)
        {
            return true;
        }
        if (m_MaxPoints > 0 && NumPoints() >= m_MaxPoints)
        {
            return true;
        }
        return e == 0;
    };

    if (!done())
    {
        Step();
    }
}

std::vector<Triangulator::Mesh> Triangulator::RunLod(ErrorHeap errors, float zScale)
{
    std::vector<Mesh> lods;
    while (!errors.empty())
    {
        const float error = errors.top();
        if (m_MaxError > error)
            throw std::exception("error < m_MaxError");
        errors.pop();

        // helper function to check if triangulation is complete
        const auto done = [this, error]()
        {
            const float e = Error();
            if (e <= error)
            {
                return true;
            }
            return e == 0;
        };

        while (!done())
        {
            Step();
        }

        lods.emplace_back(Points(zScale), Triangles());
    }
    std::reverse(lods.begin(), lods.end());
    return lods;
}

std::vector<Triangulator::PackedMesh> Triangulator::RunLod(TriangleCountHeap triangleMax)
{
    std::vector<PackedMesh> lods;
    while (!triangleMax.empty())
    {
        const float triangleCount = triangleMax.top();
        triangleMax.pop();

        // helper function to check if triangulation is complete
        const auto done = [this, triangleCount]()
        {
            if (NumTriangles() >= triangleCount)
            {
                return true;
            }
            return Error() == 0;
        };

        while (!done())
        {
            Step();
        }

        PackedMesh mesh;
        auto& [vb, ib] = mesh;
        vb.reserve(m_Points.size());
        for (const auto& p : m_Points)
            vb.emplace_back(p.x, p.y);

        ib.reserve(m_Queue.size() * 3);
        for (const int i : m_Queue)
            for (int j = 0; j < 3; ++j)
                ib.emplace_back(static_cast<uint32_t>(m_Triangles[i * 3 + j]));

        SideStitcher::Stitch(mesh, m_Heightmap->Width());
        lods.emplace_back(mesh);
    }
    std::reverse(lods.begin(), lods.end());
    return lods;
}

std::vector<Triangulator::PackedMesh> Triangulator::RunLod(ErrorHeap errors)
{
    std::vector<PackedMesh> lods;
    while (!errors.empty())
    {
        const float error = errors.top();
        if (m_MaxError > error)
            throw std::exception("error < m_MaxError");
        errors.pop();

        // helper function to check if triangulation is complete
        const auto done = [this, error]()
        {
            const float e = Error();
            if (e <= error)
            {
                return true;
            }
            return e == 0;
        };

        while (!done())
        {
            Step();
        }

        PackedMesh mesh;
        auto& [vb, ib] = mesh;
        vb.reserve(m_Points.size());
        for (const auto& p : m_Points)
            vb.emplace_back(p.x, p.y);

        ib.reserve(m_Queue.size() * 3);
        for (const int i : m_Queue)
            for (int j = 0; j < 3; ++j)
                ib.emplace_back(static_cast<uint32_t>(m_Triangles[i * 3 + j]));

        // Deprecated code for recording morph target between low and high detail.
        // Topology changes while iterating.
        // To preserve topology, need to set triangle restraint on TIN construction process.
        //for (int i = morphStart; i >= vb.size() - 1; ++i)
        //{
        //    if (vb[i].PosX == m_Heightmap->Width() - 1 || vb[i].PosY == m_Heightmap->Height() - 1 ||
        //        vb[i].PosX == 0 || vb[i].PosY == 0)
        //        continue;
        //    const auto& target = vb[m_MorphTarget[i]];
        //    vb[i].MorphPosX = target.PosX;
        //    vb[i].MorphPosY = target.PosY;
        //}

        SideStitcher::Stitch(mesh, m_Heightmap->Width());
        lods.emplace_back(mesh);
    }
    std::reverse(lods.begin(), lods.end());
    return lods;
}

void Triangulator::Run()
{
    //Snapshot();

    // helper function to check if triangulation is complete
    const auto done = [this]()
    {
        const float e = Error();
        if (e <= m_MaxError)
        {
            return true;
        }
        if (m_MaxTriangles > 0 && NumTriangles() >= m_MaxTriangles)
        {
            return true;
        }
        if (m_MaxPoints > 0 && NumPoints() >= m_MaxPoints)
        {
            return true;
        }
        return e == 0;
    };

    while (!done())
    {
        Step();
    }
}

void Triangulator::Morph(float target)
{
    //const int start = static_cast<float>(m_Points.size()) * target;
    //for (int i = start; i < m_Points.size(); ++i)
    //{
    //    if (m_MorphTarget[i] < 0) continue;
    //    m_Points[i] = m_Points[m_MorphTarget[i]];
    //}
}

void Triangulator::Initialize()
{
    m_Points.clear();
    m_Triangles.clear();
    m_Halfedges.clear();
    m_Candidates.clear();
    m_Errors.clear();
    m_QueueIndexes.clear();
    m_Queue.clear();
    m_Pending.clear();
    //m_MorphTarget.clear();

    // add points at all four corners
    const int x0 = 0;
    const int y0 = 0;
    const int x1 = m_Heightmap->Width() - 1;
    const int y1 = m_Heightmap->Height() - 1;
    const int p0 = AddPoint(glm::ivec2(x0, y0));
    const int p1 = AddPoint(glm::ivec2(x1, y0));
    const int p2 = AddPoint(glm::ivec2(x0, y1));
    const int p3 = AddPoint(glm::ivec2(x1, y1));

    //m_MorphTarget.resize(4);
    //m_MorphTarget[0] = -1;
    //m_MorphTarget[1] = -1;
    //m_MorphTarget[2] = -1;
    //m_MorphTarget[3] = -1;

    // add initial two triangles
    const int t0 = AddTriangle(p3, p0, p2, -1, -1, -1, -1);
    AddTriangle(p0, p3, p1, t0, -1, -1, -1);
    Flush();
}

float Triangulator::Error() const
{
    return m_Errors[m_Queue[0]];
}

std::vector<glm::vec3> Triangulator::Points(const float zScale) const
{
    std::vector<glm::vec3> points;
    points.reserve(m_Points.size());
    const int h1 = m_Heightmap->Height() - 1;
    for (const glm::ivec2& p : m_Points)
    {
        points.emplace_back(p.x, h1 - p.y, m_Heightmap->At(p.x, p.y) * zScale);
    }
    return points;
}

std::vector<glm::ivec3> Triangulator::Triangles() const
{
    std::vector<glm::ivec3> triangles;
    triangles.reserve(m_Queue.size());
    for (const int i : m_Queue)
    {
        triangles.emplace_back(
            m_Triangles[i * 3 + 0],
            m_Triangles[i * 3 + 1],
            m_Triangles[i * 3 + 2]);
    }
    return triangles;
}

void Triangulator::Flush()
{
    for (const int t : m_Pending)
    {
        // rasterize triangle to find maximum pixel error
        const auto pair = m_Heightmap->FindCandidate(
            m_Points[m_Triangles[t * 3 + 0]],
            m_Points[m_Triangles[t * 3 + 1]],
            m_Points[m_Triangles[t * 3 + 2]]);
        // update metadata
        m_Candidates[t] = pair.first;
        m_Errors[t] = pair.second;
        // add triangle to priority queue
        QueuePush(t);
    }

    m_Pending.clear();
}

void Triangulator::Step()
{
    // pop triangle with highest error from priority queue
    const int t = QueuePop();

    const int e0 = t * 3 + 0;
    const int e1 = t * 3 + 1;
    const int e2 = t * 3 + 2;

    const int p0 = m_Triangles[e0];
    const int p1 = m_Triangles[e1];
    const int p2 = m_Triangles[e2];

    const glm::ivec2 a = m_Points[p0];
    const glm::ivec2 b = m_Points[p1];
    const glm::ivec2 c = m_Points[p2];
    const glm::ivec2 p = m_Candidates[t];

    const int pn = AddPoint(p);

    const auto collinear = [](
        const glm::ivec2 p0, const glm::ivec2 p1, const glm::ivec2 p2)
    {
        return (p1.y - p0.y) * (p2.x - p1.x) == (p2.y - p1.y) * (p1.x - p0.x);
    };

    const auto handleCollinear = [this](const int pn, const int a)
    {
        const int a0 = a - a % 3;
        const int al = a0 + (a + 1) % 3;
        const int ar = a0 + (a + 2) % 3;
        const int p0 = m_Triangles[ar];
        const int pr = m_Triangles[a];
        const int pl = m_Triangles[al];
        const int hal = m_Halfedges[al];
        const int har = m_Halfedges[ar];

        const int b = m_Halfedges[a];

        if (b < 0)
        {
            const int t0 = AddTriangle(pn, p0, pr, -1, har, -1, a0);
            const int t1 = AddTriangle(p0, pn, pl, t0, -1, hal, -1);
            Legalize(t0 + 1);
            Legalize(t1 + 2);
            return;
        }

        const int b0 = b - b % 3;
        const int bl = b0 + (b + 2) % 3;
        const int br = b0 + (b + 1) % 3;
        const int p1 = m_Triangles[bl];
        const int hbl = m_Halfedges[bl];
        const int hbr = m_Halfedges[br];

        QueueRemove(b / 3);

        const int t0 = AddTriangle(p0, pr, pn, har, -1, -1, a0);
        const int t1 = AddTriangle(pr, p1, pn, hbr, -1, t0 + 1, b0);
        const int t2 = AddTriangle(p1, pl, pn, hbl, -1, t1 + 1, -1);
        const int t3 = AddTriangle(pl, p0, pn, hal, t0 + 2, t2 + 1, -1);

        Legalize(t0);
        Legalize(t1);
        Legalize(t2);
        Legalize(t3);
    };


    //auto iDot = [](glm::ivec2 v) { return v.x * v.x + v.y * v.y; };
    //int minDis = iDot(p - a);
    //int target = p0;
    //if (const int pbSqr = iDot(p - b); pbSqr <= minDis)
    //{
    //    target = p1;
    //    minDis = pbSqr;
    //}
    //if (const int pcSqr = iDot(p - c); pcSqr <= minDis)
    //{
    //    target = p2;
    //    minDis = pcSqr;
    //}
    //m_MorphTarget.emplace_back(target);

    if (collinear(a, b, p))
    {
        handleCollinear(pn, e0);
    }
    else if (collinear(b, c, p))
    {
        handleCollinear(pn, e1);
    }
    else if (collinear(c, a, p))
    {
        handleCollinear(pn, e2);
    }
    else
    {
        const int h0 = m_Halfedges[e0];
        const int h1 = m_Halfedges[e1];
        const int h2 = m_Halfedges[e2];

        const int t0 = AddTriangle(p0, p1, pn, h0, -1, -1, e0);
        const int t1 = AddTriangle(p1, p2, pn, h1, -1, t0 + 1, -1);
        const int t2 = AddTriangle(p2, p0, pn, h2, t0 + 2, t1 + 1, -1);

        Legalize(t0);
        Legalize(t1);
        Legalize(t2);
    }

    Flush();
}

int Triangulator::AddPoint(const glm::ivec2 point)
{
    const int i = m_Points.size();
    m_Points.push_back(point);
    return i;
}

int Triangulator::AddTriangle(
    const int a, const int b, const int c,
    const int ab, const int bc, const int ca,
    int e)
{
    if (e < 0)
    {
        // new halfedge index
        e = m_Triangles.size();
        // add triangle vertices
        m_Triangles.push_back(a);
        m_Triangles.push_back(b);
        m_Triangles.push_back(c);
        // add triangle halfedges
        m_Halfedges.push_back(ab);
        m_Halfedges.push_back(bc);
        m_Halfedges.push_back(ca);
        // add triangle metadata
        m_Candidates.emplace_back(0);
        m_Errors.push_back(0);
        m_QueueIndexes.push_back(-1);
    }
    else
    {
        // set triangle vertices
        m_Triangles[e + 0] = a;
        m_Triangles[e + 1] = b;
        m_Triangles[e + 2] = c;
        // set triangle halfedges
        m_Halfedges[e + 0] = ab;
        m_Halfedges[e + 1] = bc;
        m_Halfedges[e + 2] = ca;
    }

    // link neighboring halfedges
    if (ab >= 0)
    {
        m_Halfedges[ab] = e + 0;
    }
    if (bc >= 0)
    {
        m_Halfedges[bc] = e + 1;
    }
    if (ca >= 0)
    {
        m_Halfedges[ca] = e + 2;
    }

    // add triangle to pending queue for later rasterization
    const int t = e / 3;
    m_Pending.push_back(t);

    // return first halfedge index
    return e;
}

void Triangulator::Legalize(const int a)
{
    // if the pair of triangles doesn't satisfy the Delaunay condition
    // (p1 is inside the circumcircle of [p0, pl, pr]), flip them,
    // then do the same check/flip recursively for the new pair of triangles
    //
    //           pl                    pl
    //          /||\                  /  \
    //       al/ || \bl            al/    \a
    //        /  ||  \              /      \
    //       /  a||b  \    flip    /___ar___\
    //     p0\   ||   /p1   =>   p0\---bl---/p1
    //        \  ||  /              \      /
    //       ar\ || /br             b\    /br
    //          \||/                  \  /
    //           pr                    pr

    const auto inCircle = [](
        const glm::ivec2 a, const glm::ivec2 b, const glm::ivec2 c,
        const glm::ivec2 p)
    {
        const int64_t dx = a.x - p.x;
        const int64_t dy = a.y - p.y;
        const int64_t ex = b.x - p.x;
        const int64_t ey = b.y - p.y;
        const int64_t fx = c.x - p.x;
        const int64_t fy = c.y - p.y;
        const int64_t ap = dx * dx + dy * dy;
        const int64_t bp = ex * ex + ey * ey;
        const int64_t cp = fx * fx + fy * fy;
        return dx * (ey * cp - bp * fy) - dy * (ex * cp - bp * fx) + ap * (ex * fy - ey * fx) < 0;
    };

    const int b = m_Halfedges[a];

    if (b < 0)
    {
        return;
    }

    const int a0 = a - a % 3;
    const int b0 = b - b % 3;
    const int al = a0 + (a + 1) % 3;
    const int ar = a0 + (a + 2) % 3;
    const int bl = b0 + (b + 2) % 3;
    const int br = b0 + (b + 1) % 3;
    const int p0 = m_Triangles[ar];
    const int pr = m_Triangles[a];
    const int pl = m_Triangles[al];
    const int p1 = m_Triangles[bl];

    if (!inCircle(m_Points[p0], m_Points[pr], m_Points[pl], m_Points[p1]))
    {
        return;
    }

    const int hal = m_Halfedges[al];
    const int har = m_Halfedges[ar];
    const int hbl = m_Halfedges[bl];
    const int hbr = m_Halfedges[br];

    QueueRemove(a / 3);
    QueueRemove(b / 3);

    const int t0 = AddTriangle(p0, p1, pl, -1, hbl, hal, a0);
    const int t1 = AddTriangle(p1, p0, pr, t0, har, hbr, b0);

    Legalize(t0 + 1);
    Legalize(t1 + 2);
}

// priority queue functions

void Triangulator::QueuePush(const int t)
{
    const int i = m_Queue.size();
    m_QueueIndexes[t] = i;
    m_Queue.push_back(t);
    QueueUp(i);
}

int Triangulator::QueuePop()
{
    const int n = m_Queue.size() - 1;
    QueueSwap(0, n);
    QueueDown(0, n);
    return QueuePopBack();
}

int Triangulator::QueuePopBack()
{
    const int t = m_Queue.back();
    m_Queue.pop_back();
    m_QueueIndexes[t] = -1;
    return t;
}

void Triangulator::QueueRemove(const int t)
{
    const int i = m_QueueIndexes[t];
    if (i < 0)
    {
        const auto it = std::find(m_Pending.begin(), m_Pending.end(), t);
        if (it != m_Pending.end())
        {
            std::swap(*it, m_Pending.back());
            m_Pending.pop_back();
        }
        else
        {
            // this shouldn't happen!
        }
        return;
    }
    const int n = m_Queue.size() - 1;
    if (n != i)
    {
        QueueSwap(i, n);
        if (!QueueDown(i, n))
        {
            QueueUp(i);
        }
    }
    QueuePopBack();
}

bool Triangulator::QueueLess(const int i, const int j) const
{
    return -m_Errors[m_Queue[i]] < -m_Errors[m_Queue[j]];
}

void Triangulator::QueueSwap(const int i, const int j)
{
    const int pi = m_Queue[i];
    const int pj = m_Queue[j];
    m_Queue[i] = pj;
    m_Queue[j] = pi;
    m_QueueIndexes[pi] = j;
    m_QueueIndexes[pj] = i;
}

void Triangulator::QueueUp(const int j0)
{
    int j = j0;
    while (1)
    {
        int i = (j - 1) / 2;
        if (i == j || !QueueLess(j, i))
        {
            break;
        }
        QueueSwap(i, j);
        j = i;
    }
}

bool Triangulator::QueueDown(const int i0, const int n)
{
    int i = i0;
    while (1)
    {
        const int j1 = 2 * i + 1;
        if (j1 >= n || j1 < 0)
        {
            break;
        }
        const int j2 = j1 + 1;
        int j = j1;
        if (j2 < n && QueueLess(j2, j1))
        {
            j = j2;
        }
        if (!QueueLess(j, i))
        {
            break;
        }
        QueueSwap(i, j);
        i = j;
    }
    return i > i0;
}
