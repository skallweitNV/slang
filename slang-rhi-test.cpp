#include "slang-rhi.h"

using namespace slang::rhi;

int main()
{
    ComPtr<IDevice> device = createDevice();

    BufferDesc bufferDesc;
    bufferDesc.byteSize = 1024;
    auto buffer1 = device->createBuffer(bufferDesc);
    auto buffer2 = device->createBuffer(bufferDesc);

    ShaderProgramDesc programDesc;
    auto program = device->createShaderProgram(programDesc);

    ComputePipelineDesc pipelineDesc;
    pipelineDesc.program = program;
    auto pipeline = device->createComputePipeline(pipelineDesc);

    auto shaderObject = device->createShaderObject(program);
    shaderObject->setBuffer(ShaderOffset(), buffer1);
    shaderObject->setBuffer(ShaderOffset(), buffer2);

    CommandListDesc commandListDesc;
    auto commandList = device->createCommandList(commandListDesc);
    commandList->open();

    const void* data1;
    commandList->writeBuffer(buffer1, data1, 1024);
    const void* data2;
    commandList->writeBuffer(buffer2, data2, 1024);

    commandList->setComputeState(pipeline, shaderObject);
    commandList->dispatch(16, 16, 1);

    // commandList->bindComputePipeline()
    commandList->close();

    device->submitCommandList(commandList);
    device->waitForIdle();
}
