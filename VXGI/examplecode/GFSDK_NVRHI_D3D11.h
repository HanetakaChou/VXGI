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

#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <GFSDK_NVRHI.h>
#include <sdkddkver.h>
#include <Windows.h>
#include <d3d11_1.h>
#include <wrl.h>
#include <map>
#include <vector>
#include <set>
#include <string>

namespace NVRHI
{
  using namespace Microsoft::WRL;

  struct StageMask
  {
    enum Enum
    {
      DENY_SHADER_VERTEX = 0x1 << ShaderType::SHADER_VERTEX,
      DENY_SHADER_HULL = 0x1 << ShaderType::SHADER_HULL,
      DENY_SHADER_DOMAIN = 0x1 << ShaderType::SHADER_DOMAIN,
      DENY_SHADER_GEOMETRY = 0x1 << ShaderType::SHADER_GEOMETRY,
      DENY_SHADER_PIXEL = 0x1 << ShaderType::SHADER_PIXEL,
      DENY_INPUT_STATE = 0x20,
      DENY_RENDER_STATE = 0x40
    };
  };

  class RendererInterfaceD3D11 : public IRendererInterface
  {
  public:
    // The user-visible API
    RendererInterfaceD3D11(IErrorCallback *errorCB, ID3D11DeviceContext *context);
    virtual ~RendererInterfaceD3D11();

    void forgetAboutTexture(ID3D11Resource *resource);
    void forgetAboutBuffer(ID3D11Buffer *resource);

    // You should not call this while the client is initialized or else resources it is using may be deleted
    void clearCachedData();

    inline ID3D11DeviceContext *GetDeviceContext() const { return context.Get(); }
    inline ID3D11Device *GetDevice() const { return device.Get(); }

    // These are for convenience. They also do not alter the reference count of the returned value
    inline ID3D11ShaderResourceView *getSRVForTexture(ID3D11Resource *resource, DXGI_FORMAT format, uint32_t mipLevel = 0)
    {
      return getSRVForTexture(getHandleForTexture(resource, NULL), format, mipLevel);
    }
    inline ID3D11RenderTargetView *getRTVForTexture(ID3D11Resource *resource, uint32_t arrayItem = 0)
    {
      return getRTVForTexture(getHandleForTexture(resource, NULL), arrayItem);
    }
    inline ID3D11DepthStencilView *getDSVForTexture(ID3D11Resource *resource, uint32_t arrayItem = 0)
    {
      return getDSVForTexture(getHandleForTexture(resource, NULL), arrayItem);
    }
    inline ID3D11UnorderedAccessView *getUAVForTexture(ID3D11Resource *resource, DXGI_FORMAT format, uint32_t mipLevel = 0)
    {
      return getUAVForTexture(getHandleForTexture(resource, NULL), format, mipLevel);
    }

    ID3D11ShaderResourceView *getSRVForTexture(TextureHandle handle, uint32_t mipLevel = 0);
    ID3D11RenderTargetView *getRTVForTexture(TextureHandle handle, uint32_t arrayItem = 0);
    ID3D11DepthStencilView *getDSVForTexture(TextureHandle handle, uint32_t arrayItem = 0, uint32_t mipLevel = 0);
    ID3D11UnorderedAccessView *getUAVForTexture(TextureHandle handle, uint32_t mipLevel = 0);

    TextureHandle getHandleForTexture(ID3D11Resource *resource)
    {
      return (TextureHandle)getHandleForTexture(resource, NULL);
    }
    BufferHandle getHandleForBuffer(ID3D11Buffer *resource)
    {
      return (BufferHandle)getHandleForBuffer(resource, NULL);
    }

  private:
    RendererInterfaceD3D11 &operator=(const RendererInterfaceD3D11 &other); // undefined
  protected:
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Device> device;
    IErrorCallback *errorCB;
    bool nvapiIsInitalized;
    ComPtr<ID3DUserDefinedAnnotation> userDefinedAnnotation;

    void signalError(const char *file, int line, const char *errorDesc);

    // Store the views we have created for a particular resource. Most of these will be null at a given time
    struct TextureViewSet
    {
      // cache this here for simplicity.
      TextureDesc textureDesc;
      std::map<std::pair<DXGI_FORMAT, uint32_t>, ComPtr<ID3D11ShaderResourceView>> shaderResourceViews;
      std::map<std::pair<uint32_t, uint32_t>, ComPtr<ID3D11RenderTargetView>> renderTargetViews;
      std::map<std::pair<uint32_t, uint32_t>, ComPtr<ID3D11DepthStencilView>> depthStencilViews;
      std::vector<std::map<DXGI_FORMAT, ComPtr<ID3D11UnorderedAccessView>>> unorderedAccessViewsPerMip;
    };

    // We store these in a map so that we can map a raw D3D pointer back to the struct (eg if they from the user's code)
    typedef std::map<ComPtr<ID3D11Resource>, TextureViewSet> TextureObjectMap;
    TextureObjectMap textures;

    // We only have single views here since you can't really alias different formats and we don't support partial views
    struct BufferViewSet
    {
      // cache this here for simplicity. Only one is used depending on if this is a texture or a buffer
      BufferDesc bufferDesc;
      ComPtr<ID3D11Buffer> stagingBuffer;
      ComPtr<ID3D11ShaderResourceView> shaderResourceView;
      ComPtr<ID3D11UnorderedAccessView> unorderedAccessView;
    };

    typedef std::map<ComPtr<ID3D11Buffer>, BufferViewSet> BufferObjectMap;
    BufferObjectMap buffers;

    std::map<uint32_t, ComPtr<ID3D11BlendState>> blendStates;
    std::map<uint32_t, ComPtr<ID3D11DepthStencilState>> depthStencilStates;
    std::map<uint32_t, ComPtr<ID3D11RasterizerState>> rasterizerStates;

    std::set<PerformanceQueryHandle> perfQueries;

    D3D11_BLEND convertBlendValue(BlendState::BlendValue value);
    D3D11_BLEND_OP convertBlendOp(BlendState::BlendOp value);
    D3D11_STENCIL_OP convertStencilOp(DepthStencilState::StencilOp value);
    D3D11_COMPARISON_FUNC convertComparisonFunc(DepthStencilState::ComparisonFunc value);
    ID3D11BlendState *getBlendState(const BlendState &blendState);
    ID3D11DepthStencilState *getDepthStencilState(const DepthStencilState &depthStencilState);
    ID3D11RasterizerState *getRasterizerState(const RasterState &rasterState);

    ID3D11ShaderResourceView *getSRVForTexture(TextureObjectMap::value_type *resource, DXGI_FORMAT format, uint32_t mipLevel = 0);
    ID3D11RenderTargetView *getRTVForTexture(TextureObjectMap::value_type *resource, uint32_t arrayItem = 0, uint32_t mipLevel = 0);
    ID3D11DepthStencilView *getDSVForTexture(TextureObjectMap::value_type *resource, uint32_t arrayItem = 0, uint32_t mipLevel = 0);
    ID3D11UnorderedAccessView *getUAVForTexture(TextureObjectMap::value_type *resource, DXGI_FORMAT format, uint32_t mipLevel = 0);
    // if there are multiple views to clear you can keep calling index until you get all null
    void getClearViewForTexture(TextureObjectMap::value_type *resource, uint32_t index, bool asUINT, ID3D11UnorderedAccessView *&outUAV, ID3D11RenderTargetView *&outRTV, ID3D11DepthStencilView *&outDSV);

    ID3D11ShaderResourceView *getSRVForBuffer(BufferObjectMap::value_type *resource, Format::Enum format);
    ID3D11UnorderedAccessView *getUAVForBuffer(BufferObjectMap::value_type *resource);

    // If we just created this texture pass in the texture desc, otherwise deduce it from D3D11
    TextureObjectMap::value_type *getHandleForTexture(ID3D11Resource *resource, const TextureDesc *textureDesc);
    BufferObjectMap::value_type *getHandleForBuffer(ID3D11Buffer *resource, const BufferDesc *bufferDesc);

    TextureDesc getTextureDescFromD3D11Resource(ID3D11Resource *resource);
    BufferDesc getBufferDescFromD3D11Buffer(ID3D11Buffer *buffer);

    D3D_PRIMITIVE_TOPOLOGY getPrimType(PrimitiveType::Enum pt);

    void disableSLIResouceSync(ID3D11Resource *resource);

  public:
    // These are the methods in the in the IRendererInteface inteface that are implemented by us

    virtual TextureHandle createTexture(const TextureDesc &d, const void *data);
    virtual TextureDesc describeTexture(TextureHandle t);
    virtual void clearTextureFloat(TextureHandle t, const Color &clearColor);
    virtual void clearTextureUInt(TextureHandle t, uint32_t clearColor);
    virtual void writeTexture(TextureHandle t, uint32_t subresource, const void *data, uint32_t rowPitch, uint32_t depthPitch);
    virtual void destroyTexture(TextureHandle t);

    virtual BufferHandle createBuffer(const BufferDesc &d, const void *data);
    virtual void writeBuffer(BufferHandle b, const void *data, size_t dataSize);
    virtual void clearBufferUInt(BufferHandle b, uint32_t clearValue);
    virtual void copyToBuffer(BufferHandle dest, uint32_t destOffsetBytes, BufferHandle src, uint32_t srcOffsetBytes, size_t dataSizeBytes);
    virtual void readBuffer(BufferHandle b, void *data, size_t *dataSize);
    virtual void destroyBuffer(BufferHandle b);

    virtual ConstantBufferHandle createConstantBuffer(const ConstantBufferDesc &d, const void *data);
    virtual void writeConstantBuffer(ConstantBufferHandle b, const void *data, size_t dataSize);
    virtual void destroyConstantBuffer(ConstantBufferHandle b);

    virtual ShaderHandle createShader(const ShaderDesc &d, const void *binary, const size_t binarySize);
    virtual ShaderHandle createShaderFromAPIInterface(ShaderType::Enum shaderType, const void *apiInterface);
    virtual void destroyShader(ShaderHandle s);

    virtual SamplerHandle createSampler(const SamplerDesc &d);
    virtual void destroySampler(SamplerHandle s);

    virtual InputLayoutHandle createInputLayout(const VertexAttributeDesc *d, uint32_t attributeCount, const void *vertexShaderBinary, const size_t binarySize);
    virtual void destroyInputLayout(InputLayoutHandle i);

    virtual PerformanceQueryHandle createPerformanceQuery(const char *name);
    virtual void destroyPerformanceQuery(PerformanceQueryHandle query);
    virtual void beginPerformanceQuery(PerformanceQueryHandle query, bool onlyAnnotation);
    virtual void endPerformanceQuery(PerformanceQueryHandle query);
    virtual float getPerformanceQueryTimeMS(PerformanceQueryHandle query);

    virtual GraphicsAPI::Enum getGraphicsAPI();
    virtual void *getAPISpecificInterface(APISpecificInterface::Enum interfaceType);
    virtual bool isOpenGLExtensionSupported(const char *name);
    virtual void *getOpenGLProcAddress(const char *procname);

    virtual void draw(const DrawCallState &state, const DrawArguments *args, uint32_t numDrawCalls);
    virtual void drawIndexed(const DrawCallState &state, const DrawArguments *args, uint32_t numDrawCalls);
    virtual void drawIndirect(const DrawCallState &state, BufferHandle indirectParams, uint32_t offsetBytes);
    virtual void dispatch(const DispatchState &state, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);
    virtual void dispatchIndirect(const DispatchState &state, BufferHandle indirectParams, uint32_t offsetBytes);

    virtual void executeRenderThreadCommand(IRenderThreadCommand *onCommand);

    virtual uint32_t getNumberOfAFRGroups();
    virtual uint32_t getAFRGroupOfCurrentFrame(uint32_t numAFRGroups);

    virtual void setEnableUavBarriersForTexture(TextureHandle, bool) {}
    virtual void setEnableUavBarriersForBuffer(BufferHandle, bool) {}

    // These do not handle the pre/post commands
    void applyState(const DrawCallState &state, uint32_t denyStageMask = 0);
    void applyState(const DispatchState &state);
    void clearState();

    void debugBeginEvent(char const *name);
    void debugEndEvent();
  };

  struct UserState
  {
    void save(ID3D11DeviceContext *context);
    void restore(ID3D11DeviceContext *context);

    ID3D11InputLayout *pInputLayout;
    ID3D11Buffer *pIndexBuffer;
    DXGI_FORMAT IBFormat;
    uint32_t IBOffset;
    D3D11_PRIMITIVE_TOPOLOGY primitiveTopology;

    uint32_t numViewports;
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
    uint32_t numScissorRects;
    D3D11_RECT scissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
    ID3D11RasterizerState *pRS;

    ID3D11BlendState *pBlendState;
    float blendFactor[4];
    uint32_t sampleMask;
    ID3D11DepthStencilState *pDepthStencilState;
    uint32_t stencilRef;

    ID3D11VertexShader *pVS;
    ID3D11GeometryShader *pGS;
    ID3D11PixelShader *pPS;
    ID3D11ComputeShader *pCS;

    ID3D11Buffer *constantBuffersVS[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    ID3D11Buffer *constantBuffersGS[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    ID3D11Buffer *constantBuffersPS[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    ID3D11Buffer *constantBuffersCS[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

    ID3D11ShaderResourceView *shaderResourceViewsVS[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11ShaderResourceView *shaderResourceViewsGS[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11ShaderResourceView *shaderResourceViewsPS[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11ShaderResourceView *shaderResourceViewsCS[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11UnorderedAccessView *unorderedAccessViewsCS[D3D11_PS_CS_UAV_REGISTER_COUNT];

    ID3D11SamplerState *samplersVS[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    ID3D11SamplerState *samplersGS[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    ID3D11SamplerState *samplersPS[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    ID3D11SamplerState *samplersCS[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];

    ID3D11RenderTargetView *pRTVs[8];
    ID3D11DepthStencilView *pDSV;
  };

} // namespace NVRHI
