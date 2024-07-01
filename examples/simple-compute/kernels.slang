#if 1
struct P
{
    float a;
    uint b;
    RWStructuredBuffer<int> buffer;
    Texture2D<float> texture;
    SamplerState sampler;
};

//TEST_INPUT: set block0 = new{ out ubuffer(data=[0 0 0 0], stride=4) }
ParameterBlock<P> block0;

//TEST_INPUT: set block1 = new{ ubuffer(data=[0 1 2 3], stride=4) }
ParameterBlock<P> block1;
#else

RWStructuredBuffer<int> buffer0;
RWStructuredBuffer<int> buffer1;
#endif

[numthreads(32, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID, ConstantBuffer<P> block2)
{
	uint tid = dispatchThreadID.x;

    int inVal = block1.buffer[tid];
    // int inVal = buffer1[tid];

    int outVal = inVal;

    block0.buffer[tid] = outVal;
}
