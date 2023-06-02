#pragma once

#include <nlohmann/json.hpp>
#include <fstream>

class BoundTree
{
public:
    struct Bound
    {
        float HMin = std::numeric_limits<float>::max();
        float HMax = std::numeric_limits<float>::min();
        float AreaHeight = 0.0f;
        float AreaWidth = 0.0f;
        int AreaX = -1;
        int AreaY = -1;
        int PatchIdx = -1;
    };

    BoundTree(const std::vector<std::pair<float, float>>& patchBounds, int patchNx);
    BoundTree(const nlohmann::json& json);
    ~BoundTree() = default;

    void SaveJson(const std::filesystem::path& path) const;

    friend class TerrainSystem;

private:
    struct Node
    {
        std::unique_ptr<Node> m_Children[4] {};
        Bound m_Bound { std::numeric_limits<float>::max(), std::numeric_limits<float>::min(), -1 };

        Node(
            const Bound& b,
            std::unique_ptr<Node> child0 = nullptr, std::unique_ptr<Node> child1 = nullptr,
            std::unique_ptr<Node> child2 = nullptr, std::unique_ptr<Node> child3 = nullptr) :
            m_Bound(b)
        {
            m_Children[0] = std::move(child0);
            m_Children[1] = std::move(child1);
            m_Children[2] = std::move(child2);
            m_Children[3] = std::move(child3);
        }
    };

    std::unique_ptr<Node> m_Root;
};

inline BoundTree::BoundTree(const std::vector<std::pair<float, float>>& patchBounds, int patchNx)
{
    constexpr float PATCH_SIZE = 255.0f;

    std::function<std::unique_ptr<Node>(const std::vector<Bound>&, int, int, int)> recursiveBuild =
        [&recursiveBuild](
        const std::vector<Bound>& bds, const int w, const int xStart, const int yStart) -> std::unique_ptr<Node>
    {
        if (bds.empty()) return nullptr;
        const int h = bds.size() / w;
        if (w == 1 && h == 1)
            return std::make_unique<Node>(bds[0]);

        const int childH = h / 2;
        const int childW = w / 2;
        std::vector<Bound> lt, rt, lb, rb;
        std::unique_ptr<Node> childNodes[4];
        for (int i = 0; i < bds.size(); ++i)
        {
            const int x = i % w;
            const int y = i / w;
            if (x < childW && y < childH)
                lt.push_back(bds[i]);
            else if (x >= childW && y < childH)
                rt.push_back(bds[i]);
            else if (x < childW && y >= childH)
                lb.push_back(bds[i]);
            else rb.push_back(bds[i]);
        }
        childNodes[0] = recursiveBuild(lt, childW, xStart, yStart);
        childNodes[1] = recursiveBuild(rt, w - childW, xStart + childW, yStart);
        childNodes[2] = recursiveBuild(lb, childW, xStart, yStart + childH);
        childNodes[3] = recursiveBuild(rb, w - childW, xStart + childW, yStart + childH);
        float min = std::numeric_limits<float>::max();
        float max = std::numeric_limits<float>::min();
        for (const auto& node : childNodes)
        {
            if (node)
            {
                min = std::min(min, node->m_Bound.HMin);
                max = std::max(max, node->m_Bound.HMax);
            }
        }
        Bound b;
        b.HMin = min;
        b.HMax = max;
        b.AreaX = xStart;
        b.AreaY = yStart;
        b.AreaWidth = w * PATCH_SIZE;
        b.AreaHeight = h * PATCH_SIZE;

        return std::make_unique<Node>(b,
            std::move(childNodes[0]), std::move(childNodes[1]),
            std::move(childNodes[2]), std::move(childNodes[3]));
    };

    std::vector<Bound> bounds;
    bounds.reserve(patchBounds.size());
    for (int i = 0; i < patchBounds.size(); ++i)
    {
        Bound b;
        b.HMin = patchBounds[i].first;
        b.HMax = patchBounds[i].second;
        b.PatchIdx = i;
        b.AreaX = i % patchNx;
        b.AreaY = i / patchNx;
        b.AreaWidth = PATCH_SIZE;
        b.AreaHeight = PATCH_SIZE;
        bounds.emplace_back(b);
    }
    m_Root = recursiveBuild(bounds, patchNx, 0, 0);
}

inline BoundTree::BoundTree(const nlohmann::json& json)
{
    std::function<std::unique_ptr<Node>(const nlohmann::json&)> recursiveBuild =
        [&recursiveBuild](const nlohmann::json& j) -> std::unique_ptr<Node>
    {
        if (j.is_null()) return nullptr;

        std::unique_ptr<Node> children[4];
        for (int i = 0; i < 4; ++i)
            children[i] = recursiveBuild(j["children"][i]);

        Bound b;
        b.HMin = j["min"].get<float>();
        b.HMax = j["max"].get<float>();
        b.PatchIdx = j["pid"].get<int>();
        b.AreaX = j["x"].get<int>();
        b.AreaY = j["y"].get<int>();
        b.AreaWidth = j["w"].get<float>();
        b.AreaHeight = j["h"].get<float>();
        return std::make_unique<Node>(b,
            std::move(children[0]), std::move(children[1]),
            std::move(children[2]), std::move(children[3]));
    };
    m_Root = recursiveBuild(json);
}

inline void BoundTree::SaveJson(const std::filesystem::path& path) const
{
    std::function<void(const Node*, nlohmann::json&)> recursiveSave =
        [&recursiveSave](const Node* node, nlohmann::json& j)
    {
        if (!node) return;
        j["min"] = node->m_Bound.HMin;
        j["max"] = node->m_Bound.HMax;
        j["x"] = node->m_Bound.AreaX;
        j["y"] = node->m_Bound.AreaY;
        j["w"] = node->m_Bound.AreaWidth;
        j["h"] = node->m_Bound.AreaHeight;
        j["pid"] = node->m_Bound.PatchIdx;
        for (int i = 0; i < 4; ++i)
        {
            if (node->m_Children[i])
            {
                j["children"][i] = nlohmann::json();
                recursiveSave(node->m_Children[i].get(), j["children"][i]);
            }
            else
            {
                j["children"][i] = nullptr;
            }
        }
    };

    nlohmann::json j;
    recursiveSave(m_Root.get(), j);

    std::ofstream ofs(path);
    ofs << std::setw(4) << j;
}
