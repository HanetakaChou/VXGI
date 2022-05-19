#pragma once

#include "Windows.h"
#include <DirectXMath.h>
#include "GFSDK_NVRHI.h"
#include "GFSDK_VXGI_MathTypes.h"
#include <vector>
#include <string>
#include <map>


enum aiTextureType
{
    aiTextureType_NONE = 0x0,
    aiTextureType_DIFFUSE = 0x1,
    aiTextureType_SPECULAR = 0x2,
    aiTextureType_EMISSIVE = 0x4,
    aiTextureType_NORMALS = 0x6,
    aiTextureType_UNKNOWN = 0xC
};

class Scene
{
protected:
    NVRHI::IRendererInterface *m_Renderer;

    std::string m_ScenePath;

    unsigned int m_NumMeshes;

    VXGI::Box3f m_SceneBounds;
    std::vector<VXGI::Box3f> m_MeshBounds;

    std::vector<uint32_t> m_IndexCounts;
    std::vector<uint32_t> m_VertexCounts;

    std::vector<NVRHI::BufferRef> m_IndexBuffers;
    std::vector<NVRHI::BufferRef> m_VertexPositionBuffers;
    std::vector<NVRHI::BufferRef> m_VertexVaryingBuffers;

    std::vector<NVRHI::TextureHandle> m_DiffuseTextures;
    std::vector<NVRHI::TextureHandle> m_SpecularTextures;
    std::vector<NVRHI::TextureHandle> m_NormalsTextures;
    std::vector<NVRHI::TextureHandle> m_EmissiveTextures;

    std::vector<VXGI::float3> m_DiffuseColors;
    std::vector<VXGI::float3> m_SpecularColors;
    std::vector<VXGI::float3> m_EmissiveColors;

    std::map<std::string, NVRHI::TextureHandle> m_LoadedTextures;
    
    NVRHI::TextureHandle LoadTextureFromFile(const char *name, bool force_srgb);

public:
    Scene() : m_Renderer(NULL), m_NumMeshes(0U)
    {
    }

    virtual ~Scene()
    {
        Release();
        ReleaseResources();
    }

    VXGI::float4x4 m_WorldMatrix;

    HRESULT Load(const char *fileName, uint32_t flags = 0);
    HRESULT InitResources(NVRHI::IRendererInterface *pRenderer);
    void Release();
    void ReleaseResources();

    NVRHI::TextureHandle LoadTextureFromFileInternal(const char *name, bool force_srgb);

    const char *GetScenePath() const;

    uint32_t GetMeshesNum() const;

    VXGI::Box3f GetSceneBounds() const;

    NVRHI::BufferHandle GetIndexBuffer(uint32_t meshID) const;
    NVRHI::BufferHandle GetVertexPositionBuffer(uint32_t meshID) const;
    NVRHI::BufferHandle GetVertexVaryingBuffer(uint32_t meshID) const;

    NVRHI::DrawArguments GetMeshDrawArguments(uint32_t meshID) const;

    NVRHI::TextureHandle GetTextureSRV(aiTextureType type, uint32_t meshID) const;
    VXGI::float3 GetColor(aiTextureType type, uint32_t meshID) const;
    int GetMaterialIndex(uint32_t meshID) const;

    VXGI::Box3f GetMeshBounds(uint32_t meshID) const;
};