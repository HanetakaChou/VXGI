/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#pragma once

#include "Windows.h"
#include "assimp/scene.h"
#include "GFSDK_NVRHI.h"
#include "GFSDK_VXGI_MathTypes.h"
#include <vector>
#include <map>

struct VertexBufferEntry
{
    aiVector3D position;
    aiVector2D texCoord;
    aiVector3D normal;
    aiVector3D tangent;
    aiVector3D binormal;
};

class Scene
{
protected:
    aiScene *m_pScene;
    NVRHI::IRendererInterface *m_Renderer;

    std::vector<VXGI::Box3f> m_MeshBounds;
    VXGI::Box3f m_SceneBounds;

    std::string m_ScenePath;

    NVRHI::BufferHandle m_IndexBuffer;
    NVRHI::BufferHandle m_VertexBuffer;

    std::vector<UINT> m_IndexOffsets;
    std::vector<UINT> m_VertexOffsets;

    std::vector<NVRHI::TextureHandle> m_DiffuseTextures;
    std::vector<NVRHI::TextureHandle> m_SpecularTextures;
    std::vector<NVRHI::TextureHandle> m_NormalsTextures;
    std::vector<NVRHI::TextureHandle> m_OpacityTextures;
    std::vector<NVRHI::TextureHandle> m_EmissiveTextures;

    std::vector<VXGI::Vector3f> m_DiffuseColors;
    std::vector<VXGI::Vector3f> m_SpecularColors;
    std::vector<VXGI::Vector3f> m_EmissiveColors;

    std::vector<UINT> m_MeshToSceneMapping;
    std::map<std::string, NVRHI::TextureHandle> m_LoadedTextures;

    NVRHI::TextureHandle LoadTextureFromFile(const char *name);

public:
    Scene()
        : m_pScene(NULL), m_Renderer(NULL)
    {
    }
    virtual ~Scene()
    {
        Release();
        ReleaseResources();
    }

    HRESULT Load(const char *fileName, UINT flags = 0);
    HRESULT InitResources(NVRHI::IRendererInterface *pRenderer);
    void UpdateBounds();
    void Release();
    void ReleaseResources();

    const char *GetScenePath() { return m_ScenePath.c_str(); }

    UINT GetMeshesNum() const { return m_pScene ? m_pScene->mNumMeshes : 0; }

    VXGI::Box3f GetSceneBounds() const;

    NVRHI::BufferHandle GetIndexBuffer() const { return m_IndexBuffer; }
    NVRHI::BufferHandle GetVertexBuffer() const { return m_VertexBuffer; }

    NVRHI::DrawArguments GetMeshDrawArguments(UINT meshID) const;

    NVRHI::TextureHandle GetTextureSRV(aiTextureType type, UINT meshID) const;
    VXGI::Vector3f GetColor(aiTextureType type, UINT meshID) const;
    int GetMaterialIndex(UINT meshID) const;

    UINT GetMeshIndicesNum(UINT meshID) const;
    VXGI::Box3f GetMeshBounds(UINT meshID) const;
};