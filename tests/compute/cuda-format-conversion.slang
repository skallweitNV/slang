// Test CUDA surface format conversion for various image formats.
// This test verifies that the inline codegen approach correctly converts between
// element types and storage formats for CUDA surface reads/writes.

//TEST:SIMPLE(filecheck=CUDA):-target cuda -entry computeMain -stage compute

// Verify unorm reads/writes use uchar/ushort storage types
// CUDA: surf2Dwrite<uchar4>(
// CUDA: surf2Dread<uchar4>(
// CUDA: surf2Dwrite<ushort4>(
// CUDA: surf2Dread<ushort4>(

// Verify snorm reads/writes use uchar storage type
// CUDA: surf2Dwrite<uchar4>(
// CUDA: surf2Dread<uchar4>(

// Verify uint reads/writes use uchar storage type
// CUDA: surf2Dwrite<uchar4>(
// CUDA: surf2Dread<uchar4>(

// Verify half-float reads/writes use ushort storage type
// CUDA: surf2Dwrite<ushort>(
// CUDA: surf2Dread<ushort>(

// Test rgba8 unorm: float4 <-> uchar4
[format("rgba8")]
RWTexture2D<float4> texUnorm;

// Test rgba16 unorm: float4 <-> ushort4
[format("rgba16")]
RWTexture2D<float4> texUnorm16;

// Test rgba8_snorm: float4 <-> uchar4 (signed normalized)
[format("rgba8_snorm")]
RWTexture2D<float4> texSnorm;

// Test rgba8ui: uint4 <-> uchar4
[format("rgba8ui")]
RWTexture2D<uint4> texUint;

// Test r16f: float <-> ushort (half)
[format("r16f")]
RWTexture2D<float> texHalf;

RWStructuredBuffer<float> outputBuffer;

[numthreads(1, 1, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
    uint2 coord = uint2(0, 0);

    // Test unorm read/write
    texUnorm[coord] = float4(0.5, 0.25, 0.75, 1.0);
    float4 v1 = texUnorm[coord];

    // Test unorm16 read/write
    texUnorm16[coord] = float4(0.5, 0.25, 0.75, 1.0);
    float4 v2 = texUnorm16[coord];

    // Test snorm read/write
    texSnorm[coord] = float4(-0.5, 0.25, -0.75, 1.0);
    float4 v3 = texSnorm[coord];

    // Test uint read/write
    texUint[coord] = uint4(10, 20, 30, 40);
    uint4 v4 = texUint[coord];

    // Test half read/write
    texHalf[coord] = 3.14;
    float v5 = texHalf[coord];

    outputBuffer[0] = v1.x + v2.x + v3.x + float(v4.x) + v5;
}
