// main.cpp

#include <slang.h>

#include <slang-com-ptr.h>
using Slang::ComPtr;

#include "slang-gfx.h"
#include "gfx-util/shader-cursor.h"
#include "tools/platform/window.h"
#include "source/core/slang-basic.h"
#include "source/core/slang-process-util.h"
using namespace gfx;

#include <string>

using A = uint32_t;
using Color = uint32_t;

ComPtr<slang::IModule> compileShaderModuleFromFile(slang::ISession* slangSession, char const* filePath)
{
    SlangCompileRequest* slangRequest = nullptr;
    slangSession->createCompileRequest(&slangRequest);

    int translationUnitIndex = spAddTranslationUnit(slangRequest, SLANG_SOURCE_LANGUAGE_SLANG, filePath);
    spAddTranslationUnitSourceFile(slangRequest, translationUnitIndex, filePath);
    spSetDumpIntermediates(slangRequest, true);

    const SlangResult compileRes = spCompile(slangRequest);
    if (auto diagnostics = spGetDiagnosticOutput(slangRequest))
    {
        printf("%s", diagnostics);
    }

    if (SLANG_FAILED(compileRes))
    {
        spDestroyCompileRequest(slangRequest);
        return ComPtr<slang::IModule>();
    }

    ComPtr<slang::IModule> slangModule;
    spCompileRequest_getModule(slangRequest, translationUnitIndex, slangModule.writeRef());
    spDestroyCompileRequest(slangRequest);
    return slangModule;
}

struct ExampleProgram
{
ComPtr<gfx::IDevice> gDevice;

ComPtr<slang::ISession> gSlangSession;
ComPtr<slang::IModule> gSlangModule;
ComPtr<gfx::IShaderProgram> gProgram;

ComPtr<gfx::IPipelineState> gPipelineState;

ComPtr<gfx::IShaderProgram> loadComputeProgram(slang::IModule* slangModule, char const* entryPointName)
{
    ComPtr<slang::IEntryPoint> entryPoint;
    slangModule->findEntryPointByName(entryPointName, entryPoint.writeRef());

    ComPtr<slang::IComponentType> linkedProgram;
    entryPoint->link(linkedProgram.writeRef());

    gfx::IShaderProgram::Desc programDesc = {};
    programDesc.slangGlobalScope = linkedProgram;

    return gDevice->createProgram(programDesc);
}

Result execute()
{
    IDevice::Desc deviceDesc;
    deviceDesc.deviceType = DeviceType::Metal;
    Result res = gfxCreateDevice(&deviceDesc, gDevice.writeRef());
    if (SLANG_FAILED(res)) return res;

    gSlangSession = gDevice->getSlangSession();
    gSlangModule = compileShaderModuleFromFile(gSlangSession, "kernels.slang");

    gProgram = loadComputeProgram(gSlangModule, "computeMain");
    if (!gProgram) return SLANG_FAIL;

    ComputePipelineStateDesc desc;
    desc.program = gProgram;
    auto gPipelineState = gDevice->createComputePipelineState(desc);
    if (!gPipelineState) return SLANG_FAIL;

    size_t bufferSize = 4 * sizeof(int);

    int dataA[4];
    int dataB[4];
    for (int i = 0; i < 4; i++)
    {
        dataA[i] = i;
        dataB[i] = 1024 - i;
    }

    IBufferResource::Desc bufferDesc = {};
    bufferDesc.type = IResource::Type::Buffer;
    bufferDesc.sizeInBytes = bufferSize;
    bufferDesc.defaultState = ResourceState::ShaderResource;
    bufferDesc.allowedStates = ResourceStateSet(
        ResourceState::CopySource, ResourceState::CopyDestination, ResourceState::ShaderResource, ResourceState::UnorderedAccess);
    bufferDesc.memoryType = MemoryType::DeviceLocal;
    auto bufferA = gDevice->createBufferResource(bufferDesc, dataA);
    auto bufferB = gDevice->createBufferResource(bufferDesc, dataB);
    auto bufferC = gDevice->createBufferResource(bufferDesc);
    if (!bufferA || !bufferB || !bufferC) return SLANG_FAIL;

#if 1
    IResourceView::Desc srvDesc = {};
    srvDesc.type = IResourceView::Type::ShaderResource;
    IResourceView::Desc uavDesc = {};
    uavDesc.type = IResourceView::Type::UnorderedAccess;
    auto bufferViewA = gDevice->createBufferView(bufferA, nullptr, srvDesc);
    auto bufferViewB = gDevice->createBufferView(bufferB, nullptr, srvDesc);
    auto bufferViewC = gDevice->createBufferView(bufferC, nullptr, uavDesc);
    if (!bufferViewA || !bufferViewB || !bufferViewC) return SLANG_FAIL;
#endif

    ITransientResourceHeap::Desc transientResourceHeapDesc = {};
    transientResourceHeapDesc.constantBufferSize = 256;
    auto transientHeap = gDevice->createTransientResourceHeap(transientResourceHeapDesc);
    if (!transientHeap) return SLANG_FAIL;

    ICommandQueue::Desc queueDesc = {ICommandQueue::QueueType::Graphics};
    auto queue = gDevice->createCommandQueue(queueDesc);
    if (!queue) return SLANG_FAIL;
    auto commandBuffer = transientHeap->createCommandBuffer();
    if (!commandBuffer) return SLANG_FAIL;
#if 1
    auto encoder = commandBuffer->encodeComputeCommands();
    if (!encoder) return SLANG_FAIL;
    auto rootShaderObject = encoder->bindPipeline(gPipelineState);
    if (!rootShaderObject) return SLANG_FAIL;
    auto cursor = ShaderCursor(rootShaderObject);
    // cursor["bufferA"].setResource(bufferViewA);
    // cursor["bufferB"].setResource(bufferViewB);
    // cursor["bufferC"].setResource(bufferViewC);
    float f = 10.f;
    // cursor["cb1"]["f"].setData(f);
    // cursor["cb2"]["f"].setData(f);
    cursor["block0"]["buffer"].setResource(bufferViewC);
    cursor["block1"]["buffer"].setResource(bufferViewA);
    // cursor["buffer0"].setResource(bufferViewC);
    // cursor["buffer1"].setResource(bufferViewA);
    // encoder->dispatchCompute(1024 / 32, 1, 1);
    encoder->dispatchCompute(1, 1, 1);
    encoder->bufferBarrier(bufferC, ResourceState::UnorderedAccess, ResourceState::CopySource);
    encoder->endEncoding();
#else
    auto encoder = commandBuffer->encodeResourceCommands();
    if (!encoder) return SLANG_FAIL;
    encoder->copyBuffer(bufferC, 0, bufferB, 0, bufferSize);
    encoder->endEncoding();
#endif
    commandBuffer->close();
    queue->executeCommandBuffer(commandBuffer);

    ComPtr<ISlangBlob> blob;
    gDevice->readBufferResource(bufferC, 0, bufferSize, blob.writeRef());

    const int* dataC = (const int*)blob->getBufferPointer();
    for (int i = 0; i < 4; i++)
    {
        printf("%d\n", dataC[i]);
    }

    return SLANG_OK;
}

};

int main()
{
    ExampleProgram app;
    if (SLANG_FAILED(app.execute()))
    {
        return -1;
    }
    return 0;
}
