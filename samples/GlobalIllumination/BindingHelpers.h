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

#include "GFSDK_NVRHI.h"
#include <algorithm>

namespace NVRHI
{
    inline void BindConstantBuffer(PipelineStageBindings &ds, uint32_t slot, ConstantBufferHandle cb)
    {
        uint32_t idx;
        for (idx = 0; idx < ds.constantBufferBindingCount; idx++)
        {
            if (ds.constantBuffers[idx].slot == slot)
                break;
        }
        if (idx == ds.constantBufferBindingCount)
            ds.constantBufferBindingCount++;

        auto &binding = ds.constantBuffers[idx];
        binding.slot = slot;
        binding.buffer = cb;
        if (!cb)
        {
            // remove null
            ds.constantBufferBindingCount--;
            std::swap(binding, ds.constantBuffers[ds.constantBufferBindingCount]);
        }
    }

    inline void BindBuffer(PipelineStageBindings &ds, uint32_t slot, BufferHandle b, bool isWritable = false, Format::Enum fmt = Format::UNKNOWN)
    {
        uint32_t idx;
        for (idx = 0; idx < ds.bufferBindingCount; idx++)
        {
            if (ds.buffers[idx].slot == slot && ds.buffers[idx].isWritable == isWritable)
                break;
        }
        if (idx == ds.bufferBindingCount)
            ds.bufferBindingCount++;

        auto &binding = ds.buffers[idx];
        binding.isWritable = isWritable;
        binding.slot = slot;
        binding.buffer = b;
        binding.format = fmt;
        if (!b)
        {
            // remove null
            ds.bufferBindingCount--;
            std::swap(binding, ds.buffers[ds.bufferBindingCount]);
        }
    }

    inline void BindTexture(PipelineStageBindings &ds, uint32_t slot, TextureHandle t, bool isWritable = false, Format::Enum fmt = Format::UNKNOWN, uint32_t mipLevel = 0)
    {
        uint32_t idx;
        for (idx = 0; idx < ds.textureBindingCount; idx++)
        {
            if (ds.textures[idx].slot == slot && ds.textures[idx].isWritable == isWritable)
                break;
        }
        if (idx == ds.textureBindingCount)
            ds.textureBindingCount++;

        auto &binding = ds.textures[idx];
        binding.isWritable = isWritable;
        binding.slot = slot;
        binding.texture = t;
        binding.format = fmt;
        binding.mipLevel = mipLevel;
        if (!t)
        {
            // remove null
            ds.textureBindingCount--;
            std::swap(binding, ds.textures[ds.textureBindingCount]);
        }
    }

    inline void BindSampler(PipelineStageBindings &ds, uint32_t slot, SamplerHandle s)
    {
        uint32_t idx;
        for (idx = 0; idx < ds.textureSamplerBindingCount; idx++)
        {
            if (ds.textureSamplers[idx].slot == slot)
                break;
        }
        if (idx == ds.textureSamplerBindingCount)
            ds.textureSamplerBindingCount++;

        auto &binding = ds.textureSamplers[idx];
        binding.slot = slot;
        binding.sampler = s;
        if (!s)
        {
            // remove null
            ds.textureSamplerBindingCount--;
            std::swap(binding, ds.textureSamplers[ds.textureSamplerBindingCount]);
        }
    }

    inline void BindTextureAndSampler(PipelineStageBindings &ds, uint32_t slot, TextureHandle t, SamplerHandle s)
    {
        BindTexture(ds, slot, t, false, Format::UNKNOWN, ~0u);

#if USE_GL4
        BindSampler(ds, slot, s);
#else
        (void)s;
#endif
    }

    inline void ClearConstantBuffers(PipelineStageBindings &ds)
    {
        memset(ds.constantBuffers, 0, sizeof(ds.constantBuffers[0]) * ds.constantBufferBindingCount);
        ds.constantBufferBindingCount = 0;
    }

    inline void ClearBuffers(PipelineStageBindings &ds)
    {
        memset(ds.buffers, 0, sizeof(ds.buffers[0]) * ds.bufferBindingCount);
        ds.bufferBindingCount = 0;
    }

    inline void ClearTextures(PipelineStageBindings &ds)
    {
        memset(ds.textures, 0, sizeof(ds.textures[0]) * ds.textureBindingCount);
        ds.textureBindingCount = 0;
    }

    inline void ClearSamplers(PipelineStageBindings &ds)
    {
        memset(ds.textureSamplers, 0, sizeof(ds.textureSamplers[0]) * ds.textureSamplerBindingCount);
        ds.textureSamplerBindingCount = 0;
    }

    inline void BindVertexBuffer(DrawCallState &dcs, uint32_t slot, BufferHandle b, uint32_t stride, uint32_t offset)
    {
        uint32_t idx;
        for (idx = 0; idx < dcs.vertexBufferCount; idx++)
        {
            if (dcs.vertexBuffers[idx].slot == slot)
                break;
        }
        if (idx == dcs.vertexBufferCount)
            dcs.vertexBufferCount++;

        auto &binding = dcs.vertexBuffers[idx];
        binding.slot = slot;
        binding.buffer = b;
        binding.stride = stride;
        binding.offset = offset;

        if (!b)
        {
            // remove null
            dcs.vertexBufferCount--;
            std::swap(binding, dcs.vertexBuffers[dcs.vertexBufferCount]);
        }
    }
}

#if USE_GL4
#define CREATE_SHADER(type, code) m_RendererInterface->createShader(NVRHI::ShaderDesc(NVRHI::ShaderType::SHADER_##type), &code[0], sizeof(code))
#else
#define CREATE_SHADER(type, code) m_RendererInterface->createShader(NVRHI::ShaderDesc(NVRHI::ShaderType::SHADER_##type), &code, sizeof(code))
#endif

#define DESTROY_SHADER(s)                      \
    {                                          \
        m_RendererInterface->destroyShader(s); \
        s = NULL;                              \
    }
#define DESTROY_INPUT_LAYOUT(s)                     \
    {                                               \
        m_RendererInterface->destroyInputLayout(s); \
        s = NULL;                                   \
    }
#define DESTROY_TEXTURE(s)                      \
    {                                           \
        m_RendererInterface->destroyTexture(s); \
        s = NULL;                               \
    }
#define DESTROY_SAMPLER(s)                      \
    {                                           \
        m_RendererInterface->destroySampler(s); \
        s = NULL;                               \
    }
#define DESTROY_CONSTANT_BUFFER(s)                     \
    {                                                  \
        m_RendererInterface->destroyConstantBuffer(s); \
        s = NULL;                                      \
    }
#define DESTROY_BUFFER(s)                      \
    {                                          \
        m_RendererInterface->destroyBuffer(s); \
        s = NULL;                              \
    }
