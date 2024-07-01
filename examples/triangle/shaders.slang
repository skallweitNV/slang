// shaders.slang

//
// This file provides a simple vertex and fragment shader that can be compiled
// using Slang. This code should also be valid as HLSL, and thus it does not
// use any of the new language features supported by Slang.
//

StructuredBuffer<float> buf1;
StructuredBuffer<float> buf2;
StructuredBuffer<float> buf3;

// Uniform data to be passed from application -> shader.
cbuffer Uniforms
{
    float4x4 modelViewProjection;
}

// Per-vertex attributes to be assembled from bound vertex buffers.
struct AssembledVertex
{
    float3	position : POSITION;
    float3	color    : COLOR;
};

// Output of the vertex shader, and input to the fragment shader.
struct CoarseVertex
{
    float3 color;
};

// Output of the fragment shader
struct Fragment
{
    float4 color;
};

// Vertex  Shader

struct VertexStageOutput
{
    CoarseVertex    coarseVertex    : COARSEVERTEX;
    float4          sv_position     : SV_Position;
};

[shader("vertex")]
VertexStageOutput vertexMain(
    AssembledVertex assembledVertex)
{
    VertexStageOutput output;

    float3 position = assembledVertex.position;
    float3 color = assembledVertex.color;
    position.x += buf1[0];
    // color.x += buf1[0];
    color.y += buf3[0];

    output.coarseVertex.color = color;
    output.sv_position = mul(modelViewProjection, float4(position, 1.0));

    return output;
}

// Fragment Shader

[shader("fragment")]
float4 fragmentMain(
    VertexStageOutput vertexStageOutput) : SV_Target
{
    float3 color = vertexStageOutput.coarseVertex.color;
    color.x += buf2[0];
    color.y += buf3[0];

    return float4(color, 1.0);
}
