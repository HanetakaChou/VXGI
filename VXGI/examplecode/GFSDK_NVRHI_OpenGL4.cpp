/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "GFSDK_NVRHI_OpenGL4.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <sdkddkver.h>
#include <Windows.h>
#include <nmmintrin.h>
#endif

#include <glad/glad_wgl.h>
#include <glad/glad.h>

#include <assert.h>
#include <utility>
#include <string>

#define CHECK_GL_ERROR() checkGLError(__FILE__, __LINE__)
#define SIGNAL_ERROR(msg) m_pErrorCallback->signalError(__FILE__, __LINE__, msg)
#define SIGNAL_ERROR_FMT(...)                                           \
    {                                                                   \
        char __error_buf[4096];                                         \
        sprintf_s(__error_buf, __VA_ARGS__);                            \
        m_pErrorCallback->signalError(__FILE__, __LINE__, __error_buf); \
    }

namespace NVRHI
{
    struct FormatMapping
    {
        Format::Enum abstractFormat;
        GLenum internalFormat;
        GLenum baseFormat;
        GLenum type;
        uint32_t components;
        uint32_t bytesPerPixel;
        bool isDepthStencil;
    };

    // Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
    // https://www.khronos.org/opengles/sdk/docs/man31/html/glTexImage2D.xhtml
    const FormatMapping FormatMappings[] = {
        {Format::UNKNOWN, 0, 0, 0, 0, 0, false},
        {Format::R8_UINT, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 1, 1, false},
        {Format::R8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1, 1, false},
        {Format::RG8_UINT, GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE, 2, 2, false},
        {Format::RG8_UNORM, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2, 2, false},
        {Format::R16_UINT, GL_R16UI, GL_RED_INTEGER, GL_SHORT, 1, 2, false},
        {Format::R16_UNORM, GL_R16, GL_RED, GL_HALF_FLOAT, 1, 2, false},
        {Format::R16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT, 1, 2, false},
        {Format::RGBA8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 4, false},
        {Format::BGRA8_UNORM, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4, 4, false},
        {Format::SRGBA8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 4, false},
        {Format::R10G10B10A2_UNORM, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4, 4, false},
        {Format::R11G11B10_FLOAT, GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 4, 4, false},
        {Format::RG16_UINT, GL_RG16UI, GL_RG_INTEGER, GL_SHORT, 2, 4, false},
        {Format::RG16_FLOAT, GL_RG16F, GL_RG, GL_HALF_FLOAT, 2, 4, false},
        {Format::R32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 1, 4, false},
        {Format::R32_FLOAT, GL_R32F, GL_RED, GL_FLOAT, 1, 4, false},
        {Format::RGBA16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 4, 8, false},
        {Format::RGBA16_UNORM, GL_RGBA16, GL_RGBA, GL_UNSIGNED_INT, 4, 8, false},
        {Format::RGBA16_SNORM, GL_RGBA16_SNORM, GL_RGBA, GL_INT, 4, 8, false},
        {Format::RG32_UINT, GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, 2, 8, false},
        {Format::RG32_FLOAT, GL_RG32F, GL_RG, GL_FLOAT, 2, 8, false},
        {Format::RGB32_UINT, GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, 3, 12, false},
        {Format::RGB32_FLOAT, GL_RGB32F, GL_RGB, GL_FLOAT, 3, 12, false},
        {Format::RGBA32_UINT, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 4, 16, false},
        {Format::RGBA32_FLOAT, GL_RGBA32F, GL_RGBA, GL_FLOAT, 4, 16, false},
        {Format::D16, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 1, 2, true},
        {Format::D24S8, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 2, 4, true},
        {Format::X24G8_UINT, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 2, 4, true},
        {Format::D32, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 1, 4, true},
    };

    const FormatMapping &GetFormatMapping(Format::Enum abstractFormat)
    {
        const FormatMapping &mapping = FormatMappings[abstractFormat];
        assert(mapping.abstractFormat == abstractFormat);
        return mapping;
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

    class Texture
    {
    public:
        TextureDesc desc;
        FormatMapping formatMapping;
        GLuint handle;
        GLenum bindTarget;
        GLuint srgbView;
        bool usedInFrameBuffers;

        Texture()
            : handle(0), bindTarget(0), srgbView(0), usedInFrameBuffers(false)
        {
        }

        ~Texture()
        {
            if (handle)
                glDeleteTextures(1, &handle);
        }
    };

    class Buffer
    {
    public:
        BufferDesc desc;
        GLuint bufferHandle;
        GLuint ssboHandle;
        GLenum bindTarget;

        Buffer()
            : bufferHandle(0), ssboHandle(0), bindTarget(0)
        {
        }

        ~Buffer()
        {
            if (bufferHandle)
                glDeleteBuffers(1, &bufferHandle);

            if (ssboHandle)
                glDeleteTextures(1, &ssboHandle);
        }
    };

    class ConstantBuffer
    {
    public:
        ConstantBufferDesc desc;
        GLuint handle;

        ConstantBuffer()
            : handle(0)
        {
        }

        ~ConstantBuffer()
        {
            if (handle)
                glDeleteBuffers(1, &handle);
        }
    };

    class Sampler
    {
    public:
        SamplerDesc desc;
        GLuint handle;

        Sampler()
            : handle(0)
        {
        }

        ~Sampler()
        {
            if (handle)
                glDeleteSamplers(1, &handle);
        }
    };

    class Shader
    {
    public:
        ShaderDesc desc;
        GLuint handle;

        Shader(ShaderDesc _desc)
            : desc(_desc), handle(0)
        {
        }

        ~Shader()
        {
            if (handle)
                glDeleteProgram(handle);
        }
    };

    class FrameBuffer
    {
    public:
        GLuint handle;
        GLenum drawBuffers[8];
        uint32_t numBuffers;
        TextureHandle depthTarget;
        TextureHandle renderTargets[8];

        FrameBuffer()
            : handle(0), numBuffers(0), depthTarget(nullptr)
        {
            memset(renderTargets, 0, sizeof(renderTargets));
        }

        ~FrameBuffer()
        {
            if (handle)
                glDeleteFramebuffers(1, &handle);
        }
    };

    class InputLayout
    {
    public:
        std::vector<VertexAttributeDesc> attributes;
    };

    RendererInterfaceOGL::RendererInterfaceOGL(IErrorCallback *pErrorCallback)
        : m_pErrorCallback(pErrorCallback), m_nGraphicsPipeline(0), m_nComputePipeline(0), m_nVAO(0), m_bConservativeRasterEnabled(false), m_bForcedSampleCountEnabled(false), m_pCurrentFrameBuffer(nullptr)
    {
        m_DefaultBackBuffer = new Texture();
    }

    RendererInterfaceOGL::~RendererInterfaceOGL()
    {
        for (auto &pair : m_CachedFrameBuffers)
        {
            delete pair.second;
        }

        if (m_nGraphicsPipeline)
        {
            glDeleteProgramPipelines(1, &m_nGraphicsPipeline);
        }

        if (m_nComputePipeline)
        {
            glDeleteProgramPipelines(1, &m_nComputePipeline);
        }

        if (m_nVAO)
        {
            glDeleteVertexArrays(1, &m_nVAO);
        }

        delete m_DefaultBackBuffer;
    }

    void RendererInterfaceOGL::init()
    {
        glGenProgramPipelines(1, &m_nGraphicsPipeline);
        glGenProgramPipelines(1, &m_nComputePipeline);
    }

    bool RendererInterfaceOGL::isOpenGLExtensionSupported(const char *name)
    {
        int n = 0;
        int max = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &max);

        const char *glext = NULL;
        bool bFound = false;

        while (!bFound && n < max)
        {
            glext = (const char *)glGetStringi(GL_EXTENSIONS, n++);
            bFound = (strcmp(glext, name) == 0);

            // char pDebugStr[255];
            // sprintf_s(pDebugStr, "%d. %s\n", n, glext);
            // OutputDebugStringA(pDebugStr);
        }

        return bFound;
    }

    void *RendererInterfaceOGL::getOpenGLProcAddress(const char *procname)
    {
#ifdef _WIN32
        return wglGetProcAddress(procname);
#else
        return nullptr;
#endif
    }

    TextureHandle RendererInterfaceOGL::createTexture(const TextureDesc &d, const void *data)
    {
        const FormatMapping &formatMapping = GetFormatMapping(d.format);

        TextureHandle texture = new Texture();
        texture->desc = d;
        texture->formatMapping = formatMapping;
        glGenTextures(1, &texture->handle);

        uint32_t numlayers = 1;

        if (d.isCubeMap)
        {
            texture->bindTarget = GL_TEXTURE_CUBE_MAP;
            glBindTexture(texture->bindTarget, texture->handle);

            glTexStorage2D(
                texture->bindTarget,
                d.mipLevels,
                formatMapping.internalFormat,
                d.width,
                d.height);

            numlayers = 6;

            CHECK_GL_ERROR();
        }
        else if (d.isArray)
        {
            texture->bindTarget = GL_TEXTURE_2D_ARRAY;
            glBindTexture(texture->bindTarget, texture->handle);

            glTexStorage3D(texture->bindTarget,
                           d.mipLevels,
                           formatMapping.internalFormat,
                           d.width,
                           d.height,
                           d.depthOrArraySize);

            numlayers = d.depthOrArraySize;

            CHECK_GL_ERROR();
        }
        else if (d.depthOrArraySize > 0)
        {
            texture->bindTarget = GL_TEXTURE_3D;
            glBindTexture(texture->bindTarget, texture->handle);

            glTexStorage3D(texture->bindTarget,
                           d.mipLevels,
                           formatMapping.internalFormat,
                           d.width,
                           d.height,
                           d.depthOrArraySize);

            CHECK_GL_ERROR();
        }
        else if (d.sampleCount > 1)
        {
            texture->bindTarget = GL_TEXTURE_2D_MULTISAMPLE;
            glBindTexture(texture->bindTarget, texture->handle);

            glTexStorage2DMultisample(texture->bindTarget,
                                      d.sampleCount,
                                      formatMapping.internalFormat,
                                      d.width,
                                      d.height,
                                      GL_FALSE);

            CHECK_GL_ERROR();
        }
        else
        {
            texture->bindTarget = GL_TEXTURE_2D;
            glBindTexture(texture->bindTarget, texture->handle);

            glTexStorage2D(
                texture->bindTarget,
                d.mipLevels,
                formatMapping.internalFormat,
                d.width,
                d.height);

            CHECK_GL_ERROR();
        }

        if (d.sampleCount == 1)
        {
            glTexParameteri(texture->bindTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(texture->bindTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            CHECK_GL_ERROR();
        }

        glBindTexture(texture->bindTarget, 0);

        if (formatMapping.abstractFormat == Format::SRGBA8_UNORM)
        {
            glGenTextures(1, &texture->srgbView);

            glTextureView(
                texture->srgbView,
                texture->bindTarget,
                texture->handle,
                GL_SRGB8_ALPHA8,
                0,
                texture->desc.mipLevels,
                0,
                numlayers);

            CHECK_GL_ERROR();

            if (d.sampleCount == 1)
            {
                glBindTexture(texture->bindTarget, texture->srgbView);
                glTexParameteri(texture->bindTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(texture->bindTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glBindTexture(texture->bindTarget, 0);
                CHECK_GL_ERROR();
            }
        }

        if (data && d.mipLevels == 1)
        {
            uint32_t numSubresources = 1;
            if (d.isArray || d.isCubeMap)
                numSubresources *= texture->desc.depthOrArraySize;

            uint32_t rowPitch = formatMapping.bytesPerPixel * d.width;
            uint32_t slicePitch = rowPitch * d.height;
            uint32_t subresourcePitch = slicePitch * (d.isArray ? 1 : d.depthOrArraySize);

            for (uint32_t subresource = 0; subresource < numSubresources; subresource++)
            {
                writeTexture(texture, subresource, (char *)data + subresourcePitch * subresource, rowPitch, slicePitch);
            }
        }

        return texture;
    }

    TextureDesc RendererInterfaceOGL::describeTexture(TextureHandle t)
    {
        return t->desc;
    }

    void RendererInterfaceOGL::clearTextureFloat(TextureHandle t, const Color &clearColor)
    {
        for (uint32_t nMipLevel = 0; nMipLevel < t->desc.mipLevels; ++nMipLevel)
        {
            glClearTexImage(t->handle, nMipLevel, t->formatMapping.baseFormat, GL_FLOAT, &clearColor);
            CHECK_GL_ERROR();
        }
    }

    void RendererInterfaceOGL::clearTextureUInt(TextureHandle t, const uint32_t clearColor)
    {
        uint32_t colors[4] = {clearColor, clearColor, clearColor, clearColor};

        for (uint32_t nMipLevel = 0; nMipLevel < t->desc.mipLevels; ++nMipLevel)
        {
            glClearTexImage(t->handle, nMipLevel, t->formatMapping.baseFormat, GL_UNSIGNED_INT, colors);
            CHECK_GL_ERROR();
        }
    }

    void RendererInterfaceOGL::writeTexture(TextureHandle t, uint32_t subresource, const void *data, uint32_t rowPitch, uint32_t depthPitch)
    {
        (void)rowPitch;
        (void)depthPitch;

        glBindTexture(t->bindTarget, t->handle);

        if (t->desc.isArray || t->desc.depthOrArraySize > 0)
        {
            glTexSubImage3D(t->bindTarget, 0, 0, 0, subresource, t->desc.width, t->desc.height, 1, t->formatMapping.baseFormat, t->formatMapping.type, data);
            CHECK_GL_ERROR();
        }
        else
        {
            uint32_t width = __max(1, t->desc.width >> subresource);
            uint32_t height = __max(1, t->desc.height >> subresource);
            glTexSubImage2D(t->bindTarget, subresource, 0, 0, width, height, t->formatMapping.baseFormat, t->formatMapping.type, data);
            CHECK_GL_ERROR();
        }

        glBindTexture(t->bindTarget, 0);
    }

    void RendererInterfaceOGL::destroyTexture(TextureHandle t)
    {
        if (!t)
            return;

        if (t->usedInFrameBuffers)
        {
            std::vector<std::pair<uint32_t, FrameBuffer *>> fbToDelete;
            for (auto it : m_CachedFrameBuffers)
            {
                if (it.second->depthTarget == t)
                    fbToDelete.push_back(it);

                for (uint32_t rt = 0; rt < 8; rt++)
                    if (it.second->renderTargets[rt] == t)
                    {
                        fbToDelete.push_back(it);
                        break;
                    }
            }

            for (auto it : fbToDelete)
            {
                m_CachedFrameBuffers.erase(it.first);
                delete it.second;
            }
        }

        delete t;
    }

    BufferHandle RendererInterfaceOGL::createBuffer(const BufferDesc &d, const void *data)
    {
        BufferHandle buffer = new Buffer();

        buffer->desc = d;
        buffer->bindTarget = d.canHaveUAVs || d.structStride ? GL_SHADER_STORAGE_BUFFER : GL_TEXTURE_BUFFER;

        GLenum usage = d.canHaveUAVs ? GL_STREAM_COPY : GL_STREAM_DRAW;

        glGenBuffers(1, &buffer->bufferHandle);
        glBindBuffer(buffer->bindTarget, buffer->bufferHandle);

        glBufferData(buffer->bindTarget, d.byteSize, data, usage);
        CHECK_GL_ERROR();

        glBindBuffer(buffer->bindTarget, GL_NONE);

        glGenTextures(1, &buffer->ssboHandle);
        glBindTexture(GL_TEXTURE_BUFFER, buffer->ssboHandle);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, buffer->bufferHandle);
        CHECK_GL_ERROR();

        glBindTexture(GL_TEXTURE_BUFFER, GL_NONE);

        return buffer;
    }

    void RendererInterfaceOGL::writeBuffer(BufferHandle b, const void *data, size_t dataSize)
    {
        glBindBuffer(b->bindTarget, b->bufferHandle);

        if (dataSize > b->desc.byteSize)
            dataSize = b->desc.byteSize;

        glBufferSubData(b->bindTarget, 0, dataSize, data);
        CHECK_GL_ERROR();

        glBindBuffer(b->bindTarget, GL_NONE);
    }

    void RendererInterfaceOGL::clearBufferUInt(BufferHandle b, uint32_t clearValue)
    {
        glBindBuffer(b->bindTarget, b->bufferHandle);

        glClearBufferData(b->bindTarget, GL_R32UI, GL_RED, GL_UNSIGNED_INT, &clearValue);
        CHECK_GL_ERROR();

        glBindBuffer(b->bindTarget, GL_NONE);
    }

    void RendererInterfaceOGL::copyToBuffer(BufferHandle dest, uint32_t destOffsetBytes, BufferHandle src, uint32_t srcOffsetBytes, size_t dataSizeBytes)
    {
        glBindBuffer(GL_COPY_WRITE_BUFFER, dest->bufferHandle);
        glBindBuffer(GL_COPY_READ_BUFFER, src->bufferHandle);

        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, srcOffsetBytes, destOffsetBytes, dataSizeBytes);

        CHECK_GL_ERROR();

        glBindBuffer(GL_COPY_READ_BUFFER, GL_NONE);
        glBindBuffer(GL_COPY_WRITE_BUFFER, GL_NONE);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    void RendererInterfaceOGL::readBuffer(BufferHandle b, void *data, size_t *dataSize)
    {
        uint32_t nBytesToRead = uint32_t(*dataSize);
        if (nBytesToRead > b->desc.byteSize)
        {
            nBytesToRead = b->desc.byteSize;
        }

        glBindBuffer(GL_COPY_READ_BUFFER, b->bufferHandle);

        void *pMappedData = glMapBufferRange(GL_COPY_READ_BUFFER, 0, nBytesToRead, GL_MAP_READ_BIT);
        if (pMappedData)
        {
            memcpy(data, pMappedData, nBytesToRead);
            *dataSize = nBytesToRead;
        }
        else
        {
            *dataSize = 0;
        }

        glUnmapBuffer(GL_COPY_READ_BUFFER);
        glBindBuffer(GL_COPY_READ_BUFFER, GL_NONE);
    }

    void RendererInterfaceOGL::destroyBuffer(BufferHandle b)
    {
        if (!b)
            return;
        delete b;
    }

    ConstantBufferHandle RendererInterfaceOGL::createConstantBuffer(const ConstantBufferDesc &d, const void *data)
    {
        ConstantBufferHandle buffer = new ConstantBuffer();

        buffer->desc = d;

        glGenBuffers(1, &buffer->handle);
        glBindBuffer(GL_UNIFORM_BUFFER, buffer->handle);
        glBufferData(GL_UNIFORM_BUFFER, d.byteSize, nullptr, GL_DYNAMIC_DRAW);
        CHECK_GL_ERROR();

        glBindBuffer(GL_UNIFORM_BUFFER, GL_NONE);

        if (data)
            writeConstantBuffer(buffer, data, d.byteSize);

        return buffer;
    }

    void RendererInterfaceOGL::writeConstantBuffer(ConstantBufferHandle b, const void *data, size_t dataSize)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, b->handle);

        glBufferSubData(GL_UNIFORM_BUFFER, 0, dataSize, data);
        CHECK_GL_ERROR();

        glBindBuffer(GL_UNIFORM_BUFFER, GL_NONE);
    }

    void RendererInterfaceOGL::destroyConstantBuffer(ConstantBufferHandle b)
    {
        if (!b)
            return;
        delete b;
    }

    ShaderHandle RendererInterfaceOGL::createShader(const ShaderDesc &d, const void *binary, const size_t binarySize)
    {
        (void)binarySize;

        if (d.preCreationCommand)
            d.preCreationCommand->executeAndDispose();

        ShaderHandle shader = new Shader(d);
        std::string std_string_source = static_cast<char const *>(binary);

#if 1
        // Hack: Fix Compile Error
        {
            std::size_t found;

            found = std_string_source.find("in int gl_InstanceID;");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 2, "//");
            }

            found = std_string_source.find("in int gfsdk_VertexID;");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 2, "//");
            }

            found = std_string_source.find("out uint gl_ViewportIndex;");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 2, "//");
            }

            found = std_string_source.find("nointerpolation uint gl_ViewportIndex SEMANTIC(SV_ViewportArrayIndex);");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 29, "uint unused_viewportIndex; //");
            }

            found = std_string_source.find("bool gl_FrontFacing  SEMANTIC(SV_IsFrontFace);");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 27, "bool unused_FrontFacing; //");
            }

            if (std::string::npos != std_string_source.find("#define EXPLICIT_FAST_GS 0"))
            {
                found = std_string_source.find("#extension GL_NV_viewport_array2 : enable");
                if (std::string::npos != found)
                {
                    std_string_source.replace(found, 2, "//");
                }

                found = std_string_source.find("#extension GL_NV_geometry_shader_passthrough : require");
                if (std::string::npos != found)
                {
                    std_string_source.replace(found, 2, "//");
                }
            }

            found = std_string_source.find("#define gfsdk_ColorOutput gl_FragColor");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 2, "//");
            }

            while (std::string::npos != (found = std_string_source.find("out float4 gl_FragColor;")))
            {
                std_string_source.replace(found, 2, "//");
                std_string_source.insert(found, "layout(location = 0) out float4 gfsdk_ColorOutput;\n");
                std_string_source.insert(found, "#define gl_FragColor gfsdk_ColorOutput\n");
            }

            found = std_string_source.find("out float4 gl_FragData[];");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 2, "//");
            }

            found = std_string_source.find("#define colorX gl_FragData[0]");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 2, "//");
                std_string_source.insert(found, "layout(location = 0) out float4 gfsdk_ColorOutputX;\n");
                std_string_source.insert(found, "#define colorX gfsdk_ColorOutputX\n");
            }

            found = std_string_source.find("#define colorY gl_FragData[1]");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 2, "//");
                std_string_source.insert(found, "layout(location = 1) out float4 gfsdk_ColorOutputY;\n");
                std_string_source.insert(found, "#define colorY gfsdk_ColorOutputY\n");
            }

            found = std_string_source.find("#define colorZ gl_FragData[2]");
            if (std::string::npos != found)
            {
                std_string_source.replace(found, 2, "//");
                std_string_source.insert(found, "layout(location = 2) out float4 gfsdk_ColorOutputZ;\n");
                std_string_source.insert(found, "#define colorZ gfsdk_ColorOutputZ\n");
            }
        }
#endif

        GLenum programType;
        switch (d.shaderType)
        {
        case ShaderType::SHADER_VERTEX:
            programType = GL_VERTEX_SHADER;
            break;
        case ShaderType::SHADER_HULL:
            programType = GL_TESS_CONTROL_SHADER;
            break;
        case ShaderType::SHADER_DOMAIN:
            programType = GL_TESS_EVALUATION_SHADER;
            break;
        case ShaderType::SHADER_GEOMETRY:
            programType = GL_GEOMETRY_SHADER;
            break;
        case ShaderType::SHADER_PIXEL:
            programType = GL_FRAGMENT_SHADER;
            break;
        case ShaderType::SHADER_COMPUTE:
            programType = GL_COMPUTE_SHADER;
            break;
        default:
            SIGNAL_ERROR_FMT("Unrecognized shader type %d", d.shaderType);
            return nullptr;
        }

        char const *source = std_string_source.c_str();

        bool success = false;
        shader->handle = glCreateShaderProgramv(programType, 1, &source);
        CHECK_GL_ERROR();

        if (shader->handle)
        {
            int32_t iRes = 0;
            glGetProgramiv(shader->handle, GL_LINK_STATUS, &iRes);

            if (!iRes)
            {
                int32_t infoLen = 0;
                glGetProgramiv(shader->handle, GL_INFO_LOG_LENGTH, &infoLen);

                if (infoLen > 1)
                {
                    char *infoLog = new char[infoLen];
                    glGetProgramInfoLog(shader->handle, infoLen, nullptr, infoLog);

                    SIGNAL_ERROR_FMT("Failed to compile shader:\n%s", infoLog);

                    delete[] infoLog;
                }
                else
                {
                    SIGNAL_ERROR("Failed to compile shader and get the info log");
                }
            }
            else
            {
                success = true;
            }
        }

        if (d.postCreationCommand)
            d.postCreationCommand->executeAndDispose();

        if (!success)
        {
            delete shader;
            return nullptr;
        }

        return shader;
    }

    void RendererInterfaceOGL::destroyShader(ShaderHandle s)
    {
        if (!s)
            return;
        delete s;
    }

    SamplerHandle RendererInterfaceOGL::createSampler(const SamplerDesc &d)
    {
        SamplerHandle sampler = new Sampler();

        glGenSamplers(1, &sampler->handle);

        glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_S, convertWrapMode(d.wrapMode[0]));
        glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_T, convertWrapMode(d.wrapMode[1]));
        glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_R, convertWrapMode(d.wrapMode[2]));
        glSamplerParameterfv(sampler->handle, GL_TEXTURE_BORDER_COLOR, (const float *)&d.borderColor);
        glSamplerParameterf(sampler->handle, GL_TEXTURE_LOD_BIAS, d.mipBias);

        if (d.anisotropy > 1.f)
        {
            glSamplerParameterf(sampler->handle, GL_TEXTURE_MAX_ANISOTROPY, d.anisotropy);
        }

        if (d.shadowCompare)
        {
            glSamplerParameteri(sampler->handle, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else
        {
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MIN_FILTER, d.mipFilter ? GL_LINEAR_MIPMAP_LINEAR : (d.minFilter ? GL_LINEAR : GL_NEAREST));
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MAG_FILTER, d.magFilter ? GL_LINEAR : GL_NEAREST);
        }

        return sampler;
    }

    void RendererInterfaceOGL::destroySampler(SamplerHandle s)
    {
        if (!s)
            return;
        delete s;
    }

    InputLayoutHandle RendererInterfaceOGL::createInputLayout(const VertexAttributeDesc *d, uint32_t attributeCount, const void *vertexShaderBinary, const size_t binarySize)
    {
        (void)vertexShaderBinary;
        (void)binarySize;

        InputLayoutHandle i = new InputLayout();

        i->attributes.resize(attributeCount);

        for (uint32_t attr = 0; attr < attributeCount; attr++)
            i->attributes[attr] = d[attr];

        return i;
    }

    void RendererInterfaceOGL::destroyInputLayout(InputLayoutHandle i)
    {
        if (!i)
            return;
        delete i;
    }

    void RendererInterfaceOGL::ApplyState(const DrawCallState &state)
    {
        CHECK_GL_ERROR();

        BindVAO();

        size_t nattr = 0;
        if (state.inputLayout)
        {
            for (nattr = 0; nattr < state.inputLayout->attributes.size(); nattr++)
            {
                const VertexAttributeDesc &attr = state.inputLayout->attributes[nattr];
                const VertexBufferBinding *binding = nullptr;

                for (uint32_t buf = 0; buf < state.vertexBufferCount; buf++)
                {
                    if (state.vertexBuffers[buf].slot == attr.bufferIndex)
                    {
                        binding = &state.vertexBuffers[buf];
                        break;
                    }
                }

                if (binding == nullptr)
                {
                    SIGNAL_ERROR_FMT("Vertex buffer for slot %d is not bound", attr.bufferIndex);
                    continue;
                }

                glBindBuffer(GL_ARRAY_BUFFER, binding->buffer->bufferHandle);

                const FormatMapping &formatMapping = GetFormatMapping(attr.format);

                if (formatMapping.type == GL_INT || formatMapping.type == GL_UNSIGNED_INT)
                    glVertexAttribIPointer(
                        GLuint(nattr),
                        GLint(formatMapping.components),
                        formatMapping.type,
                        GLsizei(binding->stride),
                        (const void *)size_t(binding->offset + attr.offset));
                else
                    glVertexAttribPointer(
                        GLuint(nattr),
                        GLint(formatMapping.components),
                        formatMapping.type,
                        GL_TRUE,
                        GLsizei(binding->stride),
                        (const void *)size_t(binding->offset + attr.offset));

                glVertexAttribDivisor(GLuint(nattr), attr.isInstanced ? 1 : 0);
                glEnableVertexAttribArray(GLuint(nattr));
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

        static GLint nMaxVertexAttrs = 0;
        if (nMaxVertexAttrs == 0)
            glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &nMaxVertexAttrs);

        for (; int(nattr) < nMaxVertexAttrs; nattr++)
            glDisableVertexAttribArray(GLuint(nattr));

        const RenderState &renderState = state.renderState;

        CHECK_GL_ERROR();

        SetShaders(state);
        BindShaderResources(state);
        BindRenderTargets(renderState);

        SetRasterState(renderState.rasterState); // requires a bound framebuffer for programmable sample positions
        SetBlendState(renderState.blendState, state.renderState.targetCount);
        SetDepthStencilState(renderState.depthStencilState);

        ClearRenderTargets(renderState); // requires the correct depth and maybe blend state
    }

    void RendererInterfaceOGL::BindVAO()
    {
        if (!m_nVAO)
        {
            glGenVertexArrays(1, &m_nVAO);
        }

        glBindVertexArray(m_nVAO);
    }

    void RendererInterfaceOGL::BindRenderTargets(const RenderState &renderState)
    {
        FrameBuffer *framebuffer = GetCachedFrameBuffer(renderState);

        if (framebuffer != m_pCurrentFrameBuffer)
        {
            if (framebuffer)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->handle);
                glDrawBuffers(framebuffer->numBuffers, framebuffer->drawBuffers);
            }
            else
            {
                glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
            }

            m_pCurrentFrameBuffer = framebuffer;
        }

        // setting scissor and viewports
        if (!m_bCurrentViewportsValid || memcmp(m_vCurrentViewports, renderState.viewports, renderState.viewportCount * sizeof(NVRHI::Viewport)) != 0 || memcmp(m_vCurrentScissorRects, renderState.scissorRects, renderState.viewportCount * sizeof(NVRHI::Rect)) != 0)
        {
            m_bCurrentViewportsValid = true;
            memcpy(m_vCurrentViewports, renderState.viewports, renderState.viewportCount * sizeof(NVRHI::Viewport));
            memcpy(m_vCurrentScissorRects, renderState.scissorRects, renderState.viewportCount * sizeof(NVRHI::Rect));

            for (uint32_t rt = 0; rt < renderState.viewportCount; rt++)
            {
                float vx = renderState.viewports[rt].minX;
                float vy = renderState.viewports[rt].minY;
                float vw = renderState.viewports[rt].maxX - renderState.viewports[rt].minX;
                float vh = renderState.viewports[rt].maxY - renderState.viewports[rt].minY;

                int32_t sx = renderState.scissorRects[rt].minX;
                int32_t sy = renderState.scissorRects[rt].minY;
                int32_t sw = renderState.scissorRects[rt].maxX - renderState.scissorRects[rt].minX;
                int32_t sh = renderState.scissorRects[rt].maxY - renderState.scissorRects[rt].minY;

                glViewportIndexedf(rt, vx, vy, vw, vh);
                glDepthRangeIndexed(rt, renderState.viewports[rt].minZ, renderState.viewports[rt].maxZ);
                glScissorIndexed(rt, sx, sy, sw, sh);
            }
        }
    }

    void RendererInterfaceOGL::ClearRenderTargets(const RenderState &renderState)
    {
        uint32_t nClearBitField = 0;
        if (renderState.clearColorTarget)
        {
            nClearBitField = GL_COLOR_BUFFER_BIT;
            glClearColor(renderState.clearColor.r, renderState.clearColor.g, renderState.clearColor.b, renderState.clearColor.a);
        }

        if (renderState.clearDepthTarget)
        {
            nClearBitField |= GL_DEPTH_BUFFER_BIT;
            glClearDepthf(renderState.clearDepth);
        }

        if (renderState.clearStencilTarget)
        {
            nClearBitField |= GL_STENCIL_BUFFER_BIT;
            glClearStencil(renderState.clearStencil);
        }

        if (nClearBitField)
        {
            glClear(nClearBitField);
        }
    }

    FrameBuffer *RendererInterfaceOGL::GetCachedFrameBuffer(const RenderState &renderState)
    {
        if (renderState.targetCount == 1 && renderState.targets[0] == m_DefaultBackBuffer && renderState.depthTarget == nullptr)
        {
            // Special case: back buffer rendering
            return nullptr;
        }

        CrcHash hasher;
        for (uint32_t rt = 0; rt < renderState.targetCount; rt++)
        {
            hasher.Add(renderState.targets[rt]);
            hasher.Add(renderState.targetIndicies[rt]);
            hasher.Add(renderState.targetMipSlices[rt]);
        }
        hasher.Add(renderState.depthTarget);
        hasher.Add(renderState.depthIndex);
        hasher.Add(renderState.depthMipSlice);
        uint32_t hash = hasher.Get();

        auto it = m_CachedFrameBuffers.find(hash);
        if (it != m_CachedFrameBuffers.end())
            return it->second;

        FrameBuffer *framebuffer = new FrameBuffer();

        glGenFramebuffers(1, &framebuffer->handle);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->handle);

        for (uint32_t rt = 0; rt < renderState.targetCount; rt++)
        {
            if (renderState.targets[rt] == m_DefaultBackBuffer)
            {
                SIGNAL_ERROR("Cannot bind the default back buffer unless it's the only render target bound, no depth");
            }
            else if (renderState.targets[rt] != nullptr)
            {
                framebuffer->renderTargets[rt] = renderState.targets[rt];
                framebuffer->renderTargets[rt]->usedInFrameBuffers = true;

                if (renderState.targetIndicies[rt] == ~0u || renderState.targets[rt]->desc.depthOrArraySize == 0)
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + rt, renderState.targets[rt]->bindTarget, renderState.targets[rt]->handle, renderState.targetMipSlices[rt]);
                else
                    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + rt, renderState.targets[rt]->handle, renderState.targetMipSlices[rt], renderState.targetIndicies[rt]);

                framebuffer->drawBuffers[(framebuffer->numBuffers)++] = GL_COLOR_ATTACHMENT0 + rt;
            }
        }

        if (renderState.depthTarget)
        {
            framebuffer->depthTarget = renderState.depthTarget;
            framebuffer->depthTarget->usedInFrameBuffers = true;

            GLenum attachment;

            if (renderState.depthTarget->desc.format == Format::D24S8)
                attachment = GL_DEPTH_STENCIL_ATTACHMENT;
            else
                attachment = GL_DEPTH_ATTACHMENT;

            if (renderState.depthIndex == ~0u || renderState.depthTarget->desc.depthOrArraySize == 0)
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, renderState.depthTarget->bindTarget, renderState.depthTarget->handle, renderState.depthMipSlice);
            else
                glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, renderState.depthTarget->handle, renderState.depthMipSlice, renderState.depthIndex);
        }

        CHECK_GL_ERROR();

        uint32_t status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            SIGNAL_ERROR("Incomplete framebuffer!");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_CachedFrameBuffers[hash] = framebuffer;

        return framebuffer;
    }

    void RendererInterfaceOGL::SetShaders(const DrawCallState &state)
    {
        glUseProgramStages(m_nGraphicsPipeline, GL_VERTEX_SHADER_BIT, state.VS.shader ? state.VS.shader->handle : GL_NONE);
        glUseProgramStages(m_nGraphicsPipeline, GL_TESS_CONTROL_SHADER_BIT, state.HS.shader ? state.HS.shader->handle : GL_NONE);
        glUseProgramStages(m_nGraphicsPipeline, GL_TESS_EVALUATION_SHADER_BIT, state.DS.shader ? state.DS.shader->handle : GL_NONE);
        glUseProgramStages(m_nGraphicsPipeline, GL_GEOMETRY_SHADER_BIT, state.GS.shader ? state.GS.shader->handle : GL_NONE);
        glUseProgramStages(m_nGraphicsPipeline, GL_FRAGMENT_SHADER_BIT, state.PS.shader ? state.PS.shader->handle : GL_NONE);

        glBindProgramPipeline(m_nGraphicsPipeline);
    }

    void RendererInterfaceOGL::BindShaderResources(const PipelineStageBindings &state)
    {
        CHECK_GL_ERROR();

        // binding textures
        for (uint32_t nBinding = 0; nBinding < state.textureBindingCount; ++nBinding)
        {
            const TextureBinding &binding = state.textures[nBinding];

            if (binding.texture == m_DefaultBackBuffer)
            {
                SIGNAL_ERROR("Cannot bind the default back buffer as a texture or image");
            }
            else if (binding.texture)
            {
                if (binding.isWritable)
                {
                    GLenum format = binding.texture->formatMapping.internalFormat;
                    if (binding.format != Format::UNKNOWN)
                        format = GetFormatMapping(binding.format).internalFormat;

                    glBindImageTexture(binding.slot, binding.texture->handle, binding.mipLevel, GL_TRUE, 0, GL_READ_WRITE, format);
                    CHECK_GL_ERROR();

                    m_vecBoundImages.push_back(binding.slot);
                }
                else
                {
                    glActiveTexture(GL_TEXTURE0 + binding.slot);

                    if (binding.texture->formatMapping.abstractFormat == Format::SRGBA8_UNORM)
                        glBindTexture(binding.texture->bindTarget, binding.texture->srgbView);
                    else
                        glBindTexture(binding.texture->bindTarget, binding.texture->handle);

                    CHECK_GL_ERROR();

                    m_vecBoundTextures.push_back(std::make_pair(binding.slot, binding.texture->bindTarget));
                }
            }
        }

        // binding samplers
        for (uint32_t nSampler = 0; nSampler < state.textureSamplerBindingCount; ++nSampler)
        {
            const SamplerBinding &binding = state.textureSamplers[nSampler];

            glBindSampler(binding.slot, binding.sampler->handle);
            m_vecBoundSamplers.push_back(binding.slot);
        }

        // binding constant buffers
        for (uint32_t i = 0; i < state.constantBufferBindingCount; i++)
        {
            const ConstantBufferBinding &binding = state.constantBuffers[i];

            glBindBufferBase(GL_UNIFORM_BUFFER, binding.slot, binding.buffer->handle);
            m_vecBoundConstantBuffers.push_back(binding.slot);
        }

        // binding ssbo`s
        for (uint32_t nBuffer = 0; nBuffer < state.bufferBindingCount; ++nBuffer)
        {
            const BufferBinding &binding = state.buffers[nBuffer];

            if (binding.isWritable || binding.buffer->desc.structStride > 0)
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding.slot, binding.buffer->bufferHandle);
                m_vecBoundBuffers.push_back(binding.slot);
            }
            else
            {
                glActiveTexture(GL_TEXTURE0 + binding.slot);
                glBindTexture(GL_TEXTURE_BUFFER, binding.buffer->ssboHandle);
                glActiveTexture(GL_TEXTURE0);
                m_vecBoundTextures.push_back(std::make_pair(binding.slot, GL_TEXTURE_BUFFER));
            }
        }
    }

    void RendererInterfaceOGL::BindShaderResources(const DrawCallState &state)
    {
        BindShaderResources(state.VS);
        BindShaderResources(state.HS);
        BindShaderResources(state.DS);
        BindShaderResources(state.GS);
        BindShaderResources(state.PS);
    }

    void RendererInterfaceOGL::SetRasterState(const RasterState &rasterState)
    {
        switch (rasterState.fillMode)
        {
        case RasterState::FILL_LINE:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            break;
        case RasterState::FILL_SOLID:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;

        default:
            SIGNAL_ERROR_FMT("Unknown fill mode specified %d", rasterState.fillMode);
            break;
        }

        switch (rasterState.cullMode)
        {
        case RasterState::CULL_BACK:
            glCullFace(GL_BACK);
            glEnable(GL_CULL_FACE);
            break;
        case RasterState::CULL_FRONT:
            glCullFace(GL_FRONT);
            glEnable(GL_CULL_FACE);
            break;
        case RasterState::CULL_NONE:
            glDisable(GL_CULL_FACE);
            break;
        default:
            SIGNAL_ERROR_FMT("Unknown cullMode %d", rasterState.cullMode);
        }

        glFrontFace(rasterState.frontCounterClockwise ? GL_CCW : GL_CW);

        if (rasterState.depthClipEnable)
        {
            glEnable(GL_DEPTH_CLAMP);
        }

        if (rasterState.scissorEnable)
        {
            glEnable(GL_SCISSOR_TEST);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }

        if (rasterState.depthBias != 0 || rasterState.slopeScaledDepthBias != 0.f)
        {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(rasterState.slopeScaledDepthBias, float(rasterState.depthBias));
        }

        if (rasterState.multisampleEnable)
        {
            glEnable(GL_MULTISAMPLE);
            glSampleMaski(0, ~0u);
            CHECK_GL_ERROR();
        }
        else
        {
            glDisable(GL_MULTISAMPLE);
        }

        if (rasterState.antialiasedLineEnable)
        {
            SIGNAL_ERROR("Antialiased line rasterizer state requested");
        }

        if (rasterState.conservativeRasterEnable)
        {
            glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);
            m_bConservativeRasterEnabled = true;
            CHECK_GL_ERROR();
        }

        if (rasterState.forcedSampleCount)
        {
            if (glRasterSamplesEXT)
            {
                glEnable(GL_RASTER_MULTISAMPLE_EXT);
                glRasterSamplesEXT(rasterState.forcedSampleCount, TRUE);
                m_bForcedSampleCountEnabled = true;
                CHECK_GL_ERROR();
            }
            else
            {
                SIGNAL_ERROR("Trying to use forcedSampleCount but glRasterSamplesEXT function is NULL");
            }
        }

        if (rasterState.programmableSamplePositionsEnable)
        {
            if (glFramebufferSampleLocationsfvNV)
            {
                float locations[32];
                for (int pt = 0; pt < 16; pt++)
                {
                    locations[pt * 2 + 0] = float(rasterState.samplePositionsX[pt]) / 16.0f;
                    locations[pt * 2 + 1] = float(rasterState.samplePositionsY[pt]) / 16.0f;
                }

                glFramebufferSampleLocationsfvNV(GL_FRAMEBUFFER, 0, rasterState.forcedSampleCount, locations);
                glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_PROGRAMMABLE_SAMPLE_LOCATIONS_NV, 1);
                CHECK_GL_ERROR();
            }
            else
            {
                SIGNAL_ERROR("Trying to use programmableSamplePositions but glFramebufferSampleLocationsfvNV function is NULL");
            }
        }
    }

    void RendererInterfaceOGL::SetBlendState(const BlendState &blendState, uint32_t targetCount)
    {
        if (blendState.alphaToCoverage)
        {
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }

        for (uint32_t i = 0; i < targetCount; ++i)
        {
            if (blendState.blendEnable[i])
                glEnablei(GL_BLEND, i);

            uint32_t BlendOpRGB = convertBlendOp(blendState.blendOp[i]);
            uint32_t BlendOpAlpha = convertBlendOp(blendState.blendOpAlpha[i]);
            glBlendEquationSeparatei(i, BlendOpRGB, BlendOpAlpha);

            uint32_t SrcBlendRGB = convertBlendValue(blendState.srcBlend[i]);
            uint32_t DstBlendRGB = convertBlendValue(blendState.destBlend[i]);
            uint32_t SrcBlendAlpha = convertBlendValue(blendState.srcBlendAlpha[i]);
            uint32_t DstBlendAlpha = convertBlendValue(blendState.destBlendAlpha[i]);
            glBlendFuncSeparatei(i, SrcBlendRGB, DstBlendRGB, SrcBlendAlpha, DstBlendAlpha);

            glColorMaski(i,
                         (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_RED) != 0,
                         (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_GREEN) != 0,
                         (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_BLUE) != 0,
                         (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_ALPHA) != 0);
        }
    }

    void RendererInterfaceOGL::SetDepthStencilState(const DepthStencilState &depthState)
    {
        if (depthState.depthEnable)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthMask((depthState.depthWriteMask == DepthStencilState::DEPTH_WRITE_MASK_ALL) ? GL_TRUE : GL_FALSE);
            glDepthFunc(convertComparisonFunc(depthState.depthFunc));
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }

        if (depthState.stencilEnable)
        {
            glEnable(GL_STENCIL_TEST);

            glStencilFuncSeparate(GL_FRONT, convertComparisonFunc(depthState.frontFace.stencilFunc), depthState.stencilRefValue, depthState.stencilReadMask);
            glStencilOpSeparate(GL_FRONT, convertStencilOp(depthState.frontFace.stencilFailOp),
                                convertStencilOp(depthState.frontFace.stencilDepthFailOp),
                                convertStencilOp(depthState.frontFace.stencilPassOp));

            glStencilFuncSeparate(GL_BACK, convertComparisonFunc(depthState.backFace.stencilFunc), depthState.stencilRefValue, depthState.stencilReadMask);
            glStencilOpSeparate(GL_BACK, convertStencilOp(depthState.backFace.stencilFailOp),
                                convertStencilOp(depthState.backFace.stencilDepthFailOp),
                                convertStencilOp(depthState.backFace.stencilPassOp));

            glStencilMask(depthState.stencilWriteMask);
        }
        else
        {
            glDisable(GL_STENCIL_TEST);
        }
    }

    void RendererInterfaceOGL::draw(const DrawCallState &state, const DrawArguments *args, uint32_t numDrawCalls)
    {
        ApplyState(state);

        uint32_t nPrimType = convertPrimType(state.primType);

        for (uint32_t n = 0; n < numDrawCalls; n++)
        {
            glDrawArraysInstanced(nPrimType, args[n].startVertexLocation, args[n].vertexCount, args[n].instanceCount);
            CHECK_GL_ERROR();
        }

        RestoreDefaultState();

        CHECK_GL_ERROR();
    }

    void RendererInterfaceOGL::drawIndexed(const DrawCallState &state, const DrawArguments *args, uint32_t numDrawCalls)
    {
        ApplyState(state);

        if (state.indexBuffer)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.indexBuffer->bufferHandle);
        }

        uint32_t nPrimType = convertPrimType(state.primType);

        for (uint32_t n = 0; n < numDrawCalls; n++)
        {
            uint32_t indexOffset = args[n].startIndexLocation * 4 + state.indexBufferOffset;
            glDrawElementsBaseVertex(nPrimType, args[n].vertexCount, GL_UNSIGNED_INT, (const void *)size_t(indexOffset), args[n].startVertexLocation);
            CHECK_GL_ERROR();
        }

        RestoreDefaultState();

        if (state.indexBuffer)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_NONE);
        }

        CHECK_GL_ERROR();
    }

    void RendererInterfaceOGL::drawIndirect(const DrawCallState &state, BufferHandle indirectParams, uint32_t offsetBytes)
    {
        ApplyState(state);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectParams->bufferHandle);

        uint32_t nPrimType = convertPrimType(state.primType);
        glDrawArraysIndirect(nPrimType, (const void *)size_t(offsetBytes));
        CHECK_GL_ERROR();

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, GL_NONE);

        RestoreDefaultState();
    }

    void RendererInterfaceOGL::dispatch(const NVRHI::DispatchState &state, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        ApplyState(state);

        glDispatchCompute(groupsX, groupsY, groupsZ);

        // TODO:
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

        CHECK_GL_ERROR();

        RestoreDefaultState();
    }

    void RendererInterfaceOGL::dispatchIndirect(const DispatchState &state, BufferHandle indirectParams, uint32_t offsetBytes)
    {
        ApplyState(state);

        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectParams->bufferHandle);

        glDispatchComputeIndirect(offsetBytes);

        CHECK_GL_ERROR();

        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, GL_NONE);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

        RestoreDefaultState();
    }

    void RendererInterfaceOGL::ApplyState(const DispatchState &state)
    {
        glUseProgramStages(m_nComputePipeline, GL_COMPUTE_SHADER_BIT, state.shader->handle);
        glBindProgramPipeline(m_nComputePipeline);

        BindShaderResources(state);
    }

    void RendererInterfaceOGL::checkGLError(const char *file, int line)
    {
        GLint error = glGetError();

        if (error)
        {
            const char *errorString = 0;

            switch (error)
            {
            case GL_INVALID_ENUM:
                errorString = "GL_INVALID_ENUM";
                break;

            case GL_INVALID_FRAMEBUFFER_OPERATION:
                errorString = "GL_INVALID_FRAMEBUFFER_OPERATION";
                break;

            case GL_INVALID_VALUE:
                errorString = "GL_INVALID_VALUE";
                break;

            case GL_INVALID_OPERATION:
                errorString = "GL_INVALID_OPERATION";
                break;

            case GL_OUT_OF_MEMORY:
                errorString = "GL_OUT_OF_MEMORY";
                break;

            default:
                errorString = "unknown GL error";
                break;
            }

            m_pErrorCallback->signalError(file, line, errorString);
        }
    }

    void RendererInterfaceOGL::executeRenderThreadCommand(IRenderThreadCommand *onCommand)
    {
        // we have a simple implementation
        onCommand->executeAndDispose();
    }

    void RendererInterfaceOGL::RestoreDefaultState()
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glDisable(GL_POLYGON_OFFSET_FILL);

        glDisable(GL_BLEND);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_CLAMP);

        // restoring stencil values
        glDisable(GL_STENCIL_TEST);
        glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, (uint32_t)-1);
        glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, (uint32_t)-1);
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask((uint32_t)-1);

        // unbinding shader resources

        // samplers
        for (uint32_t nSampler = 0; nSampler < m_vecBoundSamplers.size(); ++nSampler)
        {
            glBindSampler(m_vecBoundSamplers[nSampler], GL_NONE);
        }
        m_vecBoundSamplers.resize(0);

        // Textures
        for (uint32_t nTexture = 0; nTexture < m_vecBoundTextures.size(); ++nTexture)
        {
            glActiveTexture(GL_TEXTURE0 + m_vecBoundTextures[nTexture].first);
            CHECK_GL_ERROR();

            glBindTexture(m_vecBoundTextures[nTexture].second, GL_NONE);
            CHECK_GL_ERROR();
        }

        glActiveTexture(GL_TEXTURE0);
        m_vecBoundTextures.resize(0);

        // images
        for (uint32_t nImage = 0; nImage < m_vecBoundImages.size(); nImage++)
        {
            glBindImageTexture(m_vecBoundImages[nImage], GL_NONE, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
            CHECK_GL_ERROR();
        }
        m_vecBoundImages.resize(0);

        // constant buffers
        for (uint32_t nCB = 0; nCB < m_vecBoundConstantBuffers.size(); nCB++)
        {
            glBindBufferBase(GL_UNIFORM_BUFFER, m_vecBoundConstantBuffers[nCB], GL_NONE);
        }
        m_vecBoundConstantBuffers.resize(0);

        // buffers
        for (uint32_t nBuffer = 0; nBuffer < m_vecBoundBuffers.size(); nBuffer++)
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_vecBoundBuffers[nBuffer], GL_NONE);
        }
        m_vecBoundBuffers.resize(0);

        glBindVertexArray(GL_NONE);

        glBindProgramPipeline(GL_NONE);

        if (m_bConservativeRasterEnabled)
        {
            glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);
            m_bConservativeRasterEnabled = false;
        }

        if (m_bForcedSampleCountEnabled)
        {
            glDisable(GL_RASTER_MULTISAMPLE_EXT);
            m_bForcedSampleCountEnabled = false;
        }
    }

    void RendererInterfaceOGL::UnbindFrameBuffer()
    {
        if (m_pCurrentFrameBuffer)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
            m_pCurrentFrameBuffer = nullptr;
        }

        m_bCurrentViewportsValid = false;
    }

    NVRHI::TextureHandle RendererInterfaceOGL::getHandleForTexture(uint32_t target, uint32_t texture)
    {
        for (auto it : m_NonManagedTextures)
            if (it->handle == texture)
                return it;

        TextureHandle t = new Texture();

        glBindTexture(target, texture);

        GLenum internalFormat = 0;
        glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, (GLint *)&internalFormat);

        for (uint32_t n = 0; n < _countof(FormatMappings); n++)
        {
            const FormatMapping &formatMapping = FormatMappings[n];
            if (formatMapping.internalFormat == internalFormat)
            {
                t->formatMapping = formatMapping;
                t->desc.format = formatMapping.abstractFormat;
                break;
            }
        }

        if (t->desc.format == Format::UNKNOWN)
        {
            SIGNAL_ERROR_FMT("Failed to decode GL texture format 0x%04x", internalFormat);
            delete t;
            return nullptr;
        }

        t->bindTarget = target;
        t->handle = texture;

        glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, (GLint *)&t->desc.width);
        glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, (GLint *)&t->desc.height);
        glGetTexLevelParameteriv(target, 0, GL_TEXTURE_DEPTH, (GLint *)&t->desc.depthOrArraySize);
        glGetTexLevelParameteriv(target, 0, GL_TEXTURE_SAMPLES, (GLint *)&t->desc.sampleCount);

        // GL_TEXTURE_MAX_LEVEL returns 1000 unless the app sets the actual number of mip levels...
        // glGetTexParameteriv(target, GL_TEXTURE_MAX_LEVEL, (GLint*)&t->desc.mipLevels);
        t->desc.mipLevels = 1;

        switch (target)
        {
        case GL_TEXTURE_2D_ARRAY:
            t->desc.isArray = true;
            break;
        case GL_TEXTURE_CUBE_MAP:
            t->desc.isCubeMap = true;
            break;
        case GL_TEXTURE_2D:
        case GL_TEXTURE_2D_MULTISAMPLE:
            t->desc.depthOrArraySize = 0;
            break;
        }

        m_NonManagedTextures.push_back(t);

        return t;
    }

    uint32_t RendererInterfaceOGL::getTextureOpenGLName(NVRHI::TextureHandle t)
    {
        if (!t)
            return 0;
        return t->handle;
    }

    void RendererInterfaceOGL::releaseNonManagedTextures()
    {
        for (auto t : m_NonManagedTextures)
        {
            t->handle = 0; // prevent glDeleteTextures call in ~Texture
            delete t;
        }

        m_NonManagedTextures.clear();
    }

    void RendererInterfaceOGL::debugBeginEvent(char const *name)
    {
        if (glPushGroupMarkerEXT)
        {
            glPushGroupMarkerEXT(static_cast<GLsizei>(strlen(name)), name);
        }
    }

    void RendererInterfaceOGL::debugEndEvent()
    {
        if (glPopGroupMarkerEXT)
        {
            glPopGroupMarkerEXT();
        }
    }

    uint32_t RendererInterfaceOGL::convertStencilOp(DepthStencilState::StencilOp value)
    {
        switch (value)
        {
        case DepthStencilState::STENCIL_OP_KEEP:
            return GL_KEEP;
        case DepthStencilState::STENCIL_OP_ZERO:
            return GL_ZERO;
        case DepthStencilState::STENCIL_OP_REPLACE:
            return GL_REPLACE;
        case DepthStencilState::STENCIL_OP_INCR_SAT:
            return GL_INCR_WRAP;
        case DepthStencilState::STENCIL_OP_DECR_SAT:
            return GL_DECR_WRAP;
        case DepthStencilState::STENCIL_OP_INVERT:
            return GL_INVERT;
        case DepthStencilState::STENCIL_OP_INCR:
            return GL_INCR;
        case DepthStencilState::STENCIL_OP_DECR:
            return GL_DECR;
        default:
            SIGNAL_ERROR_FMT("Unknown stencil op %d", value);
            return GL_KEEP;
        }
    }

    uint32_t RendererInterfaceOGL::convertComparisonFunc(DepthStencilState::ComparisonFunc value)
    {
        switch (value)
        {
        case DepthStencilState::COMPARISON_NEVER:
            return GL_NEVER;
        case DepthStencilState::COMPARISON_LESS:
            return GL_LESS;
        case DepthStencilState::COMPARISON_EQUAL:
            return GL_EQUAL;
        case DepthStencilState::COMPARISON_LESS_EQUAL:
            return GL_LEQUAL;
        case DepthStencilState::COMPARISON_GREATER:
            return GL_GREATER;
        case DepthStencilState::COMPARISON_NOT_EQUAL:
            return GL_NOTEQUAL;
        case DepthStencilState::COMPARISON_GREATER_EQUAL:
            return GL_GEQUAL;
        case DepthStencilState::COMPARISON_ALWAYS:
            return GL_ALWAYS;
        default:
            SIGNAL_ERROR_FMT("Unknown comparison func %d", value);
            return GL_NEVER;
        }
    }

    uint32_t RendererInterfaceOGL::convertPrimType(PrimitiveType::Enum pt)
    {
        switch (pt)
        {
        case PrimitiveType::POINT_LIST:
            return GL_POINTS;
        case PrimitiveType::TRIANGLE_LIST:
            return GL_TRIANGLES;
        case PrimitiveType::TRIANGLE_STRIP:
            return GL_TRIANGLE_STRIP;
        default:
            SIGNAL_ERROR_FMT("Unsupported primitive type %d", pt);
        }

        return 0;
    }

    uint32_t RendererInterfaceOGL::convertWrapMode(SamplerDesc::WrapMode in_eMode)
    {
        switch (in_eMode)
        {
        case SamplerDesc::WRAP_MODE_CLAMP:
            return GL_CLAMP_TO_EDGE;
        case SamplerDesc::WRAP_MODE_WRAP:
            return GL_REPEAT;
        case SamplerDesc::WRAP_MODE_BORDER:
            return GL_CLAMP_TO_BORDER;
        default:
            SIGNAL_ERROR_FMT("Unknown wrap mode specified %d", in_eMode);
            return GL_CLAMP_TO_EDGE;
            break;
        }
    }

    uint32_t RendererInterfaceOGL::convertBlendValue(BlendState::BlendValue value)
    {
        switch (value)
        {
        case BlendState::BLEND_ZERO:
            return GL_ZERO;
        case BlendState::BLEND_ONE:
            return GL_ONE;
        case BlendState::BLEND_SRC_COLOR:
            return GL_SRC_COLOR;
        case BlendState::BLEND_INV_SRC_COLOR:
            return GL_ONE_MINUS_SRC_COLOR;
        case BlendState::BLEND_SRC_ALPHA:
            return GL_SRC_ALPHA;
        case BlendState::BLEND_INV_SRC_ALPHA:
            return GL_ONE_MINUS_SRC_ALPHA;
        case BlendState::BLEND_DEST_ALPHA:
            return GL_DST_ALPHA;
        case BlendState::BLEND_INV_DEST_ALPHA:
            return GL_ONE_MINUS_DST_ALPHA;
        case BlendState::BLEND_DEST_COLOR:
            return GL_DST_COLOR;
        case BlendState::BLEND_INV_DEST_COLOR:
            return GL_ONE_MINUS_DST_COLOR;
        case BlendState::BLEND_SRC_ALPHA_SAT:
            return GL_SRC_ALPHA_SATURATE;
        case BlendState::BLEND_BLEND_FACTOR:
            return GL_CONSTANT_ALPHA;
        case BlendState::BLEND_INV_BLEND_FACTOR:
            return GL_ONE_MINUS_CONSTANT_ALPHA;
        case BlendState::BLEND_SRC1_COLOR:
            return GL_SRC1_COLOR;
        case BlendState::BLEND_INV_SRC1_COLOR:
            return GL_ONE_MINUS_SRC1_COLOR;
        case BlendState::BLEND_SRC1_ALPHA:
            return GL_SRC1_ALPHA;
        case BlendState::BLEND_INV_SRC1_ALPHA:
            return GL_ONE_MINUS_SRC1_ALPHA;
        default:
            SIGNAL_ERROR_FMT("Unknown blend value %d", value);
            return GL_ZERO;
        }
    }

    uint32_t RendererInterfaceOGL::convertBlendOp(BlendState::BlendOp value)
    {
        switch (value)
        {
        case BlendState::BLEND_OP_ADD:
            return GL_FUNC_ADD;
        case BlendState::BLEND_OP_SUBTRACT:
            return GL_FUNC_SUBTRACT;
        case BlendState::BLEND_OP_REV_SUBTRACT:
            return GL_FUNC_REVERSE_SUBTRACT;
        case BlendState::BLEND_OP_MIN:
            SIGNAL_ERROR("BLEND_OP_MIN is not supported");
            return GL_FUNC_ADD;
        case BlendState::BLEND_OP_MAX:
            SIGNAL_ERROR("BLEND_OP_MAX is not supported");
            return GL_FUNC_ADD;
        default:
            SIGNAL_ERROR_FMT("Unknown blend op %d", value);
            return GL_FUNC_ADD;
        }
    }
}