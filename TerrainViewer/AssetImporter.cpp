#define NOMINMAX
#include "AssetImporter.h"

#include <iostream>
#include <regex>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace DirectX::SimpleMath;

namespace
{
    Matrix AiMatrixToMatrix(const aiMatrix4x4& aiMat)
    {
        return Matrix(
            aiMat.a1, aiMat.a2, aiMat.a3, aiMat.a4,
            aiMat.b1, aiMat.b2, aiMat.b3, aiMat.b4,
            aiMat.c1, aiMat.c2, aiMat.c3, aiMat.c4,
            aiMat.d1, aiMat.d2, aiMat.d3, aiMat.d4).Transpose();
    }
}

AssetImporter::ImporterModelData AssetImporter::LoadAsset(const std::filesystem::path& fPath, bool yToZ)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fPath.string().c_str(),
        aiProcessPreset_TargetRealtime_Fast | aiProcess_FlipUVs | aiProcess_GenBoundingBoxes |
        (yToZ ? aiProcess_FlipWindingOrder : 0));

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        throw std::runtime_error(importer.GetErrorString());
    }

    using Vertex = VertexPositionNormalTangentTexture;
    ImporterModelData model {};

    std::function<void(const aiNode*, Matrix)> traverse =
        [scene, &model, &traverse, yToZ](const aiNode* node, Matrix t)
    {
        t                  = AiMatrixToMatrix(node->mTransformation) * t;
        const auto meshCnt = node->mNumMeshes;
        for (uint32_t i = 0; i < meshCnt; ++i)
        {
            const auto mesh    = scene->mMeshes[node->mMeshes[i]];
            const auto vertCnt = mesh->mNumVertices;
            if (mesh->mTextureCoords[0] == nullptr)
            {
                continue;
            }
            std::vector<Vertex> vb;
            for (uint32_t j = 0; j < vertCnt; ++j)
            {
                const auto& pos = mesh->mVertices[j];
                const auto& nor = mesh->mNormals[j];
                const auto& tan = mesh->mTangents[j];
                const auto& tc  = mesh->mTextureCoords[0][j];

                Vector3 tPos = Vector3::Transform(yToZ ? Vector3(pos.x, pos.z, pos.y) : Vector3(pos.x, pos.y, pos.z), t);
                Vector3 tNor = Vector3::TransformNormal(yToZ ? Vector3(nor.x, nor.z, nor.y) : Vector3(nor.x, nor.y, nor.z), t);
                tNor.Normalize();
                vb.emplace_back(
                    tPos,
                    tNor,
                    Vector3(tan.x, tan.y, tan.z),
                    Vector2(tc.x, tc.y));
            }
            std::vector<uint32_t> ib;
            const auto faceCnt = mesh->mNumFaces;
            for (uint32_t j = 0; j < faceCnt; ++j)
            {
                const aiFace& face = mesh->mFaces[j];
                for (uint32_t k = 0; k < 3; ++k)
                {
                    ib.push_back(face.mIndices[k]);
                }
                model.AreaSum += 0.5f * (vb[ib[j * 3]].Pos - vb[ib[j * 3 + 2]].Pos).Cross(vb[ib[j * 3 + 1]].Pos - vb[ib[j * 3 + 2]].Pos).Length();
            }
            const auto matIndex = mesh->mMaterialIndex;
            model.Subsets.emplace_back(std::move(vb), std::move(ib), matIndex);
            auto minCorner = Vector3(mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z);
            auto maxCorner = Vector3(mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z);
            if (yToZ)
            {
                std::swap(minCorner.y, minCorner.z);
                std::swap(maxCorner.y, maxCorner.z);
            }
            model.BoundingBox = DirectX::BoundingBox((minCorner + maxCorner) * 0.5, (maxCorner - minCorner) * 0.5);
        }
        const auto childCnt = node->mNumChildren;
        for (uint32_t i = 0; i < childCnt; ++i)
        {
            traverse(node->mChildren[i], t);
        }
    };

    traverse(scene->mRootNode, Matrix::Identity);

    const auto matCnt = scene->mNumMaterials;
    for (uint32_t i = 0; i < matCnt; ++i)
    {
        const auto material = scene->mMaterials[i];
        aiString tex;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &tex);
        auto name = material->GetName();
        auto path = std::filesystem::path(tex.C_Str());
        model.Materials.emplace_back(name.C_Str(), fPath.parent_path() / std::filesystem::path(tex.C_Str()));
    }

    return model;
}
