//TEST:REFLECTION:-stage fragment -target glsl -no-codegen
//TEST:REFLECTION:-stage fragment -target hlsl -no-codegen -profile sm_5_0
//TEST:REFLECTION:-stage fragment -target hlsl -no-codegen -profile sm_5_1
//TEST:REFLECTION:-profile spirv_1_5 -target metal -no-codegen

// Confirm that we do parameter binding correctly
// when we have both a parameter block *and* user-defined
// resource parameters that both need automatic
// binding allocation.

struct Helper
{
	Texture2D t;
	SamplerState s;	
};

ParameterBlock<Helper> a;

Texture2D b;

float4 main() : SV_target
{ return 0.0; }
