/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "GFSDK_NVRHI_D3D11.h"
#include <algorithm>
#include <codecvt>

#define CHECK_ERROR(expr, msg) \
    if (!(expr))               \
    this->signalError(__FILE__, __LINE__, msg)

#pragma warning(disable : 4127) // conditional expression is constant

namespace NVRHI
{
    using namespace Microsoft::WRL;

    class PerformanceQuery
    {
    public:
        enum State
        {
            NEW,
            STARTED,
            ANNOTATION,
            FINISHED,
            RESOLVED
        };

        std::wstring name;
        ComPtr<ID3D11Query> begin;
        ComPtr<ID3D11Query> end;
        ComPtr<ID3D11Query> disjoint;
        State state;
        float time;

        PerformanceQuery()
            : state(NEW), time(0.f)
        {
        }
    };

    // convert the format to a DXGI format
    static DXGI_FORMAT getUntypedTextureFormat(Format::Enum format, UINT &outPixelSizeBytes)
    {
        DXGI_FORMAT allocFormat = DXGI_FORMAT_UNKNOWN;
        switch (format)
        {
            // We prefer typeless since they can also be UAVs
        case Format::R8_UINT:
        case Format::R8_UNORM:
            allocFormat = DXGI_FORMAT_R8_TYPELESS;
            outPixelSizeBytes = 1;
            break;
        case Format::RG8_UINT:
        case Format::RG8_UNORM:
            allocFormat = DXGI_FORMAT_R8G8_TYPELESS;
            outPixelSizeBytes = 2;
            break;
        case Format::R16_UINT:
        case Format::R16_UNORM:
        case Format::R16_FLOAT:
            allocFormat = DXGI_FORMAT_R16_TYPELESS;
            outPixelSizeBytes = 2;
            break;
        case Format::BGRA8_UNORM:
            allocFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
            outPixelSizeBytes = 4;
            break;
        case Format::RGBA8_UNORM:
        case Format::SRGBA8_UNORM:
            allocFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
            outPixelSizeBytes = 4;
            break;
        case Format::R10G10B10A2_UNORM:
            allocFormat = DXGI_FORMAT_R10G10B10A2_TYPELESS;
            outPixelSizeBytes = 4;
            break;
        case Format::R11G11B10_FLOAT:
            allocFormat = DXGI_FORMAT_R11G11B10_FLOAT;
            outPixelSizeBytes = 4;
            break;
        case Format::RG16_UINT:
        case Format::RG16_FLOAT:
            allocFormat = DXGI_FORMAT_R16G16_TYPELESS;
            outPixelSizeBytes = 4;
            break;
        case Format::R32_UINT:
        case Format::R32_FLOAT:
            allocFormat = DXGI_FORMAT_R32_TYPELESS;
            outPixelSizeBytes = 4;
            break;
        case Format::RGBA16_FLOAT:
        case Format::RGBA16_UNORM:
        case Format::RGBA16_SNORM:
            allocFormat = DXGI_FORMAT_R16G16B16A16_TYPELESS;
            outPixelSizeBytes = 8;
            break;
        case Format::RG32_UINT:
        case Format::RG32_FLOAT:
            allocFormat = DXGI_FORMAT_R32G32_TYPELESS;
            outPixelSizeBytes = 8;
            break;
        case Format::RGB32_UINT:
        case Format::RGB32_FLOAT:
            allocFormat = DXGI_FORMAT_R32G32B32_TYPELESS;
            outPixelSizeBytes = 12;
            break;
        case Format::RGBA32_UINT:
        case Format::RGBA32_FLOAT:
            allocFormat = DXGI_FORMAT_R32G32B32A32_TYPELESS;
            outPixelSizeBytes = 16;
            break;
        case Format::D16:
            allocFormat = DXGI_FORMAT_R16_TYPELESS;
            outPixelSizeBytes = 2;
            break;
        case Format::D24S8:
        case Format::X24G8_UINT:
            allocFormat = DXGI_FORMAT_R24G8_TYPELESS;
            outPixelSizeBytes = 4;
            break;
        case Format::D32:
            allocFormat = DXGI_FORMAT_R32_TYPELESS;
            outPixelSizeBytes = 4;
            break;
        }
        return allocFormat;
    }

    static DXGI_FORMAT getTypedTextureFormat(Format::Enum format, UINT &outPixelSizeBytes, bool forSRV)
    {
        DXGI_FORMAT allocFormat = DXGI_FORMAT_UNKNOWN;
        // convert the format to a DXGI format
        switch (format)
        {
            // We prefer typeless since they can also be UAVs
        case Format::R8_UINT:
            allocFormat = DXGI_FORMAT_R8_UINT;
            outPixelSizeBytes = 1;
            break;
        case Format::R8_UNORM:
            allocFormat = DXGI_FORMAT_R8_UNORM;
            outPixelSizeBytes = 1;
            break;
        case Format::RG8_UINT:
            allocFormat = DXGI_FORMAT_R8G8_UINT;
            outPixelSizeBytes = 2;
            break;
        case Format::RG8_UNORM:
            allocFormat = DXGI_FORMAT_R8G8_UNORM;
            outPixelSizeBytes = 2;
            break;
        case Format::R16_UINT:
            allocFormat = DXGI_FORMAT_R16_UINT;
            outPixelSizeBytes = 2;
            break;
        case Format::R16_UNORM:
            allocFormat = DXGI_FORMAT_R16_UNORM;
            outPixelSizeBytes = 2;
            break;
        case Format::R16_FLOAT:
            allocFormat = DXGI_FORMAT_R16_FLOAT;
            outPixelSizeBytes = 2;
            break;
        case Format::RGBA8_UNORM:
            allocFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            outPixelSizeBytes = 4;
            break;
        case Format::BGRA8_UNORM:
            allocFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
            outPixelSizeBytes = 4;
            break;
        case Format::SRGBA8_UNORM:
            allocFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            outPixelSizeBytes = 4;
            break;
        case Format::R10G10B10A2_UNORM:
            allocFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
            outPixelSizeBytes = 4;
            break;
        case Format::R11G11B10_FLOAT:
            allocFormat = DXGI_FORMAT_R11G11B10_FLOAT;
            outPixelSizeBytes = 4;
            break;
        case Format::RG16_UINT:
            allocFormat = DXGI_FORMAT_R16G16_UINT;
            outPixelSizeBytes = 4;
            break;
        case Format::RG16_FLOAT:
            allocFormat = DXGI_FORMAT_R16G16_FLOAT;
            outPixelSizeBytes = 4;
            break;
        case Format::R32_UINT:
            allocFormat = DXGI_FORMAT_R32_UINT;
            outPixelSizeBytes = 4;
            break;
        case Format::R32_FLOAT:
            allocFormat = DXGI_FORMAT_R32_FLOAT;
            outPixelSizeBytes = 4;
            break;
        case Format::RGBA16_FLOAT:
            allocFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
            outPixelSizeBytes = 8;
            break;
        case Format::RGBA16_UNORM:
            allocFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
            outPixelSizeBytes = 8;
            break;
        case Format::RGBA16_SNORM:
            allocFormat = DXGI_FORMAT_R16G16B16A16_SNORM;
            outPixelSizeBytes = 8;
            break;
        case Format::RG32_UINT:
            allocFormat = DXGI_FORMAT_R32G32_UINT;
            outPixelSizeBytes = 8;
            break;
        case Format::RG32_FLOAT:
            allocFormat = DXGI_FORMAT_R32G32_FLOAT;
            outPixelSizeBytes = 8;
            break;
        case Format::RGB32_UINT:
            allocFormat = DXGI_FORMAT_R32G32B32_UINT;
            outPixelSizeBytes = 12;
            break;
        case Format::RGB32_FLOAT:
            allocFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            outPixelSizeBytes = 12;
            break;
        case Format::RGBA32_UINT:
            allocFormat = DXGI_FORMAT_R32G32B32A32_UINT;
            outPixelSizeBytes = 16;
            break;
        case Format::RGBA32_FLOAT:
            allocFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
            outPixelSizeBytes = 16;
            break;
        case Format::D16:
            allocFormat = forSRV ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_D16_UNORM;
            outPixelSizeBytes = 2;
            break;
        case Format::D24S8:
            allocFormat = forSRV ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
            outPixelSizeBytes = 4;
            break;
        case Format::X24G8_UINT:
            allocFormat = forSRV ? DXGI_FORMAT_X24_TYPELESS_G8_UINT : DXGI_FORMAT_D24_UNORM_S8_UINT;
            outPixelSizeBytes = 4;
            break;
        case Format::D32:
            allocFormat = forSRV ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_D32_FLOAT;
            outPixelSizeBytes = 4;
            break;
        }
        return allocFormat;
    }

    bool GetSSE42Support()
    {
        int cpui[4];
        __cpuidex(cpui, 1, 0);
        return !!(cpui[2] & 0x100000);
    }

    static const bool CpuSupportsSSE42 = GetSSE42Support();

    static const uint32_t CrcTable[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

    class CrcHash
    {
    private:
        uint32_t crc;

    public:
        CrcHash()
            : crc(0)
        {
        }

        uint32_t Get()
        {
            return crc;
        }

        template <size_t size>
        __forceinline void AddBytesSSE42(void *p)
        {
            static_assert(size % 4 == 0, "Size of hashable types must be multiple of 4");

            uint32_t *data = (uint32_t *)p;

            const size_t numIterations = size / sizeof(uint32_t);
            for (size_t i = 0; i < numIterations; i++)
            {
                crc = _mm_crc32_u32(crc, data[i]);
            }
        }

        __forceinline void AddBytes(char *p, uint32_t size)
        {
            for (uint32_t idx = 0; idx < size; idx++)
                crc = CrcTable[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
        }

        template <typename T>
        void Add(const T &value)
        {
            if (CpuSupportsSSE42)
                AddBytesSSE42<sizeof(value)>((void *)&value);
            else
                AddBytes((char *)&value, sizeof(value));
        }
    };

    void SetDebugName(ID3D11DeviceChild*, const char*)
    {
        // TODO:
    }

    RendererInterfaceD3D11::~RendererInterfaceD3D11()
    {
        clearCachedData();
    }

    RendererInterfaceD3D11::RendererInterfaceD3D11(IErrorCallback *errorCB, ID3D11DeviceContext *context)
        : context(context), errorCB(errorCB), nvapiIsInitalized(false)
    {
        this->context->GetDevice(&device);

        context->QueryInterface(IID_PPV_ARGS(&userDefinedAnnotation));
    }

    TextureHandle RendererInterfaceD3D11::createTexture(const TextureDesc &d, const void *data)
    {
        D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
        UINT pixelSizeBytes = 0;
        // We prefer typeless since they can also be UAVs
        DXGI_FORMAT allocFormat = getUntypedTextureFormat(d.format, pixelSizeBytes);
        CHECK_ERROR(allocFormat != DXGI_FORMAT_UNKNOWN, "Unknown format");

        // convert the usage
        switch (d.usage)
        {
        case TextureDesc::USAGE_DEFAULT:
            usage = D3D11_USAGE_DEFAULT;
            break;
        case TextureDesc::USAGE_DYNAMIC:
            usage = D3D11_USAGE_DYNAMIC;
            break;
        case TextureDesc::USAGE_IMMUTABLE:
            usage = D3D11_USAGE_IMMUTABLE;
            break;
        default:
            CHECK_ERROR(0, "Unknown usage");
        }
        // convert flags
        UINT bindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (d.isRenderTarget)
            bindFlags |= (d.format == Format::D16 || d.format == Format::D24S8 || d.format == Format::D32) ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
        if (d.isUAV)
            bindFlags |= D3D11_BIND_UNORDERED_ACCESS;

        UINT cpuAccessFlags = 0;
        if (d.isCPUWritable)
            cpuAccessFlags |= D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA *initialData = nullptr;
        if (data && d.mipLevels == 1)
        {
            uint32_t rowPitch = pixelSizeBytes * d.width;
            uint32_t slicePitch = rowPitch * d.height;
            uint32_t subresourcePitch = slicePitch * (d.isArray ? 1 : d.depthOrArraySize);

            uint32_t numSubresources = d.mipLevels;
            if (d.isArray || d.isCubeMap)
                numSubresources *= d.depthOrArraySize;

            initialData = new D3D11_SUBRESOURCE_DATA[numSubresources];

            for (uint32_t subresource = 0; subresource < numSubresources; subresource++)
            {
                initialData[subresource].pSysMem = (char *)data + subresourcePitch * subresource;
                initialData[subresource].SysMemPitch = rowPitch;
                initialData[subresource].SysMemSlicePitch = slicePitch;
            }
        }

        if (d.depthOrArraySize == 0 || d.isArray || d.isCubeMap)
        {
            // it's a 2D texture or an array
            D3D11_TEXTURE2D_DESC desc11;
            desc11.Width = (UINT)d.width;
            desc11.Height = (UINT)d.height;
            desc11.MipLevels = (UINT)d.mipLevels;
            // if d.isArray then d.depthOrArraySize is the length, otherwise it's the 3D texture depth
            desc11.ArraySize = (d.isArray || d.isCubeMap) ? (UINT)d.depthOrArraySize : 1;
            desc11.Format = allocFormat;
            desc11.SampleDesc.Count = (UINT)d.sampleCount;
            desc11.SampleDesc.Quality = (UINT)d.sampleQuality;
            desc11.Usage = usage;
            desc11.BindFlags = bindFlags;
            desc11.CPUAccessFlags = cpuAccessFlags;
            desc11.MiscFlags = d.isCubeMap ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

            ComPtr<ID3D11Texture2D> newTexture;
            HRESULT r = device->CreateTexture2D(&desc11, initialData, &newTexture);
            CHECK_ERROR(SUCCEEDED(r), "Creating a texture failed");

            if (d.disableGPUsSync)
                disableSLIResouceSync(newTexture.Get());

            if (d.debugName)
                SetDebugName(newTexture.Get(), d.debugName);

            if (initialData)
                delete[] initialData;

            // add it to the map (passing texture desc to store a copy of it)
            return (TextureHandle)getHandleForTexture(newTexture.Get(), &d);
        }
        else
        {
            // it's a 3D texture
            D3D11_TEXTURE3D_DESC desc11;
            desc11.Width = (UINT)d.width;
            desc11.Height = (UINT)d.height;
            desc11.Depth = (UINT)d.depthOrArraySize;
            desc11.MipLevels = (UINT)d.mipLevels;
            desc11.Format = allocFormat;
            desc11.Usage = usage;
            desc11.BindFlags = bindFlags;
            desc11.CPUAccessFlags = cpuAccessFlags;
            desc11.MiscFlags = 0;

            ComPtr<ID3D11Texture3D> newTexture;
            HRESULT r = device->CreateTexture3D(&desc11, initialData, &newTexture);
            CHECK_ERROR(SUCCEEDED(r), "Creating a texture failed");

            if (d.disableGPUsSync)
                disableSLIResouceSync(newTexture.Get());

            if (d.debugName)
                SetDebugName(newTexture.Get(), d.debugName);

            if (initialData)
                delete[] initialData;

            // add it to the map  (passing texture desc to store a copy of it)
            return (TextureHandle)getHandleForTexture(newTexture.Get(), &d);
        }
    }

    TextureDesc RendererInterfaceD3D11::describeTexture(TextureHandle t)
    {
        TextureObjectMap::value_type *handle = (TextureObjectMap::value_type *)t;
        return handle->second.textureDesc;
    }

    void RendererInterfaceD3D11::clearTextureFloat(TextureHandle t, const Color &clearColor)
    {
        TextureObjectMap::value_type *handle = (TextureObjectMap::value_type *)t;
        ID3D11UnorderedAccessView *uav = NULL;
        ID3D11RenderTargetView *rtv = NULL;
        ID3D11DepthStencilView *dsv = NULL;
        uint32_t index = 0;

        while (true)
        {
            getClearViewForTexture(handle, index++, false, uav, rtv, dsv);
            if (uav)
            {
                context->ClearUnorderedAccessViewFloat(uav, &clearColor.r);
            }
            else if (rtv)
            {
                context->ClearRenderTargetView(rtv, &clearColor.r);
            }
            else if (dsv)
            {
                // re-interpret .y as stencil. Maybe you don't want to do this, but we should do something.
                context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, clearColor.r, *((UINT8 *)&clearColor.g));
            }
            else
            {
                break; // no more sub-views
            }
        }
    }

    void RendererInterfaceD3D11::clearTextureUInt(TextureHandle t, uint32_t clearColor)
    {
        TextureObjectMap::value_type *handle = (TextureObjectMap::value_type *)t;
        ID3D11UnorderedAccessView *uav = NULL;
        ID3D11RenderTargetView *rtv = NULL;
        ID3D11DepthStencilView *dsv = NULL;
        uint32_t index = 0;

        while (true)
        {
            getClearViewForTexture(handle, index++, true, uav, rtv, dsv);
            if (uav)
            {
                UINT clearValues[4] = {clearColor, clearColor, clearColor, clearColor};
                context->ClearUnorderedAccessViewUint(uav, clearValues);
            }
            else if (rtv)
            {
                float clearValues[4] = {float(clearColor), float(clearColor), float(clearColor), float(clearColor)};
                context->ClearRenderTargetView(rtv, clearValues);
            }
            else if (dsv)
            {
                context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, float(clearColor), UINT8(clearColor));
            }
            else
            {
                break; // no more sub-views
            }
        }
    }

    void RendererInterfaceD3D11::writeTexture(TextureHandle t, uint32_t subresource, const void *data, uint32_t rowPitch, uint32_t depthPitch)
    {
        TextureObjectMap::value_type *handle = (TextureObjectMap::value_type *)t;

        ID3D11Resource *resource = handle->first.Get();

        context->UpdateSubresource(resource, subresource, NULL, data, rowPitch, depthPitch);
    }

    void RendererInterfaceD3D11::destroyTexture(TextureHandle t)
    {
        TextureObjectMap::value_type *handle = (TextureObjectMap::value_type *)t;
        if (!handle)
            return;
        // the smart pointer class will release the texture if we are the last owner
        textures.erase(handle->first);
    }

    BufferHandle RendererInterfaceD3D11::createBuffer(const BufferDesc &d, const void *data)
    {
        D3D11_BUFFER_DESC desc11;
        desc11.ByteWidth = (UINT)d.byteSize;

        // These don't map exactly, but it should be generally correct
        if (d.isCPUWritable)
            desc11.Usage = D3D11_USAGE_DYNAMIC;
        else if (data && !(d.isDrawIndirectArgs || d.canHaveUAVs))
            desc11.Usage = D3D11_USAGE_IMMUTABLE;
        else
            desc11.Usage = D3D11_USAGE_DEFAULT;

        desc11.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (d.canHaveUAVs)
            desc11.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

        desc11.CPUAccessFlags = 0;
        if (d.isCPUWritable)
            desc11.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;

        if (d.isIndexBuffer)
            desc11.BindFlags |= D3D11_BIND_INDEX_BUFFER;

        if (d.isVertexBuffer)
            desc11.BindFlags |= D3D11_BIND_VERTEX_BUFFER;

        desc11.MiscFlags = 0;
        if (d.isDrawIndirectArgs)
            desc11.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

        if (d.structStride != 0)
            desc11.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

        desc11.StructureByteStride = (UINT)d.structStride;

        D3D11_SUBRESOURCE_DATA initialData;
        initialData.pSysMem = data;
        initialData.SysMemPitch = 0;
        initialData.SysMemSlicePitch = 0;

        ComPtr<ID3D11Buffer> newBuffer;
        CHECK_ERROR(SUCCEEDED(device->CreateBuffer(&desc11, data ? &initialData : NULL, &newBuffer)), "Creation failed");

        if (d.disableGPUsSync)
            disableSLIResouceSync(newBuffer.Get());

        if (d.debugName)
            SetDebugName(newBuffer.Get(), d.debugName);
        
        // add to our map
        return (BufferHandle)getHandleForBuffer(newBuffer.Get(), &d);
    }

    void RendererInterfaceD3D11::writeBuffer(BufferHandle b, const void *data, size_t dataSize)
    {
        BufferObjectMap::value_type *handle = (BufferObjectMap::value_type *)b;

        if (handle->second.bufferDesc.isCPUWritable)
        {
            // we can map if it it's D3D11_USAGE_DYNAMIC, but not UpdateSubresource
            D3D11_MAPPED_SUBRESOURCE mappedData;
            CHECK_ERROR(SUCCEEDED(context->Map(handle->first.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData)), "Map failed");
            memcpy(mappedData.pData, data, dataSize);
            context->Unmap(handle->first.Get(), 0);
        }
        else
        {
            context->UpdateSubresource(handle->first.Get(), 0, NULL, data, (UINT)dataSize, 0);
        }
    }

    void RendererInterfaceD3D11::clearBufferUInt(BufferHandle b, uint32_t clearValue)
    {
        BufferObjectMap::value_type *handle = (BufferObjectMap::value_type *)b;
        ID3D11UnorderedAccessView *uav = getUAVForBuffer(handle);

        UINT clearValues[4] = {clearValue, clearValue, clearValue, clearValue};
        context->ClearUnorderedAccessViewUint(uav, clearValues);
    }

    void RendererInterfaceD3D11::copyToBuffer(BufferHandle dest, uint32_t destOffsetBytes, BufferHandle src, uint32_t srcOffsetBytes, size_t dataSizeBytes)
    {
        BufferObjectMap::value_type *handleDest = (BufferObjectMap::value_type *)dest;
        BufferObjectMap::value_type *handleSrc = (BufferObjectMap::value_type *)src;

        // Do a 1D copy
        D3D11_BOX srcBox;
        srcBox.left = (UINT)srcOffsetBytes;
        srcBox.right = (UINT)(srcOffsetBytes + dataSizeBytes);
        srcBox.bottom = 1;
        srcBox.top = 0;
        srcBox.front = 0;
        srcBox.back = 1;
        context->CopySubresourceRegion(handleDest->first.Get(), 0, (UINT)destOffsetBytes, 0, 0, handleSrc->first.Get(), 0, &srcBox);
    }

    void RendererInterfaceD3D11::readBuffer(BufferHandle b, void *data, size_t *dataSize)
    {
        if (!dataSize)
            return;

        size_t bufferSize = *dataSize;
        *dataSize = 0;

        if (!data)
            return;

        BufferObjectMap::value_type *handle = (BufferObjectMap::value_type *)b;

        D3D11_BUFFER_DESC desc;
        handle->first->GetDesc(&desc);
        desc.BindFlags = 0;
        desc.MiscFlags = 0;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        ComPtr<ID3D11Buffer> staging;
        if (FAILED(device->CreateBuffer(&desc, NULL, &staging)))
            return;

        context->CopyResource(staging.Get(), handle->first.Get());

        D3D11_MAPPED_SUBRESOURCE subresource;
        if (SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &subresource)))
        {
            size_t bytesCopied = std::min((size_t)desc.ByteWidth, bufferSize);
            memcpy(data, subresource.pData, bytesCopied);
            *dataSize = bytesCopied;

            context->Unmap(staging.Get(), 0);
        }
    }

    void RendererInterfaceD3D11::destroyBuffer(BufferHandle b)
    {
        BufferObjectMap::value_type *handle = (BufferObjectMap::value_type *)b;
        if (!handle)
            return;

        // smart pointers will clean up for us
        buffers.erase(handle->first);
    }

    ConstantBufferHandle RendererInterfaceD3D11::createConstantBuffer(const ConstantBufferDesc &d, const void *data)
    {
        // create our staging buffer if we don't have one
        D3D11_BUFFER_DESC desc11;
        desc11.ByteWidth = (UINT)d.byteSize;
        desc11.Usage = D3D11_USAGE_DYNAMIC;
        desc11.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc11.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc11.MiscFlags = 0;
        desc11.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA initialData;
        initialData.pSysMem = data;
        initialData.SysMemPitch = 0;
        initialData.SysMemSlicePitch = 0;

        ID3D11Buffer *constantBuffer = NULL;
        CHECK_ERROR(SUCCEEDED(device->CreateBuffer(&desc11, data ? &initialData : NULL, &constantBuffer)), "Creation of constant buffer failed");

        if (d.debugName)
            SetDebugName(constantBuffer, d.debugName);

        // we don't need to store any additional stuff so just use the D3D pointer as the handle
        return (ConstantBufferHandle)constantBuffer;
    }

    void RendererInterfaceD3D11::writeConstantBuffer(ConstantBufferHandle b, const void *data, size_t dataSize)
    {
        ID3D11Buffer *constantBuffer = (ID3D11Buffer *)b;

        D3D11_MAPPED_SUBRESOURCE mappedData;
        CHECK_ERROR(SUCCEEDED(context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData)), "Map failed");
        memcpy(mappedData.pData, data, dataSize);
        context->Unmap(constantBuffer, 0);
    }

    void RendererInterfaceD3D11::destroyConstantBuffer(ConstantBufferHandle b)
    {
        ID3D11Buffer *constantBuffer = (ID3D11Buffer *)b;
        if (!constantBuffer)
            return;
        // this should be all it takes
        constantBuffer->Release();
    }

    ShaderHandle RendererInterfaceD3D11::createShader(const ShaderDesc &d, const void *binary, const size_t binarySize)
    {
        if (d.preCreationCommand)
            d.preCreationCommand->executeAndDispose();

        ID3D11DeviceChild *ret = NULL;
        switch (d.shaderType)
        {
        case ShaderType::SHADER_VERTEX:
        {
            ID3D11VertexShader *vs = NULL;
            CHECK_ERROR(SUCCEEDED(device->CreateVertexShader(binary, binarySize, NULL, &vs)), "Creating VS failed");
            ret = vs;
        }
        break;
        case ShaderType::SHADER_HULL:
        {
            ID3D11HullShader *hs = NULL;
            CHECK_ERROR(SUCCEEDED(device->CreateHullShader(binary, binarySize, NULL, &hs)), "Creating HS failed");
            ret = hs;
        }
        break;
        case ShaderType::SHADER_DOMAIN:
        {
            ID3D11DomainShader *ds = NULL;
            CHECK_ERROR(SUCCEEDED(device->CreateDomainShader(binary, binarySize, NULL, &ds)), "Creating DS failed");
            ret = ds;
        }
        break;
        case ShaderType::SHADER_GEOMETRY:
        {
            ID3D11GeometryShader *gs = NULL;
            CHECK_ERROR(SUCCEEDED(device->CreateGeometryShader(binary, binarySize, NULL, &gs)), "Creating GS failed");
            ret = gs;
        }
        break;
        case ShaderType::SHADER_PIXEL:
        {
            ID3D11PixelShader *ps = NULL;
            CHECK_ERROR(SUCCEEDED(device->CreatePixelShader(binary, binarySize, NULL, &ps)), "Creating PS failed");
            ret = ps;
        }
        break;
        case ShaderType::SHADER_COMPUTE:
        {
            ID3D11ComputeShader *cs = NULL;
            CHECK_ERROR(SUCCEEDED(device->CreateComputeShader(binary, binarySize, NULL, &cs)), "Creating CS failed");
            ret = cs;
        }
        break;
        }

        if (d.postCreationCommand)
            d.postCreationCommand->executeAndDispose();

        return (ShaderHandle)ret;
    }

    ShaderHandle RendererInterfaceD3D11::createShaderFromAPIInterface(ShaderType::Enum shaderType, const void *apiInterface)
    {
        (void)shaderType;
        return (ShaderHandle)apiInterface;
    }

    void RendererInterfaceD3D11::destroyShader(ShaderHandle s)
    {
        ID3D11DeviceChild *shader = (ID3D11DeviceChild *)s;
        if (!shader)
            return;

        shader->Release();
    }

    // These are only in very new DXSDKs
#ifndef D3D11_FILTER_REDUCTION_TYPE_COMPARISON
#define D3D11_FILTER_REDUCTION_TYPE_COMPARISON 1
#endif

#ifndef D3D11_FILTER_REDUCTION_TYPE_STANDARD
#define D3D11_FILTER_REDUCTION_TYPE_STANDARD 0
#endif

    SamplerHandle RendererInterfaceD3D11::createSampler(const SamplerDesc &d)
    {

        // convert the sampler dessc
        D3D11_SAMPLER_DESC desc11;
        if (d.anisotropy > 1.0f)
        {
            desc11.Filter = D3D11_ENCODE_ANISOTROPIC_FILTER(d.shadowCompare ? D3D11_FILTER_REDUCTION_TYPE_COMPARISON : D3D11_FILTER_REDUCTION_TYPE_STANDARD);
        }
        else
        {
            desc11.Filter = D3D11_ENCODE_BASIC_FILTER(d.minFilter ? D3D11_FILTER_TYPE_LINEAR : D3D11_FILTER_TYPE_POINT, d.magFilter ? D3D11_FILTER_TYPE_LINEAR : D3D11_FILTER_TYPE_POINT, d.mipFilter ? D3D11_FILTER_TYPE_LINEAR : D3D11_FILTER_TYPE_POINT, d.shadowCompare ? D3D11_FILTER_REDUCTION_TYPE_COMPARISON : D3D11_FILTER_REDUCTION_TYPE_STANDARD);
        }
        D3D11_TEXTURE_ADDRESS_MODE *dest[] = {&desc11.AddressU, &desc11.AddressV, &desc11.AddressW};
        for (int i = 0; i < 3; i++)
        {
            switch (d.wrapMode[i])
            {
            case SamplerDesc::WRAP_MODE_BORDER:
                *dest[i] = D3D11_TEXTURE_ADDRESS_BORDER;
                break;
            case SamplerDesc::WRAP_MODE_CLAMP:
                *dest[i] = D3D11_TEXTURE_ADDRESS_CLAMP;
                break;
            case SamplerDesc::WRAP_MODE_WRAP:
                *dest[i] = D3D11_TEXTURE_ADDRESS_WRAP;
                break;
            }
        }

        desc11.MipLODBias = d.mipBias;
        desc11.MaxAnisotropy = std::max((UINT)d.anisotropy, 1U);
        desc11.ComparisonFunc = D3D11_COMPARISON_LESS;
        desc11.BorderColor[0] = d.borderColor.r;
        desc11.BorderColor[1] = d.borderColor.g;
        desc11.BorderColor[2] = d.borderColor.b;
        desc11.BorderColor[3] = d.borderColor.a;
        desc11.MinLOD = 0;
        desc11.MaxLOD = D3D11_FLOAT32_MAX;

        ID3D11SamplerState *sState = NULL;
        CHECK_ERROR(SUCCEEDED(device->CreateSamplerState(&desc11, &sState)), "Creating sampler state failed");

        return (SamplerHandle)sState;
    }

    void RendererInterfaceD3D11::destroySampler(SamplerHandle s)
    {
        ID3D11SamplerState *sState = (ID3D11SamplerState *)s;
        if (!sState)
            return;
        sState->Release();
    }

    InputLayoutHandle RendererInterfaceD3D11::createInputLayout(const VertexAttributeDesc *d, uint32_t attributeCount, const void *vertexShaderBinary, const size_t binarySize)
    {
        uint32_t dontCare;
        D3D11_INPUT_ELEMENT_DESC elementDesc[DrawCallState::MAX_VERTEX_ATTRIBUTE_COUNT];
        for (uint32_t i = 0; i < attributeCount; i++)
        {
            elementDesc[i].SemanticName = d[i].name;
            elementDesc[i].SemanticIndex = 0;
            elementDesc[i].Format = getTypedTextureFormat(d[i].format, dontCare, false);
            elementDesc[i].InputSlot = d[i].bufferIndex;
            elementDesc[i].AlignedByteOffset = d[i].offset;
            elementDesc[i].InputSlotClass = d[i].isInstanced ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
            elementDesc[i].InstanceDataStepRate = d[i].isInstanced ? 1 : 0;
        }

        ID3D11InputLayout *inputLayout = NULL;
        CHECK_ERROR(SUCCEEDED(device->CreateInputLayout(elementDesc, attributeCount, vertexShaderBinary, binarySize, &inputLayout)), "CreateInputLayout() failed");

        return (InputLayoutHandle)inputLayout;
    }

    void RendererInterfaceD3D11::destroyInputLayout(InputLayoutHandle i)
    {
        ID3D11InputLayout *inputLayout = (ID3D11InputLayout *)i;
        if (!inputLayout)
            return;
        inputLayout->Release();
    }

    GraphicsAPI::Enum RendererInterfaceD3D11::getGraphicsAPI()
    {
        return GraphicsAPI::D3D11;
    }

    void *RendererInterfaceD3D11::getAPISpecificInterface(APISpecificInterface::Enum interfaceType)
    {
        if (interfaceType == APISpecificInterface::D3D11DEVICECONTEXT)
        {
            // we only support this interface type
            ID3D11DeviceContext *d3d11Context = context.Get();
            return d3d11Context;
        }
        else if (interfaceType == APISpecificInterface::D3D11DEVICE)
        {
            // we only support this interface type
            ID3D11Device *d3d11Device = device.Get();
            return d3d11Device;
        }
        return NULL;
    }

    void RendererInterfaceD3D11::draw(const DrawCallState &state, const DrawArguments *args, uint32_t numDrawCalls)
    {
        clearState();
        applyState(state);

        for (uint32_t i = 0; i < numDrawCalls; i++)
            context->DrawInstanced(args[i].vertexCount, args[i].instanceCount, args[i].startVertexLocation, args[i].startInstanceLocation);

        clearState();
    }

    void RendererInterfaceD3D11::drawIndexed(const DrawCallState &state, const DrawArguments *args, uint32_t numDrawCalls)
    {
        clearState();
        applyState(state);

        for (uint32_t i = 0; i < numDrawCalls; i++)
            context->DrawIndexedInstanced(args[i].vertexCount, args[i].instanceCount, args[i].startIndexLocation, args[i].startVertexLocation, args[i].startInstanceLocation);

        clearState();
    }

    void RendererInterfaceD3D11::drawIndirect(const DrawCallState &state, BufferHandle indirectParams, uint32_t offsetBytes)
    {
        clearState();
        applyState(state);

        BufferObjectMap::value_type *handle = (BufferObjectMap::value_type *)indirectParams;
        context->DrawInstancedIndirect(handle->first.Get(), offsetBytes);

        clearState();
    }

    void RendererInterfaceD3D11::dispatch(const DispatchState &state, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        clearState();
        applyState(state);

        context->Dispatch(groupsX, groupsY, groupsZ);

        clearState();
    }

    void RendererInterfaceD3D11::dispatchIndirect(const DispatchState &state, BufferHandle indirectParams, uint32_t offsetBytes)
    {
        clearState();
        applyState(state);

        BufferObjectMap::value_type *handleArgs = (BufferObjectMap::value_type *)indirectParams;
        context->DispatchIndirect(handleArgs->first.Get(), (UINT)offsetBytes);

        clearState();
    }

    RendererInterfaceD3D11::TextureObjectMap::value_type *RendererInterfaceD3D11::getHandleForTexture(ID3D11Resource *resource, const TextureDesc *textureDesc)
    {
        if (!resource) // if it's null, we want a null handle
            return NULL;

        TextureObjectMap::iterator it = textures.find(resource);
        if (it == textures.end())
        {
            it = textures.insert(TextureObjectMap::value_type(resource, TextureViewSet())).first; // we haven't seen this one before

            // use our provided one or make one from the D3D data
            it->second.textureDesc = textureDesc ? *textureDesc : getTextureDescFromD3D11Resource(resource);

            // Resize these withe empty views to the right sizes for simplicity later
            it->second.unorderedAccessViewsPerMip.resize(it->second.textureDesc.mipLevels);
        }

        TextureObjectMap::value_type &mapValue = *it;
        return &mapValue;
    }

    void RendererInterfaceD3D11::getClearViewForTexture(TextureObjectMap::value_type *resource, uint32_t index, bool asUINT, ID3D11UnorderedAccessView *&outUAV, ID3D11RenderTargetView *&outRTV, ID3D11DepthStencilView *&outDSV)
    {
        outUAV = NULL;
        outRTV = NULL;
        outDSV = NULL;

        const TextureDesc &textureDesc = resource->second.textureDesc;
        // Try UAVs first since they are more flexible
        if (textureDesc.isUAV)
        {
            DXGI_FORMAT format;

            if (asUINT)
                format = DXGI_FORMAT_R32_UINT;
            else if (textureDesc.format == Format::RGBA16_FLOAT)
                format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            else if (textureDesc.format == Format::R8_UNORM)
                format = DXGI_FORMAT_R8_UNORM;
            else
                format = DXGI_FORMAT_R32_FLOAT;

            // we have UAVs;
            if (index < textureDesc.mipLevels)
                outUAV = getUAVForTexture(resource, format, index); // return or create a UAV or return null if we are out of mips
        }
        else if (textureDesc.isRenderTarget)
        {
            // if we are still within the volume or array
            if ((index == 0 && !textureDesc.isArray) || index < textureDesc.depthOrArraySize)
            {
                if (textureDesc.format == Format::D16 || textureDesc.format == Format::D24S8 || textureDesc.format == Format::D32)
                    outDSV = getDSVForTexture(resource, index); // return or create a RT
                else
                    outRTV = getRTVForTexture(resource, index); // return or create a RT
            }
        }
        else
        {
            CHECK_ERROR(0, "This resource cannot be cleared");
        }
    }

    ID3D11ShaderResourceView *RendererInterfaceD3D11::getSRVForTexture(TextureObjectMap::value_type *resource, DXGI_FORMAT format, uint32_t mipLevel)
    {
        ComPtr<ID3D11ShaderResourceView> &srvPtr = resource->second.shaderResourceViews[std::make_pair(format, mipLevel)];
        if (srvPtr == NULL)
        {
            // we haven't seen this one before
            const TextureDesc &textureDesc = resource->second.textureDesc;
            D3D11_SHADER_RESOURCE_VIEW_DESC desc11;
            desc11.Format = format;
            if (textureDesc.isCubeMap)
            {
                if (textureDesc.depthOrArraySize > 6)
                    desc11.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                else
                    desc11.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                desc11.Texture2DArray.ArraySize = (UINT)textureDesc.depthOrArraySize / 6;
                desc11.Texture2DArray.FirstArraySlice = 0;

                if (mipLevel >= textureDesc.mipLevels)
                {
                    desc11.Texture2DArray.MipLevels = (UINT)textureDesc.mipLevels;
                    desc11.Texture2DArray.MostDetailedMip = 0;
                }
                else
                {
                    desc11.Texture2DArray.MipLevels = 1;
                    desc11.Texture2DArray.MostDetailedMip = mipLevel;
                }
            }
            else if (textureDesc.isArray)
            {
                if (textureDesc.sampleCount > 1)
                {
                    desc11.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
                    desc11.Texture2DMSArray.ArraySize = (UINT)textureDesc.depthOrArraySize;
                    desc11.Texture2DMSArray.FirstArraySlice = 0;
                }
                else
                {
                    desc11.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                    desc11.Texture2DArray.ArraySize = (UINT)textureDesc.depthOrArraySize;
                    desc11.Texture2DArray.FirstArraySlice = 0;

                    if (mipLevel >= textureDesc.mipLevels)
                    {
                        desc11.Texture2DArray.MipLevels = (UINT)textureDesc.mipLevels;
                        desc11.Texture2DArray.MostDetailedMip = 0;
                    }
                    else
                    {
                        desc11.Texture2DArray.MipLevels = 1;
                        desc11.Texture2DArray.MostDetailedMip = mipLevel;
                    }
                }
            }
            else if (textureDesc.depthOrArraySize > 0)
            {
                desc11.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;

                if (mipLevel >= textureDesc.mipLevels)
                {
                    desc11.Texture3D.MipLevels = (UINT)textureDesc.mipLevels;
                    desc11.Texture3D.MostDetailedMip = 0;
                }
                else
                {
                    desc11.Texture3D.MipLevels = 1;
                    desc11.Texture3D.MostDetailedMip = mipLevel;
                }
            }
            else
            {
                if (textureDesc.sampleCount > 1)
                {
                    desc11.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
                }
                else
                {
                    desc11.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

                    if (mipLevel >= textureDesc.mipLevels)
                    {
                        desc11.Texture2D.MipLevels = (UINT)textureDesc.mipLevels;
                        desc11.Texture2D.MostDetailedMip = 0;
                    }
                    else
                    {
                        desc11.Texture2D.MipLevels = 1;
                        desc11.Texture2D.MostDetailedMip = mipLevel;
                    }
                }
            }
            CHECK_ERROR(SUCCEEDED(device->CreateShaderResourceView(resource->first.Get(), &desc11, &srvPtr)), "Creating the view failed");
        }
        return srvPtr.Get();
    }

    ID3D11RenderTargetView *RendererInterfaceD3D11::getRTVForTexture(TextureObjectMap::value_type *resource, uint32_t arrayItem, uint32_t mipLevel)
    {
        ComPtr<ID3D11RenderTargetView> &rtvPtr = resource->second.renderTargetViews[std::make_pair(arrayItem, mipLevel)];
        if (rtvPtr == NULL)
        {
            // we haven't seen this one before
            const TextureDesc &textureDesc = resource->second.textureDesc;
            D3D11_RENDER_TARGET_VIEW_DESC desc11;
            UINT dontCare;
            desc11.Format = getTypedTextureFormat(textureDesc.format, dontCare, false);
            if (textureDesc.isArray || textureDesc.isCubeMap)
            {
                if (textureDesc.sampleCount > 1)
                {
                    desc11.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                    desc11.Texture2DMSArray.ArraySize = 1;
                    desc11.Texture2DMSArray.FirstArraySlice = (UINT)arrayItem;
                }
                else
                {
                    desc11.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                    desc11.Texture2DArray.ArraySize = 1;
                    desc11.Texture2DArray.FirstArraySlice = (UINT)arrayItem;
                    desc11.Texture2DArray.MipSlice = mipLevel;
                }
            }
            else if (textureDesc.depthOrArraySize > 0)
            {
                desc11.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                desc11.Texture3D.FirstWSlice = (UINT)arrayItem;
                desc11.Texture3D.WSize = 1;
                desc11.Texture3D.MipSlice = mipLevel;
            }
            else
            {
                if (textureDesc.sampleCount > 1)
                {
                    desc11.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
                }
                else
                {
                    desc11.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                    desc11.Texture2D.MipSlice = mipLevel;
                }
            }
            CHECK_ERROR(SUCCEEDED(device->CreateRenderTargetView(resource->first.Get(), &desc11, &rtvPtr)), "Creating the view failed");
        }
        return rtvPtr.Get();
    }

    ID3D11DepthStencilView *RendererInterfaceD3D11::getDSVForTexture(TextureObjectMap::value_type *resource, uint32_t arrayItem, uint32_t mipLevel)
    {
        ComPtr<ID3D11DepthStencilView> &dsvPtr = resource->second.depthStencilViews[std::make_pair(arrayItem, mipLevel)];
        if (dsvPtr == NULL)
        {
            // we haven't seen this one before
            const TextureDesc &textureDesc = resource->second.textureDesc;
            D3D11_DEPTH_STENCIL_VIEW_DESC desc11;
            UINT dontCare;
            desc11.Format = getTypedTextureFormat(textureDesc.format, dontCare, false);
            desc11.Flags = 0;
            if (textureDesc.isArray)
            {
                if (textureDesc.sampleCount > 1)
                {
                    desc11.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                    desc11.Texture2DMSArray.ArraySize = 1;
                    desc11.Texture2DMSArray.FirstArraySlice = (UINT)arrayItem;
                }
                else
                {
                    desc11.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                    desc11.Texture2DArray.ArraySize = 1;
                    desc11.Texture2DArray.FirstArraySlice = (UINT)arrayItem;
                    desc11.Texture2DArray.MipSlice = mipLevel;
                }
            }
            else if (textureDesc.depthOrArraySize > 0)
            {
                CHECK_ERROR(0, "No 3D depth textures");
            }
            else
            {
                if (textureDesc.sampleCount > 1)
                {
                    desc11.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                }
                else
                {
                    desc11.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                    desc11.Texture2D.MipSlice = mipLevel;
                }
            }
            CHECK_ERROR(SUCCEEDED(device->CreateDepthStencilView(resource->first.Get(), &desc11, &dsvPtr)), "Creating the view failed");
        }
        return dsvPtr.Get();
    }

    ID3D11UnorderedAccessView *RendererInterfaceD3D11::getUAVForTexture(TextureObjectMap::value_type *resource, DXGI_FORMAT format, uint32_t mipLevel /*= 0*/)
    {
        ComPtr<ID3D11UnorderedAccessView> &uavPtr = resource->second.unorderedAccessViewsPerMip[mipLevel][format];
        if (uavPtr == NULL)
        {
            // we haven't seen this one before
            const TextureDesc &textureDesc = resource->second.textureDesc;

            CHECK_ERROR(textureDesc.sampleCount <= 1, "You cannot access a multisample UAV");

            D3D11_UNORDERED_ACCESS_VIEW_DESC desc11;
            desc11.Format = format;
            if (textureDesc.isArray)
            {
                desc11.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                desc11.Texture2DArray.ArraySize = (UINT)textureDesc.depthOrArraySize;
                desc11.Texture2DArray.FirstArraySlice = 0;
                desc11.Texture2DArray.MipSlice = (UINT)mipLevel;
            }
            else if (textureDesc.depthOrArraySize > 0)
            {
                desc11.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                desc11.Texture3D.FirstWSlice = 0;
                desc11.Texture3D.WSize = (UINT)textureDesc.depthOrArraySize;
                desc11.Texture3D.MipSlice = (UINT)mipLevel;
            }
            else
            {
                desc11.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                desc11.Texture2D.MipSlice = (UINT)mipLevel;
            }
            CHECK_ERROR(SUCCEEDED(device->CreateUnorderedAccessView(resource->first.Get(), &desc11, &uavPtr)), "Creating the view failed");
        }
        return uavPtr.Get();
    }

    ID3D11ShaderResourceView *RendererInterfaceD3D11::getSRVForTexture(TextureHandle handle, uint32_t mipLevel)
    {
        TextureObjectMap::value_type *resource = (TextureObjectMap::value_type *)handle;
        UINT dontCare;
        return getSRVForTexture(resource, getTypedTextureFormat(resource->second.textureDesc.format, dontCare, true), mipLevel);
    }

    ID3D11RenderTargetView *RendererInterfaceD3D11::getRTVForTexture(TextureHandle handle, uint32_t arrayItem)
    {
        TextureObjectMap::value_type *resource = (TextureObjectMap::value_type *)handle;
        return getRTVForTexture(resource, arrayItem);
    }

    ID3D11DepthStencilView *RendererInterfaceD3D11::getDSVForTexture(TextureHandle handle, uint32_t arrayItem, uint32_t mipLevel)
    {
        TextureObjectMap::value_type *resource = (TextureObjectMap::value_type *)handle;
        return getDSVForTexture(resource, arrayItem, mipLevel);
    }

    ID3D11UnorderedAccessView *RendererInterfaceD3D11::getUAVForTexture(TextureHandle handle, uint32_t mipLevel)
    {
        TextureObjectMap::value_type *resource = (TextureObjectMap::value_type *)handle;
        UINT dontCare;
        return getUAVForTexture(resource, getTypedTextureFormat(resource->second.textureDesc.format, dontCare, true), mipLevel);
    }

    void RendererInterfaceD3D11::applyState(const DrawCallState &state, uint32_t denyStageMask)
    {
        ID3D11RenderTargetView *renderTargetViews[D3D11_PS_OUTPUT_REGISTER_COUNT] = {0};
        UINT rtvCount = 0;
        ID3D11DepthStencilView *depthView = NULL;

        if ((denyStageMask & StageMask::DENY_INPUT_STATE) == 0)
        {
            context->IASetPrimitiveTopology(getPrimType(state.primType));
            context->IASetInputLayout((ID3D11InputLayout *)state.inputLayout);

            if (state.indexBuffer)
            {
                BufferObjectMap::value_type *handle = (BufferObjectMap::value_type *)state.indexBuffer;
                UINT dontCare = 0;
                context->IASetIndexBuffer(handle->first.Get(), getTypedTextureFormat(state.indexBufferFormat, dontCare, true), state.indexBufferOffset);
            }

            for (uint32_t i = 0; i < state.vertexBufferCount; i++)
            {
                BufferObjectMap::value_type *handle = (BufferObjectMap::value_type *)state.vertexBuffers[i].buffer;
                if (!handle)
                    return;

                ID3D11Buffer *pBuffer = (ID3D11Buffer *)handle->first.Get();
                context->IASetVertexBuffers(state.vertexBuffers[i].slot, 1, &pBuffer, &state.vertexBuffers[i].stride, &state.vertexBuffers[i].offset);
            }
        }

        if ((denyStageMask & StageMask::DENY_RENDER_STATE) == 0)
        {
            D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX] = {0};
            D3D11_RECT scissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX] = {0};

            const RenderState &renderState = state.renderState;
            FLOAT clearColor[4] = {renderState.clearColor.r, renderState.clearColor.g, renderState.clearColor.b, renderState.clearColor.a};
            // Setup the targets
            for (uint32_t rt = 0; rt < renderState.targetCount; rt++)
            {
                renderTargetViews[rt] = getRTVForTexture((TextureObjectMap::value_type *)renderState.targets[rt], state.renderState.targetIndicies[rt], state.renderState.targetMipSlices[rt]);
                rtvCount = std::max(rtvCount, (UINT)rt + 1);
                // clear stuff if required
                if (renderState.clearColorTarget)
                    context->ClearRenderTargetView(renderTargetViews[rt], clearColor);
            }

            for (uint32_t rt = 0; rt < renderState.viewportCount; rt++)
            {
                // copy viewport
                viewports[rt].TopLeftX = state.renderState.viewports[rt].minX;
                viewports[rt].TopLeftY = state.renderState.viewports[rt].minY;
                viewports[rt].Width = state.renderState.viewports[rt].maxX - state.renderState.viewports[rt].minX;
                viewports[rt].Height = state.renderState.viewports[rt].maxY - state.renderState.viewports[rt].minY;
                viewports[rt].MinDepth = state.renderState.viewports[rt].minZ;
                viewports[rt].MaxDepth = state.renderState.viewports[rt].maxZ;

                scissorRects[rt].left = (LONG)renderState.scissorRects[rt].minX;
                scissorRects[rt].top = (LONG)renderState.scissorRects[rt].minY;
                scissorRects[rt].right = (LONG)renderState.scissorRects[rt].maxX;
                scissorRects[rt].bottom = (LONG)renderState.scissorRects[rt].maxY;
            }

            if (state.renderState.depthTarget)
                depthView = getDSVForTexture((TextureObjectMap::value_type *)state.renderState.depthTarget, state.renderState.depthIndex, state.renderState.depthMipSlice);

            // clear stuff if required
            if (depthView && (renderState.clearDepthTarget || renderState.clearStencilTarget))
            {
                UINT clearFlags = 0;
                if (renderState.clearDepthTarget)
                    clearFlags |= D3D11_CLEAR_DEPTH;
                if (renderState.clearStencilTarget)
                    clearFlags |= D3D11_CLEAR_STENCIL;

                context->ClearDepthStencilView(depthView, clearFlags, renderState.clearDepth, (UINT8)renderState.clearStencil);
            }

            // Apply them
            context->RSSetViewports((UINT)renderState.viewportCount, viewports);
            context->RSSetScissorRects((UINT)renderState.viewportCount, scissorRects);

            // Get cached states or create new ones
            ID3D11RasterizerState *d3dRasterizerState = getRasterizerState(renderState.rasterState);
            ID3D11BlendState *d3dBlendState = getBlendState(renderState.blendState);
            ID3D11DepthStencilState *d3dDepthStencilState = getDepthStencilState(renderState.depthStencilState);

            // set the states
            context->RSSetState(d3dRasterizerState);
            FLOAT blendFactor[4] = {renderState.blendState.blendFactor.r, renderState.blendState.blendFactor.g, renderState.blendState.blendFactor.b, renderState.blendState.blendFactor.a};
            context->OMSetBlendState(d3dBlendState, blendFactor, D3D11_DEFAULT_SAMPLE_MASK);
            context->OMSetDepthStencilState(d3dDepthStencilState, (UINT)renderState.depthStencilState.stencilRefValue);
        }

        // Bind resources
        for (uint32_t stage = 0; stage < ShaderType::GRAPHIC_SHADERS_NUM; stage++)
        {
            if (denyStageMask & (0x1 << stage))
                continue; // ignore stage

            const PipelineStageBindings *bindings = NULL;

            switch (stage)
            {
            case ShaderType::SHADER_VERTEX:
                bindings = &state.VS;
                break;
            case ShaderType::SHADER_HULL:
                bindings = &state.HS;
                break;
            case ShaderType::SHADER_DOMAIN:
                bindings = &state.DS;
                break;
            case ShaderType::SHADER_GEOMETRY:
                bindings = &state.GS;
                break;
            case ShaderType::SHADER_PIXEL:
                bindings = &state.PS;
                break;
            }

            // Apply the shader. We cast to ID3D11DeviceChild first since that's what the handle is cast to before it was given to the client
            ID3D11DeviceChild *baseShader = (ID3D11DeviceChild *)bindings->shader;
            if (baseShader == NULL)
            {
                switch (stage)
                {
                case ShaderType::SHADER_VERTEX:
                    context->VSSetShader(NULL, NULL, 0);
                    break;
                case ShaderType::SHADER_HULL:
                    context->HSSetShader(NULL, NULL, 0);
                    break;
                case ShaderType::SHADER_DOMAIN:
                    context->DSSetShader(NULL, NULL, 0);
                    break;
                case ShaderType::SHADER_GEOMETRY:
                    context->GSSetShader(NULL, NULL, 0);
                    break;
                case ShaderType::SHADER_PIXEL:
                    context->PSSetShader(NULL, NULL, 0);
                    // shadow map rendering has no PS but a depth target is bound
                    context->OMSetRenderTargets(rtvCount, renderTargetViews, depthView);
                    break;
                }

                continue;
            }

            ID3D11ShaderResourceView *shaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
            UINT minSRV = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, maxSRV = 0;

            ID3D11UnorderedAccessView *unorderedAccessViews[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
            UINT minUAV = D3D11_PS_CS_UAV_REGISTER_COUNT, maxUAV = 0;
            UINT uavCountersUnused[D3D11_PS_CS_UAV_REGISTER_COUNT] = {D3D11_KEEP_UNORDERED_ACCESS_VIEWS};

            ID3D11Buffer *constantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_COUNT] = {0};
            UINT minCB = D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_COUNT, maxCB = 0;

            ID3D11SamplerState *samplers[D3D11_COMMONSHADER_SAMPLER_REGISTER_COUNT] = {0};
            UINT minSS = D3D11_COMMONSHADER_SAMPLER_REGISTER_COUNT, maxSS = 0;

            // Bind textures
            for (uint32_t i = 0; i < bindings->textureBindingCount; i++)
            {
                DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
                UINT dontCareSize;
                UINT slot = (UINT)bindings->textures[i].slot;
                TextureObjectMap::value_type *resource = (TextureObjectMap::value_type *)bindings->textures[i].texture;
                switch (bindings->textures[i].format)
                {
                case Format::R32_UINT:
                    textureFormat = DXGI_FORMAT_R32_UINT;
                    break;
                case Format::R32_FLOAT:
                    textureFormat = DXGI_FORMAT_R32_FLOAT;
                    break;
                case Format::RGBA8_UNORM:
                    textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                    break;
                case Format::BGRA8_UNORM:
                    textureFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
                    break;
                case Format::RGBA16_FLOAT:
                    textureFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
                    break;
                case Format::X24G8_UINT:
                    textureFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
                    break;
                case Format::UNKNOWN:
                    textureFormat = getTypedTextureFormat(resource->second.textureDesc.format, dontCareSize, true);
                    break;
                default:
                    CHECK_ERROR(0, "Unknown format");
                }

                // choose a SRV or UAV
                if (bindings->textures[i].isWritable)
                {
                    CHECK_ERROR(stage == ShaderType::SHADER_PIXEL, "UAVs only supported in pixel shaders");
                    unorderedAccessViews[slot] = getUAVForTexture(resource, textureFormat, bindings->textures[i].mipLevel);
                    minUAV = std::min(slot, minUAV);
                    maxUAV = std::max(slot, maxUAV);
                }
                else
                {
                    shaderResourceViews[slot] = getSRVForTexture(resource, textureFormat, bindings->textures[i].mipLevel);
                    minSRV = std::min(slot, minSRV);
                    maxSRV = std::max(slot, maxSRV);
                }
            }

            // Bind samplers
            for (uint32_t i = 0; i < bindings->textureSamplerBindingCount; i++)
            {
                UINT slot = (UINT)bindings->textureSamplers[i].slot;
                ID3D11SamplerState *sampler = (ID3D11SamplerState *)bindings->textureSamplers[i].sampler;

                samplers[slot] = sampler;
                minSS = std::min(slot, minSS);
                maxSS = std::max(slot, maxSS);
            }

            // Bind buffers
            for (uint32_t i = 0; i < bindings->bufferBindingCount; i++)
            {
                UINT slot = (UINT)bindings->buffers[i].slot;
                BufferObjectMap::value_type *resource = (BufferObjectMap::value_type *)bindings->buffers[i].buffer;
                // choose a SRV or UAV
                if (bindings->buffers[i].isWritable)
                {
                    CHECK_ERROR(stage == ShaderType::SHADER_PIXEL, "UAVs only supported in pixel shaders");
                    unorderedAccessViews[slot] = getUAVForBuffer(resource);
                    minUAV = std::min(slot, minUAV);
                    maxUAV = std::max(slot, maxUAV);
                }
                else
                {
                    shaderResourceViews[slot] = getSRVForBuffer(resource, bindings->buffers[i].format);
                    minSRV = std::min(slot, minSRV);
                    maxSRV = std::max(slot, maxSRV);
                }
            }

            // bind Constant buffers
            for (uint32_t i = 0; i < bindings->constantBufferBindingCount; i++)
            {
                UINT slot = (UINT)bindings->constantBuffers[i].slot;
                ID3D11Buffer *cbuffer = (ID3D11Buffer *)bindings->constantBuffers[i].buffer;

                constantBuffers[slot] = cbuffer;
                minCB = std::min(slot, minCB);
                maxCB = std::max(slot, maxCB);
            }

            switch (stage)
            {
            case ShaderType::SHADER_VERTEX:
            {
                ComPtr<ID3D11VertexShader> shader;
                baseShader->QueryInterface<ID3D11VertexShader>(&shader);
                CHECK_ERROR(shader != NULL, "This is not the right shader type");

                // apply the shader
                context->VSSetShader(shader.Get(), NULL, 0);

                // Apply them to the context
                if (maxCB >= minCB)
                    context->VSSetConstantBuffers(minCB, maxCB - minCB + 1, constantBuffers + minCB);

                if (maxSRV >= minSRV)
                    context->VSSetShaderResources(minSRV, maxSRV - minSRV + 1, shaderResourceViews + minSRV);

                if (maxSS >= minSS)
                    context->VSSetSamplers(minSS, maxSS - minSS + 1, samplers + minSS);

                break;
            }
            case ShaderType::SHADER_GEOMETRY:
            {
                ComPtr<ID3D11GeometryShader> shader;
                baseShader->QueryInterface<ID3D11GeometryShader>(&shader);
                CHECK_ERROR(shader != NULL, "This is not the right shader type");

                // apply the shader
                context->GSSetShader(shader.Get(), NULL, 0);

                // Apply them to the context
                if (maxCB >= minCB)
                    context->GSSetConstantBuffers(minCB, maxCB - minCB + 1, constantBuffers + minCB);

                if (maxSRV >= minSRV)
                    context->GSSetShaderResources(minSRV, maxSRV - minSRV + 1, shaderResourceViews + minSRV);

                if (maxSS >= minSS)
                    context->GSSetSamplers(minSS, maxSS - minSS + 1, samplers + minSS);

                break;
            }
            case ShaderType::SHADER_HULL:
            {
                ComPtr<ID3D11HullShader> shader;
                baseShader->QueryInterface<ID3D11HullShader>(&shader);
                CHECK_ERROR(shader != NULL, "This is not the right shader type");

                // apply the shader
                context->HSSetShader(shader.Get(), NULL, 0);

                // Apply them to the context
                if (maxCB >= minCB)
                    context->HSSetConstantBuffers(minCB, maxCB - minCB + 1, constantBuffers + minCB);

                if (maxSRV >= minSRV)
                    context->HSSetShaderResources(minSRV, maxSRV - minSRV + 1, shaderResourceViews + minSRV);

                if (maxSS >= minSS)
                    context->HSSetSamplers(minSS, maxSS - minSS + 1, samplers + minSS);

                break;
            }
            case ShaderType::SHADER_DOMAIN:
            {
                ComPtr<ID3D11DomainShader> shader;
                baseShader->QueryInterface<ID3D11DomainShader>(&shader);
                CHECK_ERROR(shader != NULL, "This is not the right shader type");

                // apply the shader
                context->DSSetShader(shader.Get(), NULL, 0);

                // Apply them to the context
                if (maxCB >= minCB)
                    context->DSSetConstantBuffers(minCB, maxCB - minCB + 1, constantBuffers + minCB);

                if (maxSRV >= minSRV)
                    context->DSSetShaderResources(minSRV, maxSRV - minSRV + 1, shaderResourceViews + minSRV);

                if (maxSS >= minSS)
                    context->DSSetSamplers(minSS, maxSS - minSS + 1, samplers + minSS);

                break;
            }
            case ShaderType::SHADER_PIXEL:
            {
                ComPtr<ID3D11PixelShader> shader;
                baseShader->QueryInterface<ID3D11PixelShader>(&shader);
                CHECK_ERROR(shader != NULL, "This is not the right shader type");

                // apply the shader
                context->PSSetShader(shader.Get(), NULL, 0);

                // Apply them to the context
                if (maxCB >= minCB)
                    context->PSSetConstantBuffers(minCB, maxCB - minCB + 1, constantBuffers + minCB);

                if (maxSRV >= minSRV)
                    context->PSSetShaderResources(minSRV, maxSRV - minSRV + 1, shaderResourceViews + minSRV);

                if (maxSS >= minSS)
                    context->PSSetSamplers(minSS, maxSS - minSS + 1, samplers + minSS);

                if (maxUAV >= minUAV)
                {
                    context->OMSetRenderTargetsAndUnorderedAccessViews(rtvCount, renderTargetViews, depthView, minUAV, maxUAV - minUAV + 1, unorderedAccessViews + minUAV, uavCountersUnused);
                }
                else
                {
                    context->OMSetRenderTargets(rtvCount, renderTargetViews, depthView);
                }

                break;
            }
            }
        }
    }

    void RendererInterfaceD3D11::applyState(const DispatchState &state)
    {
        // Apply the shader. We cast to ID3D11DeviceChild first since that's what the handle is cast to before it was given to the client
        ID3D11DeviceChild *baseShader = (ID3D11DeviceChild *)state.shader;
        ComPtr<ID3D11ComputeShader> computeShader;
        baseShader->QueryInterface<ID3D11ComputeShader>(&computeShader);
        CHECK_ERROR(computeShader != NULL, "This is not a compute shader");

        // apply the shader
        context->CSSetShader(computeShader.Get(), NULL, 0);

        ID3D11ShaderResourceView *shaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
        UINT minSRV = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, maxSRV = 0;

        ID3D11UnorderedAccessView *unorderedAccessViews[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
        UINT minUAV = D3D11_PS_CS_UAV_REGISTER_COUNT, maxUAV = 0;
        UINT uavCountersUnused[D3D11_PS_CS_UAV_REGISTER_COUNT] = {D3D11_KEEP_UNORDERED_ACCESS_VIEWS};

        ID3D11Buffer *constantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_COUNT] = {0};
        UINT minCB = D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_COUNT, maxCB = 0;

        ID3D11SamplerState *samplers[D3D11_COMMONSHADER_SAMPLER_REGISTER_COUNT] = {0};
        UINT minSS = D3D11_COMMONSHADER_SAMPLER_REGISTER_COUNT, maxSS = 0;

        // Bind textures
        for (uint32_t i = 0; i < state.textureBindingCount; i++)
        {
            DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
            UINT dontCareSize;
            UINT slot = (UINT)state.textures[i].slot;
            TextureObjectMap::value_type *resource = (TextureObjectMap::value_type *)state.textures[i].texture;
            switch (state.textures[i].format)
            {
            case Format::R8_UNORM:
                textureFormat = DXGI_FORMAT_R8_UNORM;
                break;
            case Format::R32_UINT:
                textureFormat = DXGI_FORMAT_R32_UINT;
                break;
            case Format::R32_FLOAT:
                textureFormat = DXGI_FORMAT_R32_FLOAT;
                break;
            case Format::RGBA8_UNORM:
                textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            case Format::BGRA8_UNORM:
                textureFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
                break;
            case Format::RGBA16_FLOAT:
                textureFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
                break;
            case Format::X24G8_UINT:
                textureFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
                break;
            case Format::UNKNOWN:
                textureFormat = getTypedTextureFormat(resource->second.textureDesc.format, dontCareSize, true);
                break;
            default:
                CHECK_ERROR(0, "Unknown format");
            }

            // choose a SRV or UAV
            if (state.textures[i].isWritable)
            {
                unorderedAccessViews[slot] = getUAVForTexture(resource, textureFormat, state.textures[i].mipLevel);
                minUAV = std::min(slot, minUAV);
                maxUAV = std::max(slot, maxUAV);
            }
            else
            {
                shaderResourceViews[slot] = getSRVForTexture(resource, textureFormat, state.textures[i].mipLevel);
                minSRV = std::min(slot, minSRV);
                maxSRV = std::max(slot, maxSRV);
            }
        }

        // Bind samplers
        for (uint32_t i = 0; i < state.textureSamplerBindingCount; i++)
        {
            UINT slot = (UINT)state.textureSamplers[i].slot;
            ID3D11SamplerState *sampler = (ID3D11SamplerState *)state.textureSamplers[i].sampler;

            samplers[slot] = sampler;
            minSS = std::min(slot, minSS);
            maxSS = std::max(slot, maxSS);
        }

        // Bind buffers
        for (uint32_t i = 0; i < state.bufferBindingCount; i++)
        {
            UINT slot = (UINT)state.buffers[i].slot;
            BufferObjectMap::value_type *resource = (BufferObjectMap::value_type *)state.buffers[i].buffer;
            // choose a SRV or UAV
            if (state.buffers[i].isWritable)
            {
                unorderedAccessViews[slot] = getUAVForBuffer(resource);
                minUAV = std::min(slot, minUAV);
                maxUAV = std::max(slot, maxUAV);
            }
            else
            {
                shaderResourceViews[slot] = getSRVForBuffer(resource, state.buffers[i].format);
                minSRV = std::min(slot, minSRV);
                maxSRV = std::max(slot, maxSRV);
            }
        }

        // bind Constant buffers
        for (uint32_t i = 0; i < state.constantBufferBindingCount; i++)
        {
            UINT slot = (UINT)state.constantBuffers[i].slot;
            ID3D11Buffer *cbuffer = (ID3D11Buffer *)state.constantBuffers[i].buffer;

            constantBuffers[slot] = cbuffer;
            minCB = std::min(slot, minCB);
            maxCB = std::max(slot, maxCB);
        }

        // Apply them to the context
        if (maxCB >= minCB)
            context->CSSetConstantBuffers(minCB, maxCB - minCB + 1, constantBuffers + minCB);

        if (maxSRV >= minSRV)
            context->CSSetShaderResources(minSRV, maxSRV - minSRV + 1, shaderResourceViews + minSRV);

        if (maxSS >= minSS)
            context->CSSetSamplers(minSS, maxSS - minSS + 1, samplers + minSS);

        if (maxUAV >= minUAV)
            context->CSSetUnorderedAccessViews(minUAV, maxUAV - minUAV + 1, unorderedAccessViews + minUAV, uavCountersUnused);
    }

    void RendererInterfaceD3D11::clearState()
    {
        //
        // Unbind IB and VB
        //

        ID3D11Buffer *pVBs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {0};
        UINT countsAndOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {0};
        context->IASetInputLayout(NULL);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetVertexBuffers(0, ARRAYSIZE(pVBs), pVBs, countsAndOffsets, countsAndOffsets);

        //
        // Unbind shaders
        //
        context->VSSetShader(NULL, NULL, 0);
        context->GSSetShader(NULL, NULL, 0);
        context->PSSetShader(NULL, NULL, 0);
        context->CSSetShader(NULL, NULL, 0);

        //
        // Unbind resources
        //
        ID3D11RenderTargetView *pRTVs[8] = {0};
        context->OMSetRenderTargets(ARRAYSIZE(pRTVs), pRTVs, NULL);

        ID3D11ShaderResourceView *pSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
        context->VSSetShaderResources(0, ARRAYSIZE(pSRVs), pSRVs);
        context->GSSetShaderResources(0, ARRAYSIZE(pSRVs), pSRVs);
        context->PSSetShaderResources(0, ARRAYSIZE(pSRVs), pSRVs);
        context->CSSetShaderResources(0, ARRAYSIZE(pSRVs), pSRVs);

        ID3D11UnorderedAccessView *pUAVs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
        UINT pUAVInitialCounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
        context->CSSetUnorderedAccessViews(0, ARRAYSIZE(pUAVs), pUAVs, pUAVInitialCounts);

        ID3D11Buffer *pCBs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
        context->VSSetConstantBuffers(0, ARRAYSIZE(pCBs), pCBs);
        context->GSSetConstantBuffers(0, ARRAYSIZE(pCBs), pCBs);
        context->PSSetConstantBuffers(0, ARRAYSIZE(pCBs), pCBs);
        context->CSSetConstantBuffers(0, ARRAYSIZE(pCBs), pCBs);

        context->RSSetState(NULL);
    }

    void RendererInterfaceD3D11::debugBeginEvent(char const* name)
    {
        std::u16string u16_name = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(name);
        if (userDefinedAnnotation)
        {
            userDefinedAnnotation->BeginEvent(reinterpret_cast<WCHAR CONST*>(u16_name.c_str()));
        }
    }

    void RendererInterfaceD3D11::debugEndEvent()
    {
        if (userDefinedAnnotation)
        {
            userDefinedAnnotation->EndEvent();
        }
    }

    // This dedudces a TextureDesc from a D3D11 texture. This is called if the client wants information about a texture that it did not create itself.
    TextureDesc RendererInterfaceD3D11::getTextureDescFromD3D11Resource(ID3D11Resource *resource)
    {

        TextureDesc returnValue;
        returnValue.debugName = NULL;

        D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

        // Texture2D?
        ComPtr<ID3D11Texture2D> texture2D;
        resource->QueryInterface<ID3D11Texture2D>(&texture2D);
        if (texture2D != NULL)
        {
            D3D11_TEXTURE2D_DESC desc11;
            texture2D->GetDesc(&desc11);

            usage = desc11.Usage;
            format = desc11.Format;
            returnValue.isCPUWritable = (desc11.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) != 0;
            returnValue.isRenderTarget = (desc11.BindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL)) != 0;
            returnValue.isUAV = (desc11.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0;
            returnValue.mipLevels = (uint32_t)desc11.MipLevels;
            returnValue.sampleCount = (uint32_t)desc11.SampleDesc.Count;
            returnValue.sampleQuality = (uint32_t)desc11.SampleDesc.Quality;

            returnValue.isArray = desc11.ArraySize > 1;
            returnValue.isCubeMap = (desc11.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0;
            // settings .z == 1, would imply a 3d texture unless isArray is set
            returnValue.width = desc11.Width;
            returnValue.height = desc11.Height;
            returnValue.depthOrArraySize = returnValue.isArray ? desc11.ArraySize : 0;
        }
        else
        {
            // Texture3D?
            ComPtr<ID3D11Texture3D> texture3D;
            resource->QueryInterface<ID3D11Texture3D>(&texture3D);
            if (texture3D != NULL)
            {
                D3D11_TEXTURE3D_DESC desc11;
                texture3D->GetDesc(&desc11);

                usage = desc11.Usage;
                format = desc11.Format;
                returnValue.isCPUWritable = (desc11.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) != 0;
                returnValue.isRenderTarget = (desc11.BindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL)) != 0;
                returnValue.isUAV = (desc11.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0;
                returnValue.mipLevels = (uint32_t)desc11.MipLevels;
                returnValue.sampleCount = 1;
                returnValue.sampleQuality = 0;

                returnValue.isArray = false;
                returnValue.width = desc11.Width;
                returnValue.height = desc11.Height;
                returnValue.depthOrArraySize = desc11.Depth;
            }
            else
            {
                CHECK_ERROR(0, "Unknown texture type");
            }
        }

        switch (usage)
        {
        case D3D11_USAGE_DEFAULT:
            returnValue.usage = TextureDesc::USAGE_DEFAULT;
            break;
        case D3D11_USAGE_IMMUTABLE:
            returnValue.usage = TextureDesc::USAGE_IMMUTABLE;
            break;
        case D3D11_USAGE_DYNAMIC:
            returnValue.usage = TextureDesc::USAGE_DYNAMIC;
            break;
        case D3D11_USAGE_STAGING:
            CHECK_ERROR(0, "Staging resources aren't supported");
            break;
        }

        switch (format)
        {
        case DXGI_FORMAT_R8_UINT:
            returnValue.format = Format::R8_UINT;
            break;
        case DXGI_FORMAT_R8_UNORM:
            returnValue.format = Format::R8_UNORM;
            break;
        case DXGI_FORMAT_R8G8_UINT:
            returnValue.format = Format::RG8_UINT;
            break;
        case DXGI_FORMAT_R8G8_UNORM:
            returnValue.format = Format::RG8_UNORM;
            break;
        case DXGI_FORMAT_R16_UINT:
            returnValue.format = Format::R16_UINT;
            break;
        case DXGI_FORMAT_R16_UNORM:
            returnValue.format = Format::R16_UNORM;
            break;
        case DXGI_FORMAT_R16_FLOAT:
            returnValue.format = Format::R16_FLOAT;
            break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            returnValue.format = Format::RGBA8_UNORM;
            break;
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            returnValue.format = Format::RGBA8_UNORM;
            break;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            returnValue.format = Format::BGRA8_UNORM;
            break;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
            returnValue.format = Format::BGRA8_UNORM;
            break;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            returnValue.format = Format::SRGBA8_UNORM;
            break;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            returnValue.format = Format::R10G10B10A2_UNORM;
            break;
        case DXGI_FORMAT_R11G11B10_FLOAT:
            returnValue.format = Format::R11G11B10_FLOAT;
            break;
        case DXGI_FORMAT_R16G16_UINT:
            returnValue.format = Format::RG16_UINT;
            break;
        case DXGI_FORMAT_R16G16_FLOAT:
            returnValue.format = Format::RG16_FLOAT;
            break;
        case DXGI_FORMAT_R32_UINT:
            returnValue.format = Format::R32_UINT;
            break;
        case DXGI_FORMAT_R32_FLOAT:
            returnValue.format = Format::R32_FLOAT;
            break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            returnValue.format = Format::RGBA16_FLOAT;
            break;
        case DXGI_FORMAT_R16G16B16A16_UNORM:
            returnValue.format = Format::RGBA16_UNORM;
            break;
        case DXGI_FORMAT_R16G16B16A16_SNORM:
            returnValue.format = Format::RGBA16_SNORM;
            break;
        case DXGI_FORMAT_R32G32_UINT:
            returnValue.format = Format::RG32_UINT;
            break;
        case DXGI_FORMAT_R32G32_FLOAT:
            returnValue.format = Format::RG32_FLOAT;
            break;
        case DXGI_FORMAT_R32G32B32_UINT:
            returnValue.format = Format::RGB32_UINT;
            break;
        case DXGI_FORMAT_R32G32B32_FLOAT:
            returnValue.format = Format::RGB32_FLOAT;
            break;
        case DXGI_FORMAT_R32G32B32A32_UINT:
            returnValue.format = Format::RGBA32_UINT;
            break;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            returnValue.format = Format::RGBA32_FLOAT;
            break;
        case DXGI_FORMAT_D16_UNORM:
            returnValue.format = Format::D16;
            break;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            returnValue.format = Format::D24S8;
            break;
        case DXGI_FORMAT_R24G8_TYPELESS:
            returnValue.format = Format::D24S8;
            break;
        case DXGI_FORMAT_D32_FLOAT:
            returnValue.format = Format::D32;
            break;
        default:
            CHECK_ERROR(0, "This format is not supported");
        }

        return returnValue;
    }

    RendererInterfaceD3D11::BufferObjectMap::value_type *RendererInterfaceD3D11::getHandleForBuffer(ID3D11Buffer *resource, const BufferDesc *bufferDesc /*= NULL*/)
    {
        if (!resource) // if it's null, we want a null handle
            return NULL;

        BufferObjectMap::iterator it = buffers.find(resource);
        if (it == buffers.end())
        {
            it = buffers.insert(BufferObjectMap::value_type(resource, BufferViewSet())).first; // we haven't seen this one before

            // use our provided one or make one from the D3D data
            it->second.bufferDesc = bufferDesc ? *bufferDesc : getBufferDescFromD3D11Buffer(resource);
        }

        BufferObjectMap::value_type &mapValue = *it;
        return &mapValue;
    }

    BufferDesc RendererInterfaceD3D11::getBufferDescFromD3D11Buffer(ID3D11Buffer *buffer)
    {
        D3D11_BUFFER_DESC desc11;
        buffer->GetDesc(&desc11);

        BufferDesc returnValue;
        returnValue.byteSize = (uint32_t)desc11.ByteWidth;
        returnValue.canHaveUAVs = (desc11.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0;
        returnValue.isCPUWritable = (desc11.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) != 0;
        returnValue.isDrawIndirectArgs = (desc11.MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) != 0;
        returnValue.structStride = (desc11.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) ? (uint32_t)desc11.StructureByteStride : 0;
        return returnValue;
    }

    ID3D11ShaderResourceView *RendererInterfaceD3D11::getSRVForBuffer(BufferObjectMap::value_type *resource, Format::Enum format)
    {
        BufferViewSet &bufferData = resource->second;
        if (bufferData.shaderResourceView)
            return bufferData.shaderResourceView.Get();

        D3D11_SHADER_RESOURCE_VIEW_DESC desc11;
        desc11.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        desc11.BufferEx.FirstElement = 0;
        desc11.BufferEx.Flags = 0;

        if (bufferData.bufferDesc.structStride != 0)
        {
            desc11.Format = DXGI_FORMAT_UNKNOWN;
            desc11.BufferEx.NumElements = bufferData.bufferDesc.byteSize / bufferData.bufferDesc.structStride;
        }
        else
        {
            if (format == Format::UNKNOWN)
                format = Format::R32_UINT;

            UINT elementSize = 0;
            desc11.Format = getTypedTextureFormat(format, elementSize, true);
            desc11.BufferEx.NumElements = bufferData.bufferDesc.byteSize / elementSize;
        }

        CHECK_ERROR(SUCCEEDED(device->CreateShaderResourceView(resource->first.Get(), &desc11, &bufferData.shaderResourceView)), "Creation failed");
        return bufferData.shaderResourceView.Get();
    }

    ID3D11UnorderedAccessView *RendererInterfaceD3D11::getUAVForBuffer(BufferObjectMap::value_type *resource)
    {
        BufferViewSet &bufferData = resource->second;
        if (bufferData.unorderedAccessView)
            return bufferData.unorderedAccessView.Get();

        D3D11_UNORDERED_ACCESS_VIEW_DESC desc11;
        desc11.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        desc11.Format = bufferData.bufferDesc.structStride ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_UINT;
        desc11.Buffer.FirstElement = 0;
        desc11.Buffer.NumElements = bufferData.bufferDesc.byteSize / (bufferData.bufferDesc.structStride ? bufferData.bufferDesc.structStride : 4);
        desc11.Buffer.Flags = 0;

        CHECK_ERROR(SUCCEEDED(device->CreateUnorderedAccessView(resource->first.Get(), &desc11, &bufferData.unorderedAccessView)), "Creation failed");
        return bufferData.unorderedAccessView.Get();
    }

    void RendererInterfaceD3D11::forgetAboutTexture(ID3D11Resource *resource)
    {
        textures.erase(resource);
    }

    void RendererInterfaceD3D11::forgetAboutBuffer(ID3D11Buffer *resource)
    {
        buffers.erase(resource);
    }

    void RendererInterfaceD3D11::clearCachedData()
    {
        textures.clear();
        buffers.clear();

        rasterizerStates.clear();
        blendStates.clear();
        depthStencilStates.clear();

        for (auto query : perfQueries)
            delete query;

        perfQueries.clear();
    }

    namespace
    {
        // Unfortunately we can't memcmp the structs since they have padding bytes in them
        inline bool operator!=(const D3D11_RENDER_TARGET_BLEND_DESC &lhsrt, const D3D11_RENDER_TARGET_BLEND_DESC &rhsrt)
        {
            if (lhsrt.BlendEnable != rhsrt.BlendEnable ||
                lhsrt.SrcBlend != rhsrt.SrcBlend ||
                lhsrt.DestBlend != rhsrt.DestBlend ||
                lhsrt.BlendOp != rhsrt.BlendOp ||
                lhsrt.SrcBlendAlpha != rhsrt.SrcBlendAlpha ||
                lhsrt.DestBlendAlpha != rhsrt.DestBlendAlpha ||
                lhsrt.BlendOpAlpha != rhsrt.BlendOpAlpha ||
                lhsrt.RenderTargetWriteMask != rhsrt.RenderTargetWriteMask)
                return true;
            return false;
        }

        inline bool operator!=(const D3D11_BLEND_DESC &lhs, const D3D11_BLEND_DESC &rhs)
        {
            if (lhs.AlphaToCoverageEnable != rhs.AlphaToCoverageEnable ||
                lhs.IndependentBlendEnable != rhs.IndependentBlendEnable)
                return true;
            for (size_t i = 0; i < sizeof(lhs.RenderTarget) / sizeof(lhs.RenderTarget[0]); i++)
            {
                if (lhs.RenderTarget[i] != rhs.RenderTarget[i])
                    return true;
            }
            return false;
        }

        inline bool operator!=(const D3D11_RASTERIZER_DESC &lhs, const D3D11_RASTERIZER_DESC &rhs)
        {
            if (lhs.FillMode != rhs.FillMode ||
                lhs.CullMode != rhs.CullMode ||
                lhs.FrontCounterClockwise != rhs.FrontCounterClockwise ||
                lhs.DepthBias != rhs.DepthBias ||
                lhs.DepthBiasClamp != rhs.DepthBiasClamp ||
                lhs.SlopeScaledDepthBias != rhs.SlopeScaledDepthBias ||
                lhs.DepthClipEnable != rhs.DepthClipEnable ||
                lhs.ScissorEnable != rhs.ScissorEnable ||
                lhs.MultisampleEnable != rhs.MultisampleEnable ||
                lhs.AntialiasedLineEnable != rhs.AntialiasedLineEnable)
                return true;

            return false;
        }

        inline bool operator!=(const D3D11_DEPTH_STENCILOP_DESC &lhs, const D3D11_DEPTH_STENCILOP_DESC &rhs)
        {
            if (lhs.StencilFailOp != rhs.StencilFailOp ||
                lhs.StencilDepthFailOp != rhs.StencilDepthFailOp ||
                lhs.StencilPassOp != rhs.StencilPassOp ||
                lhs.StencilFunc != rhs.StencilFunc)
                return true;
            return false;
        }
        inline bool operator!=(const D3D11_DEPTH_STENCIL_DESC &lhs, const D3D11_DEPTH_STENCIL_DESC &rhs)
        {
            if (lhs.DepthEnable != rhs.DepthEnable ||
                lhs.DepthWriteMask != rhs.DepthWriteMask ||
                lhs.DepthFunc != rhs.DepthFunc ||
                lhs.StencilEnable != rhs.StencilEnable ||
                lhs.StencilReadMask != rhs.StencilReadMask ||
                lhs.StencilWriteMask != rhs.StencilWriteMask ||
                lhs.FrontFace != rhs.FrontFace ||
                lhs.FrontFace != rhs.BackFace)
                return true;

            return false;
        }
    }

    ID3D11BlendState *RendererInterfaceD3D11::getBlendState(const BlendState &blendState)
    {
        CrcHash hasher;
        hasher.Add(blendState);
        uint32_t hash = hasher.Get();
        ComPtr<ID3D11BlendState> d3dBlendState = blendStates[hash];

        if (d3dBlendState)
            return d3dBlendState.Get();

        D3D11_BLEND_DESC desc11New;
        desc11New.AlphaToCoverageEnable = blendState.alphaToCoverage ? TRUE : FALSE;
        // we always use this and set the states for each target explicitly
        desc11New.IndependentBlendEnable = TRUE;

        for (uint32_t i = 0; i < RenderState::MAX_RENDER_TARGETS; i++)
        {
            desc11New.RenderTarget[i].BlendEnable = blendState.blendEnable[i] ? TRUE : FALSE;
            desc11New.RenderTarget[i].SrcBlend = convertBlendValue(blendState.srcBlend[i]);
            desc11New.RenderTarget[i].DestBlend = convertBlendValue(blendState.destBlend[i]);
            desc11New.RenderTarget[i].BlendOp = convertBlendOp(blendState.blendOp[i]);
            desc11New.RenderTarget[i].SrcBlendAlpha = convertBlendValue(blendState.srcBlendAlpha[i]);
            desc11New.RenderTarget[i].DestBlendAlpha = convertBlendValue(blendState.destBlendAlpha[i]);
            desc11New.RenderTarget[i].BlendOpAlpha = convertBlendOp(blendState.blendOpAlpha[i]);
            desc11New.RenderTarget[i].RenderTargetWriteMask =
                (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_RED ? D3D11_COLOR_WRITE_ENABLE_RED : 0) |
                (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_GREEN ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0) |
                (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_BLUE ? D3D11_COLOR_WRITE_ENABLE_BLUE : 0) |
                (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_ALPHA ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0);
        }

        CHECK_ERROR(SUCCEEDED(device->CreateBlendState(&desc11New, &d3dBlendState)), "Creating blend state failed");

        blendStates[hash] = d3dBlendState;
        return d3dBlendState.Get();
    }

    ID3D11DepthStencilState *RendererInterfaceD3D11::getDepthStencilState(const DepthStencilState &depthState)
    {
        CrcHash hasher;
        hasher.Add(depthState);
        uint32_t hash = hasher.Get();
        ComPtr<ID3D11DepthStencilState> d3dDepthStencilState = depthStencilStates[hash];

        if (d3dDepthStencilState)
            return d3dDepthStencilState.Get();

        D3D11_DEPTH_STENCIL_DESC desc11New;
        desc11New.DepthEnable = depthState.depthEnable ? TRUE : FALSE;
        desc11New.DepthWriteMask = depthState.depthWriteMask == DepthStencilState::DEPTH_WRITE_MASK_ALL ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        desc11New.DepthFunc = convertComparisonFunc(depthState.depthFunc);
        desc11New.StencilEnable = depthState.stencilEnable ? TRUE : FALSE;
        desc11New.StencilReadMask = (UINT8)depthState.stencilReadMask;
        desc11New.StencilWriteMask = (UINT8)depthState.stencilWriteMask;
        desc11New.FrontFace.StencilFailOp = convertStencilOp(depthState.frontFace.stencilFailOp);
        desc11New.FrontFace.StencilDepthFailOp = convertStencilOp(depthState.frontFace.stencilDepthFailOp);
        desc11New.FrontFace.StencilPassOp = convertStencilOp(depthState.frontFace.stencilPassOp);
        desc11New.FrontFace.StencilFunc = convertComparisonFunc(depthState.frontFace.stencilFunc);
        desc11New.BackFace.StencilFailOp = convertStencilOp(depthState.backFace.stencilFailOp);
        desc11New.BackFace.StencilDepthFailOp = convertStencilOp(depthState.backFace.stencilDepthFailOp);
        desc11New.BackFace.StencilPassOp = convertStencilOp(depthState.backFace.stencilPassOp);
        desc11New.BackFace.StencilFunc = convertComparisonFunc(depthState.backFace.stencilFunc);

        CHECK_ERROR(SUCCEEDED(device->CreateDepthStencilState(&desc11New, &d3dDepthStencilState)), "Creating depth-stencil state failed");

        depthStencilStates[hash] = d3dDepthStencilState;
        return d3dDepthStencilState.Get();
    }

    ID3D11RasterizerState *RendererInterfaceD3D11::getRasterizerState(const RasterState &rasterState)
    {
        CrcHash hasher;
        hasher.Add(rasterState);
        uint32_t hash = hasher.Get();
        ComPtr<ID3D11RasterizerState> d3dRasterizerState = rasterizerStates[hash];

        if (d3dRasterizerState)
            return d3dRasterizerState.Get();

        D3D11_RASTERIZER_DESC desc11New;
        switch (rasterState.fillMode)
        {
        case RasterState::FILL_SOLID:
            desc11New.FillMode = D3D11_FILL_SOLID;
            break;
        case RasterState::FILL_LINE:
            desc11New.FillMode = D3D11_FILL_WIREFRAME;
            break;
        default:
            CHECK_ERROR(0, "Unknown fillMode");
        }

        switch (rasterState.cullMode)
        {
        case RasterState::CULL_BACK:
            desc11New.CullMode = D3D11_CULL_BACK;
            break;
        case RasterState::CULL_FRONT:
            desc11New.CullMode = D3D11_CULL_FRONT;
            break;
        case RasterState::CULL_NONE:
            desc11New.CullMode = D3D11_CULL_NONE;
            break;
        default:
            CHECK_ERROR(0, "Unknown cullMode");
        }

        desc11New.FrontCounterClockwise = rasterState.frontCounterClockwise ? TRUE : FALSE;
        desc11New.DepthBias = rasterState.depthBias;
        desc11New.DepthBiasClamp = rasterState.depthBiasClamp;
        desc11New.SlopeScaledDepthBias = rasterState.slopeScaledDepthBias;
        desc11New.DepthClipEnable = rasterState.depthClipEnable ? TRUE : FALSE;
        desc11New.ScissorEnable = rasterState.scissorEnable ? TRUE : FALSE;
        desc11New.MultisampleEnable = rasterState.multisampleEnable ? TRUE : FALSE;
        desc11New.AntialiasedLineEnable = rasterState.antialiasedLineEnable ? TRUE : FALSE;

        bool extendedState = rasterState.conservativeRasterEnable || rasterState.forcedSampleCount || rasterState.programmableSamplePositionsEnable;

        if (extendedState)
        {
            CHECK_ERROR(false, "Without NVAPI support");
        }
        else
        {
            CHECK_ERROR(SUCCEEDED(device->CreateRasterizerState(&desc11New, &d3dRasterizerState)), "Creating rasterizer state failed");
        }

        rasterizerStates[hash] = d3dRasterizerState;
        return d3dRasterizerState.Get();
    }

    D3D11_BLEND RendererInterfaceD3D11::convertBlendValue(BlendState::BlendValue value)
    {
        switch (value)
        {
        case BlendState::BLEND_ZERO:
            return D3D11_BLEND_ZERO;
        case BlendState::BLEND_ONE:
            return D3D11_BLEND_ONE;
        case BlendState::BLEND_SRC_COLOR:
            return D3D11_BLEND_SRC_COLOR;
        case BlendState::BLEND_INV_SRC_COLOR:
            return D3D11_BLEND_INV_SRC_COLOR;
        case BlendState::BLEND_SRC_ALPHA:
            return D3D11_BLEND_SRC_ALPHA;
        case BlendState::BLEND_INV_SRC_ALPHA:
            return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendState::BLEND_DEST_ALPHA:
            return D3D11_BLEND_DEST_ALPHA;
        case BlendState::BLEND_INV_DEST_ALPHA:
            return D3D11_BLEND_INV_DEST_ALPHA;
        case BlendState::BLEND_DEST_COLOR:
            return D3D11_BLEND_DEST_COLOR;
        case BlendState::BLEND_INV_DEST_COLOR:
            return D3D11_BLEND_INV_DEST_COLOR;
        case BlendState::BLEND_SRC_ALPHA_SAT:
            return D3D11_BLEND_SRC_ALPHA_SAT;
        case BlendState::BLEND_BLEND_FACTOR:
            return D3D11_BLEND_BLEND_FACTOR;
        case BlendState::BLEND_INV_BLEND_FACTOR:
            return D3D11_BLEND_INV_BLEND_FACTOR;
        case BlendState::BLEND_SRC1_COLOR:
            return D3D11_BLEND_SRC1_COLOR;
        case BlendState::BLEND_INV_SRC1_COLOR:
            return D3D11_BLEND_INV_SRC1_COLOR;
        case BlendState::BLEND_SRC1_ALPHA:
            return D3D11_BLEND_SRC1_ALPHA;
        case BlendState::BLEND_INV_SRC1_ALPHA:
            return D3D11_BLEND_INV_SRC1_ALPHA;
        default:
            CHECK_ERROR(0, "Unknown blend value");
            return D3D11_BLEND_ZERO;
        }
    }

    D3D11_BLEND_OP RendererInterfaceD3D11::convertBlendOp(BlendState::BlendOp value)
    {
        switch (value)
        {
        case BlendState::BLEND_OP_ADD:
            return D3D11_BLEND_OP_ADD;
        case BlendState::BLEND_OP_SUBTRACT:
            return D3D11_BLEND_OP_SUBTRACT;
        case BlendState::BLEND_OP_REV_SUBTRACT:
            return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendState::BLEND_OP_MIN:
            return D3D11_BLEND_OP_MIN;
        case BlendState::BLEND_OP_MAX:
            return D3D11_BLEND_OP_MAX;
        default:
            CHECK_ERROR(0, "Unknown blend op");
            return D3D11_BLEND_OP_ADD;
        }
    }

    D3D11_STENCIL_OP RendererInterfaceD3D11::convertStencilOp(DepthStencilState::StencilOp value)
    {
        switch (value)
        {
        case DepthStencilState::STENCIL_OP_KEEP:
            return D3D11_STENCIL_OP_KEEP;
        case DepthStencilState::STENCIL_OP_ZERO:
            return D3D11_STENCIL_OP_ZERO;
        case DepthStencilState::STENCIL_OP_REPLACE:
            return D3D11_STENCIL_OP_REPLACE;
        case DepthStencilState::STENCIL_OP_INCR_SAT:
            return D3D11_STENCIL_OP_INCR_SAT;
        case DepthStencilState::STENCIL_OP_DECR_SAT:
            return D3D11_STENCIL_OP_DECR_SAT;
        case DepthStencilState::STENCIL_OP_INVERT:
            return D3D11_STENCIL_OP_INVERT;
        case DepthStencilState::STENCIL_OP_INCR:
            return D3D11_STENCIL_OP_INCR;
        case DepthStencilState::STENCIL_OP_DECR:
            return D3D11_STENCIL_OP_DECR;
        default:
            CHECK_ERROR(0, "Unknown stencil op");
            return D3D11_STENCIL_OP_KEEP;
        }
    }

    D3D11_COMPARISON_FUNC RendererInterfaceD3D11::convertComparisonFunc(DepthStencilState::ComparisonFunc value)
    {
        switch (value)
        {
        case DepthStencilState::COMPARISON_NEVER:
            return D3D11_COMPARISON_NEVER;
        case DepthStencilState::COMPARISON_LESS:
            return D3D11_COMPARISON_LESS;
        case DepthStencilState::COMPARISON_EQUAL:
            return D3D11_COMPARISON_EQUAL;
        case DepthStencilState::COMPARISON_LESS_EQUAL:
            return D3D11_COMPARISON_LESS_EQUAL;
        case DepthStencilState::COMPARISON_GREATER:
            return D3D11_COMPARISON_GREATER;
        case DepthStencilState::COMPARISON_NOT_EQUAL:
            return D3D11_COMPARISON_NOT_EQUAL;
        case DepthStencilState::COMPARISON_GREATER_EQUAL:
            return D3D11_COMPARISON_GREATER_EQUAL;
        case DepthStencilState::COMPARISON_ALWAYS:
            return D3D11_COMPARISON_ALWAYS;
        default:
            CHECK_ERROR(0, "Unknown comparison func");
            return D3D11_COMPARISON_NEVER;
        }
    }

    void RendererInterfaceD3D11::executeRenderThreadCommand(IRenderThreadCommand *onCommand)
    {
        // we have a simple implementation
        onCommand->executeAndDispose();
    }

    void RendererInterfaceD3D11::signalError(const char *file, int line, const char *errorDesc)
    {
        if (errorCB)
            errorCB->signalError(file, line, errorDesc);
        else
            fprintf(stderr, "%s:%i %s\n", file, line, errorDesc);
    }

    D3D_PRIMITIVE_TOPOLOGY RendererInterfaceD3D11::getPrimType(PrimitiveType::Enum pt)
    {
        // setup the primitive type
        switch (pt)
        {
        case PrimitiveType::POINT_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
        case PrimitiveType::TRIANGLE_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case PrimitiveType::TRIANGLE_STRIP:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        case PrimitiveType::PATCH_1_CONTROL_POINT:
            return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
            break;
        case PrimitiveType::PATCH_3_CONTROL_POINT:
            return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
            break;
        default:
            CHECK_ERROR(0, "Unsupported type");
        }
        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

    void RendererInterfaceD3D11::disableSLIResouceSync(ID3D11Resource *)
    {
        // CHECK_ERROR(false, "Without NVAPI support");
    }

    uint32_t RendererInterfaceD3D11::getNumberOfAFRGroups()
    {
        // CHECK_ERROR(false, "Without NVAPI support");
        return 1;
    }

    uint32_t RendererInterfaceD3D11::getAFRGroupOfCurrentFrame(uint32_t)
    {
        CHECK_ERROR(false, "Without NVAPI support");
        return 0;
    }

    PerformanceQueryHandle RendererInterfaceD3D11::createPerformanceQuery(const char *name)
    {
        PerformanceQueryHandle query = new PerformanceQuery();

        CD3D11_QUERY_DESC descTQ(D3D11_QUERY_TIMESTAMP);
        CD3D11_QUERY_DESC descTDQ(D3D11_QUERY_TIMESTAMP_DISJOINT);

        CHECK_ERROR(SUCCEEDED(device->CreateQuery(&descTQ, &query->begin)) &&
                        SUCCEEDED(device->CreateQuery(&descTQ, &query->end)) &&
                        SUCCEEDED(device->CreateQuery(&descTDQ, &query->disjoint)),
                    "Failed to create a query");

        size_t nameLength;
        if (name && (nameLength = strlen(name)) != 0)
        {
            query->name.resize(nameLength);
            MultiByteToWideChar(CP_ACP, 0, name, int(nameLength), &query->name[0], int(nameLength));
        }

        perfQueries.insert(query);

        return query;
    }

    void RendererInterfaceD3D11::destroyPerformanceQuery(PerformanceQueryHandle query)
    {
        if (query)
        {
            perfQueries.erase(query);
            delete query;
        }
    }

    void RendererInterfaceD3D11::beginPerformanceQuery(PerformanceQueryHandle query, bool onlyAnnotation)
    {
        CHECK_ERROR(query->state != PerformanceQuery::STARTED && query->state != PerformanceQuery::ANNOTATION, "Query is already started");

        if (userDefinedAnnotation && !query->name.empty())
            userDefinedAnnotation->BeginEvent(query->name.c_str());

        if (onlyAnnotation)
        {
            query->state = PerformanceQuery::ANNOTATION;
        }
        else
        {
            query->state = PerformanceQuery::STARTED;
            context->Begin(query->disjoint.Get());
            context->End(query->begin.Get());
        }
    }

    void RendererInterfaceD3D11::endPerformanceQuery(PerformanceQueryHandle query)
    {
        CHECK_ERROR(query->state == PerformanceQuery::STARTED || query->state == PerformanceQuery::ANNOTATION, "Query is not started");

        if (userDefinedAnnotation && !query->name.empty())
            userDefinedAnnotation->EndEvent();

        if (query->state == PerformanceQuery::ANNOTATION)
        {
            query->state = PerformanceQuery::RESOLVED;
            query->time = 0.f;
        }
        else
        {
            query->state = PerformanceQuery::FINISHED;
            context->End(query->end.Get());
            context->End(query->disjoint.Get());
        }
    }

    float RendererInterfaceD3D11::getPerformanceQueryTimeMS(PerformanceQueryHandle query)
    {
        CHECK_ERROR(query->state != PerformanceQuery::STARTED, "Query is in progress, can't get time");
        CHECK_ERROR(query->state != PerformanceQuery::NEW, "Query has never been started, can't get time");

        if (query->state == PerformanceQuery::RESOLVED)
            return query->time;

        query->state = PerformanceQuery::RESOLVED;
        query->time = 0.f;

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
        while (context->GetData(query->disjoint.Get(), &disjointData, sizeof(disjointData), 0) == S_FALSE)
            ;

        if (disjointData.Disjoint == FALSE)
        {
            UINT64 startTime = 0;
            while (context->GetData(query->begin.Get(), &startTime, sizeof(startTime), 0) == S_FALSE)
                ;
            UINT64 endTime = 0;
            while (context->GetData(query->end.Get(), &endTime, sizeof(endTime), 0) == S_FALSE)
                ;

            double delta = double(endTime - startTime);
            double frequency = double(disjointData.Frequency);
            query->time = 1000.f * float(delta / frequency);
            return query->time;
        }

        return 0.f;
    }

#define SAFE_RELEASE(p)     \
    {                       \
        if (p)              \
        {                   \
            (p)->Release(); \
            (p) = NULL;     \
        }                   \
    }

#define SAFE_RELEASE_ARRAY(A)                   \
    for (uint32_t i = 0; i < ARRAYSIZE(A); ++i) \
        SAFE_RELEASE(A[i]);

#define D3D11_GET_ARRAY(METHOD, A) \
    context->METHOD(0, ARRAYSIZE(A), A);

#define D3D11_SET_AND_RELEASE_ARRAY(METHOD, A) \
    context->METHOD(0, ARRAYSIZE(A), A);       \
    SAFE_RELEASE_ARRAY(A);

    void UserState::save(ID3D11DeviceContext *context)
    {
        memset(this, 0, sizeof(*this));

        context->IAGetInputLayout(&pInputLayout);
        context->IAGetIndexBuffer(&pIndexBuffer, &IBFormat, &IBOffset);
        context->IAGetPrimitiveTopology(&primitiveTopology);

        numViewports = ARRAYSIZE(viewports);
        context->RSGetViewports(&numViewports, viewports);

        numScissorRects = ARRAYSIZE(scissorRects);
        context->RSGetScissorRects(&numScissorRects, scissorRects);

        context->RSGetState(&pRS);

        context->VSGetShader(&pVS, NULL, 0);
        context->GSGetShader(&pGS, NULL, 0);
        context->PSGetShader(&pPS, NULL, 0);
        context->CSGetShader(&pCS, NULL, 0);

        D3D11_GET_ARRAY(VSGetConstantBuffers, constantBuffersVS);
        D3D11_GET_ARRAY(VSGetShaderResources, shaderResourceViewsVS);
        D3D11_GET_ARRAY(VSGetSamplers, samplersVS);

        D3D11_GET_ARRAY(GSGetConstantBuffers, constantBuffersGS);
        D3D11_GET_ARRAY(GSGetShaderResources, shaderResourceViewsGS);
        D3D11_GET_ARRAY(GSGetSamplers, samplersGS);

        D3D11_GET_ARRAY(PSGetConstantBuffers, constantBuffersPS);
        D3D11_GET_ARRAY(PSGetShaderResources, shaderResourceViewsPS);
        D3D11_GET_ARRAY(PSGetSamplers, samplersPS);

        D3D11_GET_ARRAY(CSGetConstantBuffers, constantBuffersCS);
        D3D11_GET_ARRAY(CSGetShaderResources, shaderResourceViewsCS);
        D3D11_GET_ARRAY(CSGetSamplers, samplersCS);
        D3D11_GET_ARRAY(CSGetUnorderedAccessViews, unorderedAccessViewsCS);

        context->OMGetBlendState(&pBlendState, blendFactor, &sampleMask);
        context->OMGetDepthStencilState(&pDepthStencilState, &stencilRef);
        context->OMGetRenderTargets(ARRAYSIZE(pRTVs), pRTVs, &pDSV);
    }

    void UserState::restore(ID3D11DeviceContext *context)
    {
        context->IASetInputLayout(pInputLayout);
        SAFE_RELEASE(pInputLayout);

        context->IASetIndexBuffer(pIndexBuffer, IBFormat, IBOffset);
        SAFE_RELEASE(pIndexBuffer);

        context->IASetPrimitiveTopology(primitiveTopology);

        context->RSSetViewports(numViewports, viewports);
        context->RSSetScissorRects(numScissorRects, scissorRects);

        context->RSSetState(pRS);
        SAFE_RELEASE(pRS);

        context->VSSetShader(pVS, NULL, 0);
        context->GSSetShader(pGS, NULL, 0);
        context->PSSetShader(pPS, NULL, 0);
        context->CSSetShader(pCS, NULL, 0);
        SAFE_RELEASE(pVS);
        SAFE_RELEASE(pGS);
        SAFE_RELEASE(pPS);
        SAFE_RELEASE(pCS);

        D3D11_SET_AND_RELEASE_ARRAY(VSSetConstantBuffers, constantBuffersVS);
        D3D11_SET_AND_RELEASE_ARRAY(VSSetShaderResources, shaderResourceViewsVS);
        D3D11_SET_AND_RELEASE_ARRAY(VSSetSamplers, samplersVS);

        D3D11_SET_AND_RELEASE_ARRAY(GSSetConstantBuffers, constantBuffersGS);
        D3D11_SET_AND_RELEASE_ARRAY(GSSetShaderResources, shaderResourceViewsGS);
        D3D11_SET_AND_RELEASE_ARRAY(GSSetSamplers, samplersGS);

        D3D11_SET_AND_RELEASE_ARRAY(PSSetConstantBuffers, constantBuffersPS);
        D3D11_SET_AND_RELEASE_ARRAY(PSSetShaderResources, shaderResourceViewsPS);
        D3D11_SET_AND_RELEASE_ARRAY(PSSetSamplers, samplersPS);

        D3D11_SET_AND_RELEASE_ARRAY(CSSetConstantBuffers, constantBuffersCS);
        D3D11_SET_AND_RELEASE_ARRAY(CSSetShaderResources, shaderResourceViewsCS);
        D3D11_SET_AND_RELEASE_ARRAY(CSSetSamplers, samplersCS);

        UINT pUAVInitialCounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
        context->CSSetUnorderedAccessViews(0, ARRAYSIZE(unorderedAccessViewsCS), unorderedAccessViewsCS, pUAVInitialCounts);
        SAFE_RELEASE_ARRAY(unorderedAccessViewsCS);

        context->OMSetBlendState(pBlendState, blendFactor, sampleMask);
        context->OMSetDepthStencilState(pDepthStencilState, stencilRef);
        SAFE_RELEASE(pBlendState);
        SAFE_RELEASE(pDepthStencilState);

        context->OMSetRenderTargets(ARRAYSIZE(pRTVs), pRTVs, pDSV);
        SAFE_RELEASE_ARRAY(pRTVs);
        SAFE_RELEASE(pDSV);
    }

    bool RendererInterfaceD3D11::isOpenGLExtensionSupported(const char *name)
    {
        (void)name;
        return false;
    }

    void *RendererInterfaceD3D11::getOpenGLProcAddress(const char *procname)
    {
        (void)procname;
        return NULL;
    }

} // namespace NVRHI
