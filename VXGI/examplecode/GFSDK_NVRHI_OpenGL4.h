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

#include <GFSDK_NVRHI.h>

#include <vector>
#include <map>

namespace NVRHI
{
    class FrameBuffer;

    class RendererInterfaceOGL : public IRendererInterface
    {
    public:
        RendererInterfaceOGL(IErrorCallback *pErrorCallback);
        ~RendererInterfaceOGL();

        void init();

        void *getAPISpecificInterface(APISpecificInterface::Enum) override { return nullptr; }
        bool isOpenGLExtensionSupported(const char *name) override;
        void *getOpenGLProcAddress(const char *procname) override;

        TextureHandle createTexture(const TextureDesc &d, const void *data) override;
        TextureDesc describeTexture(TextureHandle t) override;
        void clearTextureFloat(TextureHandle t, const Color &clearColor) override;
        void clearTextureUInt(TextureHandle t, uint32_t clearColor) override;
        void writeTexture(TextureHandle t, uint32_t subresource, const void *data, uint32_t rowPitch, uint32_t depthPitch) override;
        void destroyTexture(TextureHandle t) override;

        BufferHandle createBuffer(const BufferDesc &d, const void *data) override;
        void writeBuffer(BufferHandle b, const void *data, size_t dataSize) override;
        void clearBufferUInt(BufferHandle b, uint32_t clearValue) override;
        void copyToBuffer(BufferHandle dest, uint32_t destOffsetBytes, BufferHandle src, uint32_t srcOffsetBytes, size_t dataSizeBytes) override;
        void readBuffer(BufferHandle b, void *data, size_t *dataSize) override; // for debugging purposes only
        void destroyBuffer(BufferHandle b) override;

        ConstantBufferHandle createConstantBuffer(const ConstantBufferDesc &d, const void *data) override;
        void writeConstantBuffer(ConstantBufferHandle b, const void *data, size_t dataSize) override;
        void destroyConstantBuffer(ConstantBufferHandle b) override;

        ShaderHandle createShader(const ShaderDesc &d, const void *binary, const size_t binarySize) override;
        ShaderHandle createShaderFromAPIInterface(ShaderType::Enum, const void *) override { return nullptr; }
        void destroyShader(ShaderHandle s) override;

        SamplerHandle createSampler(const SamplerDesc &d) override;
        void destroySampler(SamplerHandle s) override;

        InputLayoutHandle createInputLayout(const VertexAttributeDesc *d, uint32_t attributeCount, const void *vertexShaderBinary, const size_t binarySize) override;
        void destroyInputLayout(InputLayoutHandle i) override;

        PerformanceQueryHandle createPerformanceQuery(const char *) override { return nullptr; }
        void destroyPerformanceQuery(PerformanceQueryHandle) override {}
        void beginPerformanceQuery(PerformanceQueryHandle, bool) override {}
        void endPerformanceQuery(PerformanceQueryHandle) override {}
        float getPerformanceQueryTimeMS(PerformanceQueryHandle) override { return 0.f; }

        GraphicsAPI::Enum getGraphicsAPI() override { return GraphicsAPI::OPENGL4; };

        void draw(const DrawCallState &state, const DrawArguments *args, uint32_t numDrawCalls) override;
        void drawIndexed(const DrawCallState &state, const DrawArguments *args, uint32_t numDrawCalls) override;
        void drawIndirect(const DrawCallState &state, BufferHandle indirectParams, uint32_t offsetBytes) override;

        void dispatch(const DispatchState &state, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
        void dispatchIndirect(const DispatchState &state, BufferHandle indirectParams, uint32_t offsetBytes) override;

        void executeRenderThreadCommand(IRenderThreadCommand *onCommand) override;

        uint32_t getNumberOfAFRGroups() override { return 1; }
        uint32_t getAFRGroupOfCurrentFrame(uint32_t) override { return 0; }
        void setEnableUavBarriersForTexture(TextureHandle, bool) override {}
        void setEnableUavBarriersForBuffer(BufferHandle, bool) override {}

        void ApplyState(const DrawCallState &state);
        void RestoreDefaultState();
        void UnbindFrameBuffer();

        TextureHandle getHandleForDefaultBackBuffer() { return m_DefaultBackBuffer; }
        TextureHandle getHandleForTexture(uint32_t target, uint32_t texture);
        uint32_t getTextureOpenGLName(TextureHandle t);
        void releaseNonManagedTextures();

        void debugBeginEvent(char const *name);
        void debugEndEvent();

    protected:
        IErrorCallback *m_pErrorCallback;

        uint32_t m_nGraphicsPipeline;
        uint32_t m_nComputePipeline;
        uint32_t m_nVAO;

        // state cache
        std::vector<uint32_t> m_vecBoundSamplers;
        std::vector<uint32_t> m_vecBoundConstantBuffers;
        std::vector<uint32_t> m_vecBoundBuffers;
        std::vector<uint32_t> m_vecBoundImages;
        std::vector<std::pair<uint32_t, uint32_t>> m_vecBoundTextures;
        bool m_bConservativeRasterEnabled;
        bool m_bForcedSampleCountEnabled;

        std::map<uint32_t, FrameBuffer *> m_CachedFrameBuffers;
        std::vector<TextureHandle> m_NonManagedTextures;
        TextureHandle m_DefaultBackBuffer;
        FrameBuffer *m_pCurrentFrameBuffer;
        NVRHI::Viewport m_vCurrentViewports[16];
        NVRHI::Rect m_vCurrentScissorRects[16];
        bool m_bCurrentViewportsValid;

        FrameBuffer *GetCachedFrameBuffer(const RenderState &state);

        void BindVAO();
        void BindRenderTargets(const RenderState &renderState);
        void ClearRenderTargets(const RenderState &renderState);
        void SetRasterState(const RasterState &rasterState);
        void SetBlendState(const BlendState &blendState, uint32_t targetCount);
        void SetDepthStencilState(const DepthStencilState &depthState);
        void SetShaders(const DrawCallState &state);
        void BindShaderResources(const PipelineStageBindings &state);
        void BindShaderResources(const DrawCallState &state);

        void ApplyState(const DispatchState &state);

        void checkGLError(const char *file, int line);

        uint32_t convertStencilOp(DepthStencilState::StencilOp value);
        uint32_t convertComparisonFunc(DepthStencilState::ComparisonFunc value);
        uint32_t convertPrimType(PrimitiveType::Enum primType);
        uint32_t convertWrapMode(SamplerDesc::WrapMode in_eMode);
        uint32_t convertBlendValue(BlendState::BlendValue value);
        uint32_t convertBlendOp(BlendState::BlendOp value);
    };
}
