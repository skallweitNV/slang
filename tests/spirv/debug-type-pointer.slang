//TEST(compute, vulkan):SIMPLE(filecheck=SPV): -stage compute -entry computeMain -target spirv -emit-spirv-directly -g2

// This test is to check if DebugTypePointer is emitted when "-g2" option is used

//TEST_INPUT:ubuffer(data=[0], stride=4):out,name=outputBuffer
RWStructuredBuffer<float> outputBuffer;

//SPV: [[STRING_float:%[1-9][0-9]*]] = OpString "float"
//SPV: [[STRING_pValue:%[1-9][0-9]*]] = OpString "pValue"
//SPV: [[STRING_uint64:%[1-9][0-9]*]] = OpString "uint64"
//SPV: [[STRING_pNext:%[1-9][0-9]*]] = OpString "pNext"

struct LinkedNode
{
    float value;

    //SPV: [[TYPE_float:%[1-9][0-9]*]] = OpExtInst %void %{{[0-9]*}} DebugTypeBasic [[STRING_float]]
    //SPV: [[TYPE_float_ptr:%[1-9][0-9]*]] = OpExtInst %void %{{[0-9]*}} DebugTypePointer [[TYPE_float]]
    //SPV: DebugTypeMember [[STRING_pValue]] [[TYPE_float_ptr]]
    float *pValue;

    //SPV: [[TYPE_uint64:%[1-9][0-9]*]] = OpExtInst %void %{{[0-9]*}} DebugTypeBasic [[STRING_uint64]]
    //SPV: DebugTypeMember [[STRING_pNext]] [[TYPE_uint64]]
    LinkedNode *pNext;
};

float test(LinkedNode *pNode)
{
    //SPV: DebugValue %pNodeNext
    LinkedNode *pNodeNext = pNode->pNext;
    return *(pNode->pValue) + pNodeNext->value;
}

cbuffer Constants
{
    LinkedNode *head;
};

[numthreads(1,1,1)]
void computeMain()
{
    LinkedNode node; // required to emit DebugTypeXXX
    outputBuffer[0] = test(head);
}

