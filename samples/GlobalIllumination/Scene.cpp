/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "Scene.h"
#include <assert.h>

__declspec(align(16)) struct MaterialConstants
{
    VXGI::Vector4f g_BaseColor;
    float g_Metallic;
    float g_Roughness;
};

HRESULT Scene::Load(const char *fileName)
{
    if (0 == std::strcmp(fileName, "Sponza\\SponzaNoFlag.obj"))
    {
        // Cornel Box
#include "../assets/cornell_box.h"

        m_numMeshes = g_cornell_box_mesh_count;

        // Update Bounds
        {
            float const maxFloat = 3.402823466e+38F;
            VXGI::Vector3f _minBoundary(maxFloat, maxFloat, maxFloat);
            VXGI::Vector3f _maxBoundary(-maxFloat, -maxFloat, -maxFloat);

            m_MeshBounds.resize(m_numMeshes);

            m_SceneBounds.lower = _minBoundary;
            m_SceneBounds.upper = _maxBoundary;

            for (UINT meshID = 0; meshID < m_numMeshes; ++meshID)
            {
                VXGI::Vector3f minBoundary = _minBoundary;
                VXGI::Vector3f maxBoundary = _maxBoundary;

                for (UINT v = 0; v < g_cornell_box_mesh_vertex_count[meshID]; ++v)
                {
                    minBoundary.x = __min(minBoundary.x, g_cornell_box_mesh_vertex_position[meshID][3 * v] * 100.0F);
                    minBoundary.y = __min(minBoundary.y, g_cornell_box_mesh_vertex_position[meshID][3 * v + 2] * 100.0F);
                    minBoundary.z = __min(minBoundary.z, g_cornell_box_mesh_vertex_position[meshID][3 * v + 1] * 100.0F);

                    maxBoundary.x = __max(maxBoundary.x, g_cornell_box_mesh_vertex_position[meshID][3 * v] * 100.0F);
                    maxBoundary.y = __max(maxBoundary.y, g_cornell_box_mesh_vertex_position[meshID][3 * v + 2] * 100.0F);
                    maxBoundary.z = __max(maxBoundary.z, g_cornell_box_mesh_vertex_position[meshID][3 * v + 1] * 100.0F);
                }

                m_MeshBounds[meshID].lower = minBoundary;
                m_MeshBounds[meshID].upper = maxBoundary;

                m_SceneBounds.lower.x = __min(m_SceneBounds.lower.x, minBoundary.x);
                m_SceneBounds.lower.y = __min(m_SceneBounds.lower.y, minBoundary.y);
                m_SceneBounds.lower.z = __min(m_SceneBounds.lower.z, minBoundary.z);

                m_SceneBounds.upper.x = __max(m_SceneBounds.upper.x, maxBoundary.x);
                m_SceneBounds.upper.y = __max(m_SceneBounds.upper.y, maxBoundary.y);
                m_SceneBounds.upper.z = __max(m_SceneBounds.upper.z, maxBoundary.z);
            }
        }

        // Buffers
        {
            m_IndexCounts.resize(m_numMeshes);
            m_IndexOffsets.resize(m_numMeshes);
            m_VertexOffsets.resize(m_numMeshes);

            UINT totalIndices = 0;
            UINT totalVertices = 0;

            for (uint32_t meshID = 0; meshID < m_numMeshes; ++meshID)
            {
                m_IndexCounts[meshID] = g_cornell_box_mesh_index_count[meshID];
                m_IndexOffsets[meshID] = totalIndices;
                m_VertexOffsets[meshID] = totalVertices;

                totalIndices += g_cornell_box_mesh_index_count[meshID];
                totalVertices += g_cornell_box_mesh_vertex_count[meshID];
            }

            m_Indices.resize(totalIndices);
            m_Vertices.resize(totalVertices);

            for (UINT meshID = 0; meshID < m_numMeshes; ++meshID)
            {
                UINT indexOffset = m_IndexOffsets[meshID];
                UINT vertexOffset = m_VertexOffsets[meshID];

                UINT numIndices = g_cornell_box_mesh_index_count[meshID];
                UINT numVertices = g_cornell_box_mesh_vertex_count[meshID];

                for (UINT i = 0; i < numIndices; ++i)
                {
                    m_Indices[indexOffset + i] = g_cornell_box_mesh_index[meshID][i];
                }

                for (UINT v = 0; v < numVertices; ++v)
                {
                    m_Vertices[vertexOffset + v].position.x = g_cornell_box_mesh_vertex_position[meshID][3 * v] * 100.0F;
                    m_Vertices[vertexOffset + v].position.y = g_cornell_box_mesh_vertex_position[meshID][3 * v + 2] * 100.0F;
                    m_Vertices[vertexOffset + v].position.z = g_cornell_box_mesh_vertex_position[meshID][3 * v + 1] * 100.0F;

                    m_Vertices[vertexOffset + v].normal.x = g_cornell_box_mesh_vertex_normal[meshID][3 * v];
                    m_Vertices[vertexOffset + v].normal.y = g_cornell_box_mesh_vertex_normal[meshID][3 * v + 2];
                    m_Vertices[vertexOffset + v].normal.z = g_cornell_box_mesh_vertex_normal[meshID][3 * v + 1];
                }
            }
        }

        // Materials
        {
            m_BaseColors.resize(m_numMeshes);
            m_MetallicValues.resize(m_numMeshes);
            m_RoughnessValues.resize(m_numMeshes);

            for (UINT meshID = 0; meshID < m_numMeshes; ++meshID)
            {
                m_BaseColors[meshID].x = g_cornell_box_mesh_base_color[meshID][0];
                m_BaseColors[meshID].y = g_cornell_box_mesh_base_color[meshID][1];
                m_BaseColors[meshID].z = g_cornell_box_mesh_base_color[meshID][2];

                m_MetallicValues[meshID] = g_cornell_box_mesh_metallic[meshID];

                m_RoughnessValues[meshID] = g_cornell_box_mesh_roughness[meshID];
            }
        }

        return S_OK;
    }
    else
    {
        return E_FAIL;
    }
}

void Scene::Release()
{
    printf("Releasing the scene...\n");
}

HRESULT Scene::InitResources(NVRHI::IRendererInterface *pRenderer)
{
    m_Renderer = pRenderer;

    if (m_numMeshes > 0U)
    {
        NVRHI::BufferDesc indexBufferDesc;
        indexBufferDesc.isIndexBuffer = true;
        indexBufferDesc.byteSize = sizeof(UINT) * static_cast<uint32_t>(m_Indices.size());
        m_IndexBuffer = m_Renderer->createBuffer(indexBufferDesc, &m_Indices[0]);

        NVRHI::BufferDesc vertexBufferDesc;
        vertexBufferDesc.isVertexBuffer = true;
        vertexBufferDesc.byteSize = sizeof(VertexBufferEntry) * static_cast<uint32_t>(m_Vertices.size());
        m_VertexBuffer = m_Renderer->createBuffer(vertexBufferDesc, &m_Vertices[0]);

        assert(m_MaterialBuffers.empty());
        m_MaterialBuffers.resize(m_numMeshes);

        for (UINT meshID = 0; meshID < m_numMeshes; ++meshID)
        {
            MaterialConstants materialData;
            materialData.g_BaseColor = VXGI::Vector4f(m_BaseColors[meshID].x, m_BaseColors[meshID].y, m_BaseColors[meshID].z, 1.0F);
            materialData.g_Metallic = m_MetallicValues[meshID];
            materialData.g_Roughness = m_RoughnessValues[meshID];

            NVRHI::ConstantBufferDesc materialBufferDesc(sizeof(MaterialConstants), NULL);
            m_MaterialBuffers[meshID] = m_Renderer->createConstantBuffer(materialBufferDesc, &materialData);
        }
    }

    return S_OK;
}

void Scene::ReleaseResources()
{
    m_Renderer->destroyBuffer(m_IndexBuffer);
    m_Renderer->destroyBuffer(m_VertexBuffer);

    for (NVRHI::ConstantBufferHandle &MaterialBuffer : m_MaterialBuffers)
    {
        m_Renderer->destroyConstantBuffer(MaterialBuffer);
        MaterialBuffer = NULL;
    }
}

NVRHI::DrawArguments Scene::GetMeshDrawArguments(UINT meshID) const
{
    NVRHI::DrawArguments args;

    args.vertexCount = m_IndexCounts[meshID];
    args.startIndexLocation = m_IndexOffsets[meshID];
    args.startVertexLocation = m_VertexOffsets[meshID];

    return args;
}

NVRHI::ConstantBufferHandle Scene::GetMaterialBuffer(UINT meshID) const
{
    return m_MaterialBuffers[meshID];
}

VXGI::Box3f Scene::GetSceneBounds() const
{
    assert(!m_MeshBounds.empty());

    return m_SceneBounds;
}

VXGI::Box3f Scene::GetMeshBounds(UINT meshID) const
{
    assert(meshID < m_MeshBounds.size());

    return m_MeshBounds[meshID];
}
