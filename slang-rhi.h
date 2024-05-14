#pragma once

#include <float.h>
#include <assert.h>

#include "slang.h"
#include "slang-com-ptr.h"

#if defined(SLANG_RHI_DYNAMIC)
#    if defined(_MSC_VER)
#        ifdef SLANG_RHI_DYNAMIC_EXPORT
#            define SLANG_RHI_API SLANG_DLL_EXPORT
#        else
#            define SLANG_RHI_API __declspec(dllimport)
#        endif
#    else
// TODO: need to consider compiler capabilities
//#     ifdef SLANG_DYNAMIC_EXPORT
#        define SLANG_RHI_API SLANG_DLL_EXPORT
//#     endif
#    endif
#endif

#ifndef SLANG_RHI_API
#    define SLANG_RHI_API
#endif

#define SLANG_RHI_ENUM_CLASS_FLAG_OPERATORS(T) \
    inline T operator | (T a, T b) { return T(uint32_t(a) | uint32_t(b)); } \
    inline T operator & (T a, T b) { return T(uint32_t(a) & uint32_t(b)); } \
    inline T operator ~ (T a) { return T(~uint32_t(a)); } \
    inline bool operator !(T a) { return uint32_t(a) == 0; } \
    inline bool operator ==(T a, uint32_t b) { return uint32_t(a) == b; } \
    inline bool operator !=(T a, uint32_t b) { return uint32_t(a) != b; }


#define SLANG_RHI_INLINE_CREATE(interfaceType, descType, funcName) \
    inline ComPtr<interfaceType> funcName(const descType& desc) \
    { \
        ComPtr<interfaceType> obj; \
        SLANG_RETURN_NULL_ON_FAIL(funcName(desc, obj.writeRef())); \
        return obj; \
    }


namespace slang::rhi {

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

static constexpr uint32_t kMaxRenderTargetCount = 8;

// ----------------------------------------------------------------------------
// Struct types
// ----------------------------------------------------------------------------

// Every struct passed to/from the RHI should have a structType as the first
// field. This allows the RHI to check that a correct struct is passed and
// allows to support multiple struct versions in the same API call.

enum class StructType : uint32_t
{
    BufferDesc_1,
    TextureDesc_1,
    SamplerDesc_1,
    InputLayoutDesc_1,
    FramebufferDesc_1,
    ShaderProgramDesc_1,
    ComputePipelineDesc_1,
    GraphicsPipelineDesc_1,
    RaytracingPipelineDesc_1,
    FenceDesc_1,
    CommandListDesc_1,
    DeviceDesc_1,
    DeviceDescD3D12_1,
    DeviceDescVulkan_1,
};

// ----------------------------------------------------------------------------
// Basic types
// ----------------------------------------------------------------------------

template<typename T>
using ComPtr = Slang::ComPtr<T>;

using Result = SlangResult;

using Int = SlangInt;
using UInt = SlangUInt;

using Index = Int;
using Count = Int;
using Size = uint64_t;
using Offset = uint64_t;

using DeviceAddress = uint64_t;

struct Color
{
    float r, g, b, a;

    Color() : r(0.f), g(0.f), b(0.f), a(0.f) {}
    explicit Color(float c) : r(c), g(c), b(c), a(c) {}
    Color(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_) {}

    constexpr bool operator==(const Color& rhs) const { return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a; }
    constexpr bool operator!=(const Color& rhs) const { return !(*this == rhs); }
};

struct Viewport
{
    float minX, maxX;
    float minY, maxY;
    float minZ, maxZ;

    Viewport() : minX(0.f), maxX(0.f), minY(0.f), maxY(0.f), minZ(0.f), maxZ(1.f) {}
    Viewport(float width, float height) : minX(0.f), maxX(width), minY(0.f), maxY(height), minZ(0.f), maxZ(1.f) {}
    Viewport(float minX_, float maxX_, float minY_, float maxY_, float minZ_, float maxZ_)
        : minX(minX_), maxX(maxX_), minY(minY_), maxY(maxY_), minZ(minZ_), maxZ(maxZ_)
    {}

    [[nodiscard]] float width() const { return maxX - minX; }
    [[nodiscard]] float height() const { return maxY - minY; }
    [[nodiscard]] float depth() const { return maxZ - minZ; }

    constexpr bool operator==(const Viewport& rhs) const
    {
        return minX == rhs.minX && maxX == rhs.maxX && minY == rhs.minY && maxY == rhs.maxY && minZ == rhs.minZ && maxZ == rhs.maxZ;
    }
    constexpr bool operator!=(const Viewport& rhs) const { return !(*this == rhs); }
};

struct Rect
{
    float minX, maxX;
    float minY, maxY;

    Rect() : minX(0.f), maxX(0.f), minY(0.f), maxY(0.f) {}
    Rect(float width, float height) : minX(0.f), maxX(width), minY(0.f), maxY(height) {}
    Rect(float minX_, float maxX_, float minY_, float maxY_) : minX(minX_), maxX(maxX_), minY(minY_), maxY(maxY_) {}

    [[nodiscard]] float width() const { return maxX - minX; }
    [[nodiscard]] float height() const { return maxY - minY; }

    constexpr bool operator==(const Rect& rhs) const { return minX == rhs.minX && maxX == rhs.maxX && minY == rhs.minY && maxY == rhs.maxY; }
    constexpr bool operator!=(const Rect& rhs) const { return !(*this == rhs); }
};

struct ShaderOffset
{
    Offset uniformOffset;
    Index bindingRangeIndex;
    Index bindingArrayIndex;

    ShaderOffset() : uniformOffset(0), bindingRangeIndex(0), bindingArrayIndex(0) {}
    ShaderOffset(Offset uniformOffset_, Index bindingRangeIndex_, Index bindingArrayIndex_)
        : uniformOffset(uniformOffset_), bindingRangeIndex(bindingRangeIndex_), bindingArrayIndex(bindingArrayIndex_)
    {}

    uint32_t getHashCode() const
    {
        return (uint32_t)(((bindingRangeIndex << 20) + bindingArrayIndex) ^ uniformOffset);
    }

    constexpr bool operator==(const ShaderOffset& rhs) const { return uniformOffset == rhs.uniformOffset && bindingRangeIndex == rhs.bindingRangeIndex && bindingArrayIndex == rhs.bindingArrayIndex; }
    constexpr bool operator!=(const ShaderOffset& rhs) const { return !(*this == rhs); }
    constexpr bool operator<(const ShaderOffset& rhs) const
    {
        if (bindingRangeIndex < rhs.bindingRangeIndex)
            return true;
        if (bindingRangeIndex > rhs.bindingRangeIndex)
            return false;
        if (bindingArrayIndex < rhs.bindingArrayIndex)
            return true;
        if (bindingArrayIndex > rhs.bindingArrayIndex)
            return false;
        return uniformOffset < rhs.uniformOffset;
    }
    constexpr bool operator<=(const ShaderOffset& rhs) const { return (*this == rhs) || (*this < rhs); }
    constexpr bool operator>(const ShaderOffset& rhs) const { return (rhs < *this); }
    constexpr bool operator>=(const ShaderOffset& rhs) const { return (rhs <= *this); }
};

enum class GraphicsAPI
{
    Auto,
    D3D12,
    Vulkan,
    CUDA,
    CPU,
};

enum class NativeHandleType
{
    Win32                                   = 0x000,    // General Win32 HANDLE.
    FileDescriptor                          = 0x001,    // General file descriptor.

    D3D12_Device                            = 0x100,
    D3D12_CommandQueue                      = 0x101,
    D3D12_CommandList                       = 0x102,
    D3D12_Resource                          = 0x103,
    D3D12_RenderTargetViewDescriptor        = 0x104,
    D3D12_DepthStencilViewDescriptor        = 0x105,
    D3D12_ShaderResourceViewGpuDescripror   = 0x106,
    D3D12_UnorderedAccessViewGpuDescripror  = 0x107,
    D3D12_RootSignature                     = 0x108,
    D3D12_PipelineState                     = 0x109,
    D3D12_CommandAllocator                  = 0x10a,

    VK_Device                               = 0x200,
    VK_PhysicalDevice                       = 0x201,
    VK_Instance                             = 0x202,
    VK_Queue                                = 0x203,
    VK_CommandBuffer                        = 0x204,
    VK_DeviceMemory                         = 0x205,
    VK_Buffer                               = 0x206,
    VK_Image                                = 0x207,
    VK_ImageView                            = 0x208,
    VK_AccelerationStructureKHR             = 0x209,
    VK_Sampler                              = 0x20a,
    VK_ShaderModule                         = 0x20b,
    VK_RenderPass                           = 0x20c,
    VK_Framebuffer                          = 0x20d,
    VK_DescriptorPool                       = 0x20e,
    VK_DescriptorSetLayout                  = 0x21f,
    VK_DescriptorSet                        = 0x210,
    VK_PipelineLayout                       = 0x211,
    VK_Pipeline                             = 0x212,
    VK_Micromap                             = 0x213,

    CUDA_Device                             = 0x300,

};

struct NativeHandle
{
    uint64_t value = 0;

    NativeHandle() = default;
    explicit NativeHandle(uint64_t value) : value(value) {}
    explicit NativeHandle(void* value) : value(reinterpret_cast<uintptr_t>(value)) {}

    template<typename T>
    T* as() const { return reinterpret_cast<T*>(value); }
};

// ----------------------------------------------------------------------------
// Resource formats
// ----------------------------------------------------------------------------

enum class Format : uint32_t
{
    Unknown,

    R8_UINT,
    R8_SINT,
    R8_UNORM,
    R8_SNORM,
    RG8_UINT,
    RG8_SINT,
    RG8_UNORM,
    RG8_SNORM,
    R16_UINT,
    R16_SINT,
    R16_UNORM,
    R16_SNORM,
    R16_FLOAT,
    BGRA4_UNORM,
    B5G6R5_UNORM,
    B5G5R5A1_UNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    RGBA8_UNORM,
    RGBA8_SNORM,
    BGRA8_UNORM,
    SRGBA8_UNORM,
    SBGRA8_UNORM,
    R10G10B10A2_UNORM,
    R11G11B10_FLOAT,
    RG16_UINT,
    RG16_SINT,
    RG16_UNORM,
    RG16_SNORM,
    RG16_FLOAT,
    R32_UINT,
    R32_SINT,
    R32_FLOAT,
    RGBA16_UINT,
    RGBA16_SINT,
    RGBA16_FLOAT,
    RGBA16_UNORM,
    RGBA16_SNORM,
    RG32_UINT,
    RG32_SINT,
    RG32_FLOAT,
    RGB32_UINT,
    RGB32_SINT,
    RGB32_FLOAT,
    RGBA32_UINT,
    RGBA32_SINT,
    RGBA32_FLOAT,
    
    D16,
    D24S8,
    X24G8_UINT,
    D32,
    D32S8,
    X32G8_UINT,

    BC1_UNORM,
    BC1_UNORM_SRGB,
    BC2_UNORM,
    BC2_UNORM_SRGB,
    BC3_UNORM,
    BC3_UNORM_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UFLOAT,
    BC6H_SFLOAT,
    BC7_UNORM,
    BC7_UNORM_SRGB,

    CountOf,
};
    
enum class FormatKind : uint8_t
{
    Integer,
    Normalized,
    Float,
    DepthStencil
};

struct FormatInfo
{
    Format format;
    const char* name;
    FormatKind kind;
    uint8_t bytesPerBlock;
    uint8_t blockSize;
    bool hasRed : 1;
    bool hasGreen : 1;
    bool hasBlue : 1;
    bool hasAlpha : 1;
    bool hasDepth : 1;
    bool hasStencil : 1;
    bool isSigned : 1;
    bool isSRGB : 1;
};

enum class FormatSupport : uint32_t
{
    None            = 0,

    Buffer          = (1<<0),
    IndexBuffer     = (1<<1),
    VertexBuffer    = (1<<2),

    Texture         = (1<<3),
    DepthStencil    = (1<<4),
    RenderTarget    = (1<<5),
    Blendable       = (1<<6),

    ShaderLoad      = (1<<7),
    ShaderSample    = (1<<8),
    ShaderUavLoad   = (1<<9),
    ShaderUavStore  = (1<<10),
    ShaderAtomic    = (1<<11),
};

SLANG_RHI_ENUM_CLASS_FLAG_OPERATORS(FormatSupport)

// ----------------------------------------------------------------------------
// Resources
// ----------------------------------------------------------------------------

class IResource : public ISlangUnknown
{
public:
    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(NativeHandleType type, NativeHandle* outHandle) const = 0;
};

enum class MemoryType : uint32_t
{
    DeviceLocal,
    Upload,
    ReadBack,
};

enum class ResourceState : uint32_t
{
    Undefined                           = 0,
    Common                              = (1<<0),
    ConstantBuffer                      = (1<<1),
    VertexBuffer                        = (1<<2),
    IndexBuffer                         = (1<<3),
    IndirectArgument                    = (1<<4),
    ShaderResource                      = (1<<5),
    UnorderedAccess                     = (1<<6),
    RenderTarget                        = (1<<7),
    DepthWrite                          = (1<<8),
    DepthRead                           = (1<<9),
    StreamOutput                        = (1<<10),
    CopySource                          = (1<<11),
    CopyDestination                     = (1<<12),
    ResolveSource                       = (1<<13),
    ResolveDestination                  = (1<<14),
    Present                             = (1<<15),
    AccelerationStructure               = (1<<16),
    AccelerationStructureBuildInput     = (1<<17),
};

// ----------------------------------------------------------------------------
// Buffer
// ----------------------------------------------------------------------------

struct BufferRange
{
    Offset byteOffset = 0;
    Size byteSize = 0;
    
    BufferRange() = default;
    BufferRange(Offset byteOffset_, Size byteSize_) : byteOffset(byteOffset_), byteSize(byteSize_) {}

    // [[nodiscard]] NVRHI_API BufferRange resolve(const BufferDesc& desc) const;
    // [[nodiscard]] constexpr bool isEntireBuffer(const BufferDesc& desc) const { return (byteOffset == 0) && (byteSize == ~0ull || byteSize == desc.byteSize); }

    constexpr bool operator==(const BufferRange& rhs) const { return byteOffset == rhs.byteOffset && byteSize == rhs.byteSize; }
    constexpr bool operator!=(const BufferRange& rhs) const { return !(*this == rhs); }
};

static const BufferRange kEntireBuffer{0ull, ~0ull};

struct BufferDesc
{
    StructType structType = StructType::BufferDesc_1;
    void *next = nullptr;

    /// Size of the buffer in bytes.
    Size byteSize = 0;
    /// Struct stride for structured buffers.
    Size structStride = 0;
    /// Format used for typed buffer views.
    Format format = Format::Unknown;

    /// Debug name for validation layers.
    const char* debugName = nullptr;

    MemoryType memoryType;
    ResourceState defaultState;
    ResourceState allowedStates;
    bool isShared;

};

class IBuffer : public IResource
{
public:
    virtual SLANG_NO_THROW BufferDesc& SLANG_MCALL getDesc() = 0;
    virtual SLANG_NO_THROW DeviceAddress SLANG_MCALL getDeviceAddress() = 0;
};

// ----------------------------------------------------------------------------
// Texture
// ----------------------------------------------------------------------------

using MipLevel = uint32_t;
using ArraySlice = uint32_t;

// describes a 2D section of a single mip level + single slice of a texture
struct TextureSlice
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
    // -1 means the entire dimension is part of the region
    // resolve() below will translate these values into actual dimensions
    uint32_t width = uint32_t(-1);
    uint32_t height = uint32_t(-1);
    uint32_t depth = uint32_t(-1);

    MipLevel mipLevel = 0;
    ArraySlice arraySlice = 0;
};

struct TextureSubresourceSet {};

struct TextureDesc
{
    StructType structType = StructType::TextureDesc_1;
    void *next = nullptr;
    
};

class ITexture : public IResource
{
public:
    virtual SLANG_NO_THROW TextureDesc& SLANG_MCALL getDesc() = 0;
    virtual SLANG_NO_THROW DeviceAddress SLANG_MCALL getDeviceAddress() = 0;
};

// ----------------------------------------------------------------------------
// Sampler
// ----------------------------------------------------------------------------

enum class ComparisonFunc : uint8_t
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

enum class TextureFilteringMode : uint8_t
{
    Point,
    Linear,
};

enum class TextureAddressingMode : uint8_t
{
    Wrap,
    ClampToEdge,
    ClampToBorder,
    MirrorRepeat,
    MirrorOnce,
};

enum class TextureReductionOp : uint8_t
{
    Average,
    Comparison,
    Minimum,
    Maximum,
};

struct SamplerDesc
{
    StructType structType = StructType::SamplerDesc_1;
    void *next = nullptr;

    TextureFilteringMode minFilter = TextureFilteringMode::Linear;
    TextureFilteringMode magFilter = TextureFilteringMode::Linear;
    TextureFilteringMode mipFilter = TextureFilteringMode::Linear;
    TextureReductionOp reductionOp = TextureReductionOp::Average;
    TextureAddressingMode addressU = TextureAddressingMode::Wrap;
    TextureAddressingMode addressV = TextureAddressingMode::Wrap;
    TextureAddressingMode addressW = TextureAddressingMode::Wrap;
    float mipBias = 0.f;
    uint32_t maxAnisotropy = 1;
    ComparisonFunc comparisonFunc = ComparisonFunc::Never;
    Color borderColor = Color(1.f);
    float mipMin = -FLT_MAX;
    float mipMax = FLT_MAX;
};

class ISampler : public IResource
{
public:
    virtual SLANG_NO_THROW const SamplerDesc& SLANG_MCALL getDesc() const = 0;
};

// ----------------------------------------------------------------------------
// InputLayout
// ----------------------------------------------------------------------------

struct InputAttributeDesc
{
    /// Name of the attribute.
    const char* name = nullptr;
    /// Format of the element.
    Format format = Format::Unknown;
    Count arraySize = 1;
    Index bufferIndex = 0;
    Offset byteOffset = 0;
    /// Element stride in bytes.
    /// Note: For most graphics APIs, all strides for a given bufferIndex must be identical.
    Size byteStride = 0;
    /// If true, this is a per-instance attribute.
    bool isInstanced = false;
};

struct InputLayoutDesc
{
    StructType structType = StructType::InputLayoutDesc_1;
    void *next = nullptr;

    const InputAttributeDesc* attributes = nullptr;
    Count attributeCount = 0;
};

class IInputLayout : public IResource
{
};

// ----------------------------------------------------------------------------
// Framebuffer
// ----------------------------------------------------------------------------

struct FramebufferAttachmentDesc
{
    ITexture* texture = nullptr;
    // TextureSubresourceSet subresources = TextureSubresourceSet(0, 1, 0, 1);
    Format format = Format::Unknown;
    bool isReadOnly = false;
    // [[nodiscard]] bool valid() const { return texture != nullptr; }
};

struct FramebufferDesc
{
    StructType structType = StructType::FramebufferDesc_1;
    void *next = nullptr;

    FramebufferAttachmentDesc colorAttachments[kMaxRenderTargetCount];
    FramebufferAttachmentDesc depthStencilAttachment;
};

struct FramebufferInfo
{
    Format colorFormats[kMaxRenderTargetCount] = {};
    Format depthStencilFormat = Format::Unknown;
    uint32_t sampleCount = 1;
    uint32_t sampleQuality = 0;

    constexpr bool operator==(const FramebufferInfo& rhs) const
    {
        if (sampleCount != rhs.sampleCount || sampleQuality != rhs.sampleQuality)
            return false;
        for (int i = 0; i < kMaxRenderTargetCount; i++)
            if (colorFormats[i] != rhs.colorFormats[i])
                return false;
        return depthStencilFormat == rhs.depthStencilFormat;
    }
    constexpr bool operator!=(const FramebufferInfo& rhs) const { return !(*this == rhs); }
};

class IFramebuffer : public IResource
{
public:
    virtual SLANG_NO_THROW FramebufferDesc& SLANG_MCALL getDesc() = 0;
    virtual SLANG_NO_THROW FramebufferInfo& SLANG_MCALL getInfo() = 0;
};

// ----------------------------------------------------------------------------
// ShaderProgram
// ----------------------------------------------------------------------------

struct ShaderProgramDesc
{
    StructType structType = StructType::ShaderProgramDesc_1;
    void* next = nullptr;
};

class IShaderProgram : public IResource
{
public:
};

// ----------------------------------------------------------------------------
// ShaderObject
// ----------------------------------------------------------------------------

class IShaderObject : public IResource
{
public:
    virtual SLANG_NO_THROW void SLANG_MCALL setBuffer(ShaderOffset offset, IBuffer* buffer, const BufferRange& range = kEntireBuffer) = 0;
    // virtual SLANG_NO_THROW void SLANG_MCALL setTexture(ShaderOffset offset, ITexture* texture, const TextureSubresourceSet& subresourceSet = kEntireSubresourceSet) = 0;
    virtual SLANG_NO_THROW void SLANG_MCALL setSampler(ShaderOffset offset, ISampler* sampler) = 0;
};

class IImmutableShaderObject : public IShaderObject
{
public:
    virtual SLANG_NO_THROW Result SLANG_MCALL finalize() = 0;
};

class IMutableShaderObject : public IShaderObject
{
public:
};

// ----------------------------------------------------------------------------
// ComputePipeline
// ----------------------------------------------------------------------------

struct ComputePipelineDesc
{
    StructType structType = StructType::ComputePipelineDesc_1;
    void* next = nullptr;

    IShaderProgram* program = nullptr;
};

class IComputePipeline : public IResource
{
public:
};

// ----------------------------------------------------------------------------
// GraphicsPipeline
// ----------------------------------------------------------------------------

enum class PrimitiveType
{
    Point,
    Line,
    Triangle,
    Patch,
};

struct DepthStencilDesc
{
    bool            depthTestEnable     = false;
    bool            depthWriteEnable    = true;
    ComparisonFunc  depthFunc           = ComparisonFunc::Less;

    bool                stencilEnable       = false;
    uint32_t            stencilReadMask     = 0xFFFFFFFF;
    uint32_t            stencilWriteMask    = 0xFFFFFFFF;
    // DepthStencilOpDesc  frontFace;
    // DepthStencilOpDesc  backFace;

    uint32_t stencilRef = 0;
};

struct RasterizerDesc
{
    // FillMode fillMode = FillMode::Solid;
    // CullMode cullMode = CullMode::None;
    // FrontFaceMode frontFace = FrontFaceMode::CounterClockwise;
    int32_t depthBias = 0;
    float depthBiasClamp = 0.0f;
    float slopeScaledDepthBias = 0.0f;
    bool depthClipEnable = true;
    bool scissorEnable = false;
    bool multisampleEnable = false;
    bool antialiasedLineEnable = false;
    bool enableConservativeRasterization = false;
    uint32_t forcedSampleCount = 0;
};


struct GraphicsPipelineDesc
{
    StructType structType = StructType::GraphicsPipelineDesc_1;
    void* next = nullptr;

    IShaderProgram* program = nullptr;
    IInputLayout* inputLayout = nullptr;
    IFramebuffer* framebuffer = nullptr;
    // PrimitiveType primitiveType = PrimitiveType::TriangleList;
    DepthStencilDesc    depthStencil;
    RasterizerDesc      rasterizer;
    // BlendDesc           blend;
};

class IGraphicsPipeline : public IResource
{
public:
};

// ----------------------------------------------------------------------------
// RaytracingPipeline
// ----------------------------------------------------------------------------

struct RaytracingPipelineDesc
{
    StructType structType = StructType::RaytracingPipelineDesc_1;
    void* next = nullptr;
};

class IRaytracingPipeline : public IResource
{
public:
};

// ----------------------------------------------------------------------------
// Fence
// ----------------------------------------------------------------------------

struct FenceDesc
{
    StructType structType = StructType::FenceDesc_1;
    void* next = nullptr;

    uint64_t initialValue = 0;
    bool isShared = false;
};

class IFence : public IResource
{
public:
    /// Returns the currently signaled value on the device.
    virtual SLANG_NO_THROW Result SLANG_MCALL getCurrentValue(uint64_t* outValue) = 0;
};

// ----------------------------------------------------------------------------
// CommandList
// ----------------------------------------------------------------------------

struct CommandListDesc
{
    StructType structType = StructType::CommandListDesc_1;
    void* next = nullptr;
};

class ICommandList : public IResource
{
public:
    virtual Result SLANG_MCALL open() = 0;
    virtual Result SLANG_MCALL close() = 0;

    virtual void SLANG_MCALL clearTextureFloat(ITexture* texture, const TextureSubresourceSet& subresources, const Color& color) = 0;
    virtual void SLANG_MCALL clearTextureUInt(ITexture* texture, const TextureSubresourceSet& subresources, uint32_t color) = 0;
    virtual void SLANG_MCALL clearDepthStencilTexture(ITexture* texture, const TextureSubresourceSet& subresources, bool clearDepth, float depth, bool clearStencil, uint8_t stencil) = 0;

    virtual void SLANG_MCALL bindComputePipeline(IComputePipeline* pipeline, IShaderObject* rootObject);
    virtual void SLANG_MCALL bindComputePipelineWithRootObject(IComputePipeline* pipeline, IShaderObject* rootObject);
    virtual void SLANG_MCALL dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;
    virtual void SLANG_MCALL dispatchIndirect(IBuffer* cmdBuffer, Offset offsetInBytes);

    virtual void SLANG_MCALL beginMarker(const char* name) = 0;
    virtual void SLANG_MCALL endMarker() = 0;
};

// ----------------------------------------------------------------------------
// Device
// ----------------------------------------------------------------------------

enum class DeviceType : uint8_t
{
    Automatic,
    D3D12,
    Vulkan,
    CUDA,
    CPU,
};

struct DeviceDesc
{
    StructType structType = StructType::DeviceDesc_1;
    void* next = nullptr;

    DeviceType type = DeviceType::Automatic;

    /// LUID of the adapter to use. Use rhiGetAdapterCount()/rhiGetAdapterInfo() to get a list of available adapters.
    // const AdapterLUID* adapterLUID = nullptr;

    /// Enable RHI's validation layer.
    bool enableValidationLayer = false;

    /// Enable the underlying API's validation layer (if available).
    bool enableAPIValidationLayer = false;

    // IMessageCallback* messageCallback;
};

struct DeviceLimits
{
    /// Maximum dimension for 1D textures.
    uint32_t maxTextureDimension1D;
    /// Maximum dimensions for 2D textures.
    uint32_t maxTextureDimension2D;
    /// Maximum dimensions for 3D textures.
    uint32_t maxTextureDimension3D;
    /// Maximum dimensions for cube textures.
    uint32_t maxTextureDimensionCube;
    /// Maximum number of texture layers.
    uint32_t maxTextureArrayLayers;

    /// Maximum number of vertex input elements in a graphics pipeline.
    uint32_t maxVertexInputElements;
    /// Maximum offset of a vertex input element in the vertex stream.
    uint32_t maxVertexInputElementOffset;
    /// Maximum number of vertex streams in a graphics pipeline.
    uint32_t maxVertexStreams;
    /// Maximum stride of a vertex stream.
    uint32_t maxVertexStreamStride;

    /// Maximum number of threads per thread group.
    uint32_t maxComputeThreadsPerGroup;
    /// Maximum dimensions of a thread group.
    uint32_t maxComputeThreadGroupSize[3];
    /// Maximum number of thread groups per dimension in a single dispatch.
    uint32_t maxComputeDispatchThreadGroups[3];

    /// Maximum number of viewports per pipeline.
    uint32_t maxViewports;
    /// Maximum viewport dimensions.
    uint32_t maxViewportDimensions[2];
    /// Maximum framebuffer dimensions.
    uint32_t maxFramebufferDimensions[3];

    /// Maximum samplers visible in a shader stage.
    uint32_t maxShaderVisibleSamplers;
};

struct DeviceInfo
{
    DeviceType type;

    DeviceLimits limits;

    const char** features;

    /// The name of the graphics adapter.
    const char* adapterName = nullptr;

    /// The clock frequency used in timestamp queries.
    uint64_t timestampFrequency = 0;
};

class IDevice : public IResource
{
public:
    virtual SLANG_NO_THROW DeviceInfo& SLANG_MCALL getInfo() = 0;

    virtual SLANG_NO_THROW bool SLANG_MCALL hasFeature(const char* feature) = 0;

    /// Creates a new buffer resource.
    virtual SLANG_NO_THROW Result SLANG_MCALL createBuffer(const BufferDesc& desc, IBuffer** outBuffer) = 0;

    /// Creates a new texture resource.
    virtual SLANG_NO_THROW Result SLANG_MCALL createTexture(const TextureDesc& desc, ITexture** outTexture) = 0;

    /// Creates a new sampler.
    virtual SLANG_NO_THROW Result SLANG_MCALL createSampler(const SamplerDesc& desc, ISampler** outSampler) = 0;

    virtual SLANG_NO_THROW Result SLANG_MCALL createFence(const FenceDesc& desc, IFence** outFence) = 0;

    virtual SLANG_NO_THROW Result SLANG_MCALL createInputLayout(const InputLayoutDesc& desc, IInputLayout** outInputLayout) = 0;

    virtual SLANG_NO_THROW Result SLANG_MCALL createFramebuffer(const FramebufferDesc& desc, IFramebuffer** outFramebuffer) = 0;

    virtual SLANG_NO_THROW Result SLANG_MCALL createShaderProgram(const ShaderProgramDesc& desc, IShaderProgram** outShaderProgram) = 0;

    virtual SLANG_NO_THROW Result SLANG_MCALL createComputePipeline(const ComputePipelineDesc& desc, IComputePipeline** outPipeline) = 0;

    virtual SLANG_NO_THROW Result SLANG_MCALL createGraphicsPipeline(const GraphicsPipelineDesc& desc, IGraphicsPipeline** outPipeline) = 0;

    virtual SLANG_NO_THROW Result SLANG_MCALL createCommandList(const CommandListDesc& desc, ICommandList** outCommandList) = 0;


    // TODO add access mode
    virtual SLANG_NO_THROW Result SLANG_MCALL mapBuffer(IBuffer* buffer, void** outPointer) = 0;
    inline void* mapBuffer(IBuffer* buffer)
    {
        void* ptr = nullptr;
        SLANG_RETURN_NULL_ON_FAIL(mapBuffer(buffer, &ptr));
        return ptr;
    }

    virtual SLANG_NO_THROW Result SLANG_MCALL unmapBuffer(IBuffer* buffer) = 0;


    // virtual uint64_t executeCommandLists(ICommandList* const* pCommandLists, size_t numCommandLists, CommandQueue executionQueue = CommandQueue::Graphics) = 0;
    // virtual void queueWaitForCommandList(CommandQueue waitQueue, CommandQueue executionQueue, uint64_t instance) = 0;

    // Wait until all work on the device has completed.
    virtual SLANG_NO_THROW Result SLANG_MCALL waitForIdle() = 0;

    // Releases the resources that were referenced in the command lists that have finished executing.
    // IMPORTANT: Call this method at least once per frame.
    virtual SLANG_NO_THROW void SLANG_MCALL runGarbageCollection() = 0;


    // Convenience methods
    SLANG_RHI_INLINE_CREATE(IBuffer, BufferDesc, createBuffer)
    SLANG_RHI_INLINE_CREATE(ITexture, TextureDesc, createTexture)
    SLANG_RHI_INLINE_CREATE(ISampler, SamplerDesc, createSampler)
    SLANG_RHI_INLINE_CREATE(IFence, FenceDesc, createFence)
    SLANG_RHI_INLINE_CREATE(IInputLayout, InputLayoutDesc, createInputLayout)
    SLANG_RHI_INLINE_CREATE(IFramebuffer, FramebufferDesc, createFramebuffer)
    SLANG_RHI_INLINE_CREATE(IShaderProgram, ShaderProgramDesc, createShaderProgram)
    SLANG_RHI_INLINE_CREATE(IComputePipeline, ComputePipelineDesc, createComputePipeline)
    SLANG_RHI_INLINE_CREATE(IGraphicsPipeline, GraphicsPipelineDesc, createGraphicsPipeline)
    SLANG_RHI_INLINE_CREATE(ICommandList, CommandListDesc, createCommandList)

    inline ComPtr<IInputLayout> createInputLayout(const InputAttributeDesc* attributes, Count attributeCount)
    {
        InputLayoutDesc desc;
        desc.attributes = attributes;
        desc.attributeCount = attributeCount;
        return createInputLayout(desc);
    }
};

// ----------------------------------------------------------------------------
// Adapter
// ----------------------------------------------------------------------------

struct AdapterLUID
{
    uint8_t luid[16] = {0};

    constexpr bool operator==(const AdapterLUID& rhs) const
    {
        for (size_t i = 0; i < sizeof(luid); ++i)
            if (luid[i] != rhs.luid[i])
                return false;
        return true;
    }
    constexpr bool operator!=(const AdapterLUID& rhs) const { return !(*this == rhs); }
};

struct AdapterInfo
{
    /// Descriptive name of the adapter.
    char name[128];

    /// Unique identifier for the vendor (only available for D3D12 and Vulkan).
    uint32_t vendorID;

    /// Unique identifier for the physical device among devices from the vendor (only available for D3D12 and Vulkan)
    uint32_t deviceID;

    /// Logically unique identifier of the adapter.
    AdapterLUID luid;
};


class IAdapter : public ISlangUnknown
{
public:
    SLANG_COM_INTERFACE(0x633284BB, 0x5BDB, 0x478F, { 0x8B, 0x10, 0xC4, 0x28, 0x38, 0x99, 0xF1, 0x56 })

    virtual SLANG_NO_THROW AdapterInfo& SLANG_MCALL getInfo() = 0;
};

// ----------------------------------------------------------------------------
// Message callback
// ----------------------------------------------------------------------------

enum class MessageSeverity : uint8_t
{
    Info,
    Warning,
    Error,
    Fatal,
};

class IMessageCallback
{
public:
    virtual void SLANG_MCALL message(MessageSeverity severity, const char* msg) = 0;
};

// ----------------------------------------------------------------------------
// Factory
// ----------------------------------------------------------------------------

class IFactory : public ISlangUnknown
{
public:
    SLANG_COM_INTERFACE(0x633284BB, 0x5BDB, 0x478F, { 0x8B, 0x10, 0xC4, 0x28, 0x38, 0x99, 0xF1, 0x57 })

    virtual Result SLANG_MCALL enumAdapter(Index index, IAdapter** outAdapter) = 0;

    virtual Result SLANG_MCALL createDevice(const DeviceDesc* desc, IAdapter* adapter, IDevice** outDevice) = 0;
};

} // namespace slang::rhi

// Global public functions

extern "C" {

SLANG_RHI_API slang::rhi::Result SLANG_STDCALL slangRhiCreateFactory(slang::rhi::GraphicsAPI api, slang::rhi::IFactory** outFactory);


/// Return the number of adapters for a given device type.
SLANG_RHI_API slang::rhi::Result SLANG_MCALL rhiGetAdapterCount(slang::rhi::DeviceType deviceType, slang::rhi::Count* outCount);

/// Get information about an adapter.
SLANG_RHI_API slang::rhi::Result SLANG_MCALL rhiGetAdapterInfo(slang::rhi::DeviceType deviceType, slang::rhi::Index index, slang::rhi::AdapterInfo* outInfo);

/// Get information about a format.
SLANG_RHI_API slang::rhi::Result SLANG_MCALL rhiGetFormatInfo(slang::rhi::Format format, slang::rhi::FormatInfo* outInfo);

/// Create a new device.
SLANG_RHI_API slang::rhi::Result SLANG_MCALL rhiCreateDevice(const slang::rhi::DeviceDesc& desc, slang::rhi::IDevice** outDevice);

/// Reports current set of live objects in gfx.
/// Currently this only calls D3D's ReportLiveObjects.
SLANG_RHI_API slang::rhi::Result SLANG_MCALL rhiReportLiveObjects();

SLANG_RHI_API slang::rhi::Result SLANG_MCALL rhiSetMessageCallback(slang::rhi::IMessageCallback* callback);

} // extern "C"

namespace slang::rhi {

inline ComPtr<IFactory> createFactory(GraphicsAPI api)
{
    ComPtr<IFactory> factory;
    SLANG_RETURN_NULL_ON_FAIL(slangRhiCreateFactory(api, factory.writeRef()));
    return factory;
}

inline ComPtr<IDevice> createDevice(const DeviceDesc& desc, IAdapter* adapter = nullptr)
{
    ComPtr<IDevice> device;
    // SLANG_RETURN_NULL_ON_FAIL(adapter->createDevice(&desc, adapter, device.writeRef()));
    return device;
}

} // namespace slang::rhi

// #include <d3d12.h>
#if defined(__d3d12_h__) && defined(__ID3D12Device_FWD_DEFINED__) && defined(__ID3D12CommandQueue_FWD_DEFINED__)
namespace slang::rhi {
struct DeviceDescD3D12
{
    StructType structType = StructType::DeviceDescD3D12_1;
    void* next = nullptr;

    // Existing D3D12 objects to use.
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* graphicsCommandQueue = nullptr;
    ID3D12CommandQueue* computeCommandQueue = nullptr;
    ID3D12CommandQueue* copyCommandQueue = nullptr;

    // Heap sizes.
    uint32_t renderTargetViewHeapSize = 1024;
    uint32_t depthStencilViewHeapSize = 1024;
    uint32_t shaderResourceViewHeapSize = 16384;
    uint32_t samplerHeapSize = 1024;
    uint32_t maxTimerQueries = 256;    
};

class IDeviceD3D12
{
public:
    DXGI_FORMAT getDxgiFormat(Format format);
};

}
#endif

// #include <vulkan.h>
#ifdef VULKAN_CORE_H_
namespace slang::rhi {
struct DeviceDescVulkan
{
    StructType structType = StructType::DeviceDescVulkan_1;
    void* next = nullptr;

    // Existing Vulkan objects to use.
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    int graphicsQueueIndex = -1;
    VkQueue transferQueue = VK_NULL_HANDLE;
    int transferQueueIndex = -1;
    VkQueue computeQueue = VK_NULL_HANDLE;
    int computeQueueIndex = -1;

    VkAllocationCallbacks *allocationCallbacks = nullptr;

    const char **instanceExtensions = nullptr;
    size_t instanceExtensionCount = 0;

    const char **deviceExtensions = nullptr;
    size_t deviceExtensionCount = 0;

    uint32_t maxTimerQueries = 256;
};

class IDeviceVulkan
{
public:
    VkFormat getVkFormat(Format format);
};

}
#endif
