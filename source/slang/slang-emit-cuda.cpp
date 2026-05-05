// slang-emit-cuda.cpp
#include "slang-emit-cuda.h"

#include "../core/slang-writer.h"
#include "slang-emit-source-writer.h"
#include "slang-rich-diagnostics.h"

#include <assert.h>

namespace Slang
{

static void emitUnsupportedTargetIntrinsicExpr(
    CUDASourceEmitter* emitter,
    IRInst* inst,
    const char* operation,
    SourceLoc location)
{
    emitter->getSink()->diagnose(
        Diagnostics::UnsupportedTargetIntrinsic{.operation = operation, .location = location});
    emitter->getSourceWriter()->emit("(");
    emitter->emitType(inst->getDataType());
    emitter->getSourceWriter()->emit("{})");
}

static UnownedStringSlice getOptixCoopVecComponentTypeName(int componentType)
{
    switch (componentType)
    {
    case SLANG_SCALAR_TYPE_FLOAT_E4M3:
        return UnownedStringSlice("OPTIX_COOP_VEC_ELEM_TYPE_FLOAT8_E4M3");
    case SLANG_SCALAR_TYPE_FLOAT_E5M2:
        return UnownedStringSlice("OPTIX_COOP_VEC_ELEM_TYPE_FLOAT8_E5M2");
    case SLANG_SCALAR_TYPE_FLOAT16:
        return UnownedStringSlice("OPTIX_COOP_VEC_ELEM_TYPE_FLOAT16");
    case SLANG_SCALAR_TYPE_FLOAT32:
        return UnownedStringSlice("OPTIX_COOP_VEC_ELEM_TYPE_FLOAT32");
    case SLANG_SCALAR_TYPE_INT8:
        return UnownedStringSlice("OPTIX_COOP_VEC_ELEM_TYPE_INT8");
    case SLANG_SCALAR_TYPE_INT32:
        return UnownedStringSlice("OPTIX_COOP_VEC_ELEM_TYPE_INT32");
    case SLANG_SCALAR_TYPE_UINT8:
        return UnownedStringSlice("OPTIX_COOP_VEC_ELEM_TYPE_UINT8");
    case SLANG_SCALAR_TYPE_UINT32:
        return UnownedStringSlice("OPTIX_COOP_VEC_ELEM_TYPE_UINT32");
    default:
        return UnownedStringSlice();
    }
}

static UnownedStringSlice getOptixCoopVecMatrixLayoutName(int matrixLayout)
{
    switch (matrixLayout)
    {
    case SLANG_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR:
        return UnownedStringSlice("OPTIX_COOP_VEC_MATRIX_LAYOUT_ROW_MAJOR");
    case SLANG_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR:
        return UnownedStringSlice("OPTIX_COOP_VEC_MATRIX_LAYOUT_COLUMN_MAJOR");
    case SLANG_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL:
        return UnownedStringSlice("OPTIX_COOP_VEC_MATRIX_LAYOUT_INFERENCING_OPTIMAL");
    case SLANG_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL:
        return UnownedStringSlice("OPTIX_COOP_VEC_MATRIX_LAYOUT_TRAINING_OPTIMAL");
    default:
        SLANG_UNEXPECTED("invalid OptiX cooperative vector matrix layout");
    }
}

struct FragmentShape
{
    int m, n, k;

    bool isValid() const { return m > 0 && n > 0 && k > 0; }
};

inline FragmentShape computeShapeCombination(uint32_t matrixUse, uint32_t row, uint32_t col);
static bool coopMatMulAddTypeCombinationIsValid(IROp aType, IROp bType, IROp cType, IROp dType);

static CUDAExtensionTracker::BaseTypeFlags _findBaseTypesUsed(IRModule* module)
{
    typedef CUDAExtensionTracker::BaseTypeFlags Flags;

    // All basic types are hoistable so must be in global scope.
    Flags baseTypesUsed = 0;

    auto moduleInst = module->getModuleInst();

    // Search all the insts in global scope, for BasicTypes
    for (auto inst : moduleInst->getChildren())
    {
        if (auto basicType = as<IRBasicType>(inst))
        {
            // Get the base type, and set the bit
            const auto baseTypeEnum = basicType->getBaseType();

            baseTypesUsed |= Flags(1) << int(baseTypeEnum);
        }
    }

    return baseTypesUsed;
}

void CUDAExtensionTracker::finalize()
{
    if (isBaseTypeRequired(BaseType::Half))
    {
        // The cuda_fp16.hpp header indicates the need is for version 5.3, but when this is tried
        // NVRTC says it cannot load builtins.
        // The lowest version that this does work for is 6.0, so that's what we use here.

        // https://docs.nvidia.com/cuda/nvrtc/index.html#group__options
        requireSMVersion(SemanticVersion(6, 0));
    }
}

UnownedStringSlice CUDASourceEmitter::getBuiltinTypeName(IROp op)
{
    switch (op)
    {
    case kIROp_VoidType:
        return UnownedStringSlice("void");
    case kIROp_BoolType:
        return UnownedStringSlice("bool");

    case kIROp_Int8Type:
        return UnownedStringSlice("char");
    case kIROp_Int16Type:
        return UnownedStringSlice("short");
    case kIROp_IntType:
        return UnownedStringSlice("int");
    case kIROp_Int64Type:
        return UnownedStringSlice("longlong");

    case kIROp_UInt8Type:
        return UnownedStringSlice("uchar");
    case kIROp_UInt16Type:
        return UnownedStringSlice("ushort");
    case kIROp_UIntType:
        return UnownedStringSlice("uint");
    case kIROp_UInt64Type:
        return UnownedStringSlice("ulonglong");

    case kIROp_IntPtrType:
        if (getPointerSize(getTargetReq()) == sizeof(uint64_t))
            return UnownedStringSlice("int64_t");
        else
            return UnownedStringSlice("int");

    case kIROp_UIntPtrType:
        if (getPointerSize(getTargetReq()) == sizeof(uint64_t))
            return UnownedStringSlice("uint64_t");
        else
            return UnownedStringSlice("uint");

    case kIROp_HalfType:
        return UnownedStringSlice("__half");

    case kIROp_FloatType:
        return UnownedStringSlice("float");
    case kIROp_DoubleType:
        return UnownedStringSlice("double");
    case kIROp_FloatE4M3Type:
        return UnownedStringSlice("__nv_fp8_e4m3");
    case kIROp_FloatE5M2Type:
        return UnownedStringSlice("__nv_fp8_e5m2");
    case kIROp_BFloat16Type:
        return UnownedStringSlice("__nv_bfloat16");
    default:
        return UnownedStringSlice();
    }
}


UnownedStringSlice CUDASourceEmitter::getVectorPrefix(IROp op)
{
    switch (op)
    {
    case kIROp_BoolType:
        return UnownedStringSlice("bool");

    case kIROp_Int8Type:
        return UnownedStringSlice("char");
    case kIROp_Int16Type:
        return UnownedStringSlice("short");
    case kIROp_IntType:
        return UnownedStringSlice("int");
    case kIROp_Int64Type:
        return UnownedStringSlice("longlong");

    case kIROp_UInt8Type:
        return UnownedStringSlice("uchar");
    case kIROp_UInt16Type:
        return UnownedStringSlice("ushort");
    case kIROp_UIntType:
        return UnownedStringSlice("uint");
    case kIROp_UInt64Type:
        return UnownedStringSlice("ulonglong");

    case kIROp_IntPtrType:
        if (getPointerSize(getTargetReq()) == sizeof(uint64_t))
            return UnownedStringSlice("longlong");
        else
            return UnownedStringSlice("int");

    case kIROp_UIntPtrType:
        if (getPointerSize(getTargetReq()) == sizeof(uint64_t))
            return UnownedStringSlice("ulonglong");
        else
            return UnownedStringSlice("uint");

    case kIROp_HalfType:
        m_extensionTracker->requireBaseType(BaseType::Half);
        return UnownedStringSlice("__half");

    case kIROp_FloatE4M3Type:
        m_extensionTracker->requireFp8();
        return UnownedStringSlice("__nv_fp8_e4m3");
    case kIROp_FloatE5M2Type:
        m_extensionTracker->requireFp8();
        return UnownedStringSlice("__nv_fp8_e5m2");
    case kIROp_BFloat16Type:
        m_extensionTracker->requireBfloat16();
        return UnownedStringSlice("__nv_bfloat16");

    case kIROp_FloatType:
        return UnownedStringSlice("float");
    case kIROp_DoubleType:
        return UnownedStringSlice("double");
    default:
        return UnownedStringSlice();
    }
}

void CUDASourceEmitter::emitTempModifiers(IRInst* temp)
{
    CPPSourceEmitter::emitTempModifiers(temp);
    if (as<IRModuleInst>(temp->getParent()))
    {
        m_writer->emit("__device__ ");
    }
}

SlangResult CUDASourceEmitter::_calcCUDATextureTypeName(
    IRTextureTypeBase* texType,
    StringBuilder& outName)
{
    // Not clear how to do this yet
    if (texType->isMultisample())
    {
        return SLANG_FAIL;
    }

    switch (texType->getAccess())
    {
    case SLANG_RESOURCE_ACCESS_READ:
        {
            outName << "CUtexObject";
            return SLANG_OK;
        }
    case SLANG_RESOURCE_ACCESS_READ_WRITE:
    case SLANG_RESOURCE_ACCESS_RASTER_ORDERED:
    case SLANG_RESOURCE_ACCESS_WRITE:
        {
            outName << "CUsurfObject";
            return SLANG_OK;
        }
    default:
        break;
    }
    return SLANG_FAIL;
}

SlangResult CUDASourceEmitter::calcTypeName(IRType* type, CodeGenTarget target, StringBuilder& out)
{
    SLANG_UNUSED(target);

    // The names CUDA produces are all compatible with 'C' (ie they aren't templated types)
    SLANG_ASSERT(
        target == CodeGenTarget::CUDASource || target == CodeGenTarget::CUDAHeader ||
        target == CodeGenTarget::CSource);

    switch (type->getOp())
    {
    case kIROp_PtrType:
    case kIROp_NativePtrType:
        {
            auto ptrType = cast<IRPtrTypeBase>(type);
            if (auto unsizedArrayType = as<IRUnsizedArrayType>(ptrType->getValueType()))
            {
                SLANG_RETURN_ON_FAIL(calcTypeName(unsizedArrayType->getElementType(), target, out));
                out << "**";
                return SLANG_OK;
            }
            break;
        }
    case kIROp_VectorType:
        {
            auto vecType = static_cast<IRVectorType*>(type);
            auto vecCount = int(getIntVal(vecType->getElementCount()));
            const IROp elemType = vecType->getElementType()->getOp();

            UnownedStringSlice prefix = getVectorPrefix(elemType);
            if (prefix.getLength() <= 0)
            {
                return SLANG_FAIL;
            }
            out << prefix << vecCount;
            return SLANG_OK;
        }
    case kIROp_TensorViewType:
        {
            out << "TensorView";
            return SLANG_OK;
        }
    case kIROp_CoopVectorType:
        {
            if (isOptixCoopVec)
            {
                auto coopVecType = static_cast<IRCoopVectorType*>(type);
                auto elemCount = int(getIntVal(coopVecType->getElementCount()));
                auto elemType = coopVecType->getElementType();

                out << "OptixCoopVec<" << getBuiltinTypeName(elemType->getOp()) << ", " << elemCount
                    << ">";
                return SLANG_OK;
            }
            SLANG_DIAGNOSE_UNEXPECTED(
                getSink(),
                SourceLoc(),
                "Cooperative vectors should have been lowered before reaching CUDA emit for "
                "non-OptiX targets");
            return SLANG_FAIL;
        }
    case kIROp_RaytracingAccelerationStructureType:
    case kIROp_HitObjectType:
        {
            out << "OptixTraversableHandle";
            return SLANG_OK;
        }
    case kIROp_CoopMatrixType:
        {
            auto coopType = as<IRCoopMatrixType>(type);
            auto result = emitWMMAFragmentType(coopType, out);
            m_extensionTracker->requireSMVersion(SemanticVersion(8, 0));
            // FP8 mma instructions (mma.sync.m16n8k16 with .e4m3/.e5m2) were
            // introduced in PTX ISA 8.7 / SM 8.9 (Ada Lovelace).  Earlier SM
            // targets reject the PTX as invalid at JIT time.
            auto elemOp = coopType->getElementType()->getOp();
            if (elemOp == kIROp_FloatE4M3Type || elemOp == kIROp_FloatE5M2Type)
                m_extensionTracker->requireSMVersion(SemanticVersion(8, 9));
            return result;
        }
    case kIROp_FloatE4M3Type:
        out << "__nv_fp8_e4m3";
        m_extensionTracker->requireFp8();
        return SLANG_OK;
    case kIROp_FloatE5M2Type:
        out << "__nv_fp8_e5m2";
        m_extensionTracker->requireFp8();
        return SLANG_OK;
    case kIROp_BFloat16Type:
        out << "__nv_bfloat16";
        m_extensionTracker->requireBfloat16();
        return SLANG_OK;
    default:
        {
            if (isNominalOp(type->getOp()))
            {
                out << getName(type);
                return SLANG_OK;
            }

            if (IRBasicType::isaImpl(type->getOp()))
            {
                out << getBuiltinTypeName(type->getOp());
                return SLANG_OK;
            }

            if (auto texType = as<IRTextureTypeBase>(type))
            {
                return _calcCUDATextureTypeName(texType, out);
            }

            switch (type->getOp())
            {
            case kIROp_SamplerStateType:
                out << "SamplerState";
                return SLANG_OK;
            case kIROp_SamplerComparisonStateType:
                out << "SamplerComparisonState";
                return SLANG_OK;
            default:
                break;
            }

            break;
        }
    }

    return Super::calcTypeName(type, target, out);
}

void CUDASourceEmitter::emitLayoutSemanticsImpl(
    IRInst* inst,
    char const* uniformSemanticSpelling,
    EmitLayoutSemanticOption layoutSemanticOption)
{
    Super::emitLayoutSemanticsImpl(inst, uniformSemanticSpelling, layoutSemanticOption);
}

void CUDASourceEmitter::emitParameterGroupImpl(
    IRGlobalParam* varDecl,
    IRUniformParameterGroupType* type)
{
    auto elementType = type->getElementType();

    m_writer->emit("extern \"C\" __constant__ ");
    emitType(elementType, "SLANG_globalParams");
    m_writer->emit(";\n");

    m_writer->emit("#define ");
    m_writer->emit(getName(varDecl));
    m_writer->emit(" (&SLANG_globalParams)\n");
}

void CUDASourceEmitter::emitEntryPointAttributesImpl(
    IRFunc* irFunc,
    IREntryPointDecoration* entryPointDecor)
{
    SLANG_UNUSED(irFunc);
    SLANG_UNUSED(entryPointDecor);
}

void CUDASourceEmitter::emitFunctionPreambleImpl(IRInst* inst)
{
    if (!inst)
        return;
    if (inst->findDecoration<IREntryPointDecoration>())
    {
        m_writer->emit("extern \"C\" __global__ ");
        return;
    }

    if (inst->findDecoration<IRCudaKernelDecoration>())
    {
        m_writer->emit("__global__ ");
    }
    else if (inst->findDecoration<IRCudaHostDecoration>())
    {
        m_writer->emit("__host__ ");
    }
    else
    {
        m_writer->emit("__device__ ");
    }
}

String CUDASourceEmitter::generateEntryPointNameImpl(IREntryPointDecoration* entryPointDecor)
{
    // We have an entry-point function in the IR module, which we
    // will want to emit as a `__global__` function in the generated
    // CUDA C++.
    //
    // The most common case will be a compute kernel, in which case
    // we will emit the function more or less as-is, including
    // usingits original name as the name of the global symbol.
    //
    String funcName = Super::generateEntryPointNameImpl(entryPointDecor);
    String globalSymbolName = funcName;

    // We also suport emitting ray tracing kernels for use with
    // OptiX, and in that case the name of the global symbol
    // must be prefixed to indicate to the OptiX runtime what
    // stage it is to be compiled for.
    //
    auto stage = entryPointDecor->getProfile().getStage();
    switch (stage)
    {
    default:
        break;

#define CASE(STAGE, PREFIX)                    \
    case Stage::STAGE:                         \
        globalSymbolName = #PREFIX + funcName; \
        break

        // Optix 7 Guide, Section 6.1 (Program input)
        //
        // > The input PTX should include one or more NVIDIA OptiX programs.
        // > The type of program affects how the program can be used during
        // > the execution of the pipeline. These program types are specified
        // by prefixing the program name with the following:
        //
        // >    Program type        Function name prefix
        CASE(RayGeneration, __raygen__);
        CASE(Intersection, __intersection__);
        CASE(AnyHit, __anyhit__);
        CASE(ClosestHit, __closesthit__);
        CASE(Miss, __miss__);
        CASE(Callable, __direct_callable__);
        //
        // There are two stages (or "program types") supported by OptiX
        // that Slang currently cannot target:
        //
        // CASE(ContinuationCallable,   __continuation_callable__);
        // CASE(Exception,              __exception__);
        //
#undef CASE
    }

    return globalSymbolName;
}

void CUDASourceEmitter::emitGlobalRTTISymbolPrefix()
{
    m_writer->emit("__constant__ ");
}

void CUDASourceEmitter::emitLoopControlDecorationImpl(IRLoopControlDecoration* decl)
{
    if (decl->getMode() == kIRLoopControl_Unroll)
    {
        m_writer->emit("#pragma unroll\n");
    }
}

void CUDASourceEmitter::_emitInitializerListValue(IRType* dstType, IRInst* value)
{
    // When constructing a matrix or vector from a single value this is handled by the default path

    switch (value->getOp())
    {
    case kIROp_MakeVector:
    case kIROp_MakeMatrix:
        {
            IRType* type = value->getDataType();

            // If the types are the same, we can can just break down and use
            if (dstType == type)
            {
                if (auto vecType = as<IRVectorType>(type))
                {
                    if (UInt(getIntVal(vecType->getElementCount())) == value->getOperandCount())
                    {
                        emitType(type);
                        _emitInitializerList(
                            vecType->getElementType(),
                            value->getOperands(),
                            value->getOperandCount());
                        return;
                    }
                }
                else if (auto matType = as<IRMatrixType>(type))
                {
                    const Index colCount = Index(getIntVal(matType->getColumnCount()));
                    const Index rowCount = Index(getIntVal(matType->getRowCount()));

                    // TODO(JS): If num cols = 1, then it *doesn't* actually return a vector.
                    // That could be argued is an error because we want swizzling or [] to work.
                    IRBuilder builder(matType->getModule());
                    builder.setInsertBefore(matType);
                    const Index operandCount = Index(value->getOperandCount());

                    // Can init, with vectors.
                    // For now special case if the rowVectorType is not actually a vector (when
                    // elementSize == 1)
                    if (operandCount == rowCount)
                    {
                        // Emit the braces for the Matrix struct, and then each row vector in its
                        // own line.
                        emitType(matType);
                        m_writer->emit("{\n");
                        m_writer->indent();
                        for (Index i = 0; i < rowCount; ++i)
                        {
                            if (i != 0)
                                m_writer->emit(",\n");
                            emitType(matType->getElementType());
                            m_writer->emit(colCount);
                            _emitInitializerList(
                                matType->getElementType(),
                                value->getOperand(i)->getOperands(),
                                colCount);
                        }
                        m_writer->dedent();
                        m_writer->emit("\n}");
                        return;
                    }
                    else if (operandCount == rowCount * colCount)
                    {
                        // Handle if all are explicitly defined
                        IRType* elementType = matType->getElementType();
                        IRUse* operands = value->getOperands();

                        // Emit the braces for the Matrix struct, and the elements of each row in
                        // its own line.
                        emitType(matType);
                        m_writer->emit("{\n");
                        m_writer->indent();
                        for (Index i = 0; i < rowCount; ++i)
                        {
                            if (i != 0)
                                m_writer->emit(",\n");
                            _emitInitializerListContent(elementType, operands, colCount);
                            operands += colCount;
                        }
                        m_writer->dedent();
                        m_writer->emit("\n}");
                        return;
                    }
                }
            }

            break;
        }
    }

    // All other cases we just use the default emitting - might not work on arrays defined in global
    // scope on CUDA though
    emitOperand(value, getInfo(EmitOp::General));
}

void CUDASourceEmitter::_emitInitializerListContent(
    IRType* elementType,
    IRUse* operands,
    Index operandCount)
{
    for (Index i = 0; i < operandCount; ++i)
    {
        if (i != 0)
            m_writer->emit(", ");
        _emitInitializerListValue(elementType, operands[i].get());
    }
}


void CUDASourceEmitter::_emitInitializerList(
    IRType* elementType,
    IRUse* operands,
    Index operandCount)
{
    m_writer->emit("{\n");
    m_writer->indent();

    _emitInitializerListContent(elementType, operands, operandCount);

    m_writer->dedent();
    m_writer->emit("\n}");
}

// Find the IRFormatDecoration on a resource instruction, traversing through loads/field addresses.
static IRFormatDecoration* _findImageFormatDecorationForCUDA(IRInst* resourceInst)
{
    if (IRLoad* load = as<IRLoad>(resourceInst))
    {
        if (IRFieldAddress* fieldAddress = as<IRFieldAddress>(load->getOperand(0)))
        {
            IRInst* field = fieldAddress->getField();
            return field->findDecoration<IRFormatDecoration>();
        }
    }
    return resourceInst->findDecoration<IRFormatDecoration>();
}

// Helper to map SlangScalarType to BaseType.
static BaseType _scalarTypeToBaseType(SlangScalarType scalarType)
{
    switch (scalarType)
    {
    case SLANG_SCALAR_TYPE_UINT8:
        return BaseType::UInt8;
    case SLANG_SCALAR_TYPE_INT8:
        return BaseType::Int8;
    case SLANG_SCALAR_TYPE_UINT16:
        return BaseType::UInt16;
    case SLANG_SCALAR_TYPE_INT16:
        return BaseType::Int16;
    case SLANG_SCALAR_TYPE_UINT32:
        return BaseType::UInt;
    case SLANG_SCALAR_TYPE_INT32:
        return BaseType::Int;
    case SLANG_SCALAR_TYPE_UINT64:
        return BaseType::UInt64;
    case SLANG_SCALAR_TYPE_INT64:
        return BaseType::Int64;
    case SLANG_SCALAR_TYPE_FLOAT16:
        return BaseType::Half;
    case SLANG_SCALAR_TYPE_FLOAT32:
        return BaseType::Float;
    case SLANG_SCALAR_TYPE_FLOAT64:
        return BaseType::Double;
    default:
        return BaseType::Void;
    }
}

// Check if format and element type are compatible (no conversion needed).
static bool _isImageFormatCompatibleCUDA(ImageFormat imageFormat, IRType* dataType)
{
    int numElems = 1;
    if (auto vecType = as<IRVectorType>(dataType))
    {
        numElems = int(getIntVal(vecType->getElementCount()));
        dataType = vecType->getElementType();
    }

    BaseType baseType = BaseType::Void;
    if (auto basicType = as<IRBasicType>(dataType))
        baseType = basicType->getBaseType();

    const auto& info = getImageFormatInfo(imageFormat);

    if (numElems != info.channelCount)
        return false;

    BaseType formatBaseType = _scalarTypeToBaseType(info.scalarType);
    return formatBaseType == baseType;
}

// Determine if a format conversion is required for a surface access.
static bool _isCUDAConvertRequired(ImageFormat imageFormat, IRInst* resourceInst)
{
    auto textureType = as<IRTextureTypeBase>(resourceInst->getDataType());
    IRType* elementType = textureType ? textureType->getElementType() : nullptr;
    return elementType && !_isImageFormatCompatibleCUDA(imageFormat, elementType);
}

// Get CUDA storage type name for a given scalar type and channel count.
static const char* _getCUDAStorageScalarTypeName(SlangScalarType scalarType)
{
    switch (scalarType)
    {
    case SLANG_SCALAR_TYPE_UINT8:
        return "uchar";
    case SLANG_SCALAR_TYPE_INT8:
        return "char";
    case SLANG_SCALAR_TYPE_UINT16:
    case SLANG_SCALAR_TYPE_FLOAT16: // Half floats stored as ushort
        return "ushort";
    case SLANG_SCALAR_TYPE_INT16:
        return "short";
    case SLANG_SCALAR_TYPE_UINT32:
        return "uint";
    case SLANG_SCALAR_TYPE_INT32:
        return "int";
    case SLANG_SCALAR_TYPE_FLOAT32:
        return "float";
    default:
        return nullptr;
    }
}

void CUDASourceEmitter::_emitCUDAStorageTypeName(const ImageFormatInfo& info)
{
    const char* scalarName = _getCUDAStorageScalarTypeName(info.scalarType);
    SLANG_ASSERT(scalarName);
    m_writer->emit(scalarName);
    if (info.channelCount > 1)
        m_writer->emitUInt64(info.channelCount);
}

void CUDASourceEmitter::_emitCUDAUnpackExpr(
    const ImageFormatInfo& info,
    IRType* elementType,
    const char* storageVarName)
{
    // Determine the element's scalar base type
    IRType* scalarType = elementType;
    int elemCount = 1;
    if (auto vecType = as<IRVectorType>(elementType))
    {
        elemCount = int(getIntVal(vecType->getElementCount()));
        scalarType = vecType->getElementType();
    }

    BaseType elemBaseType = BaseType::Void;
    if (auto basicType = as<IRBasicType>(scalarType))
        elemBaseType = basicType->getBaseType();

    int formatChannels = info.channelCount;
    const char* components[] = {"x", "y", "z", "w"};

    // Emit make_TYPE(...)
    if (elemCount > 1)
    {
        m_writer->emit("make_");
        emitSimpleType(elementType);
        m_writer->emit("(");
    }

    for (int i = 0; i < elemCount; i++)
    {
        if (i > 0)
            m_writer->emit(", ");

        if (i >= formatChannels)
        {
            // Zero-fill channels beyond what the format provides
            m_writer->emit("0");
            continue;
        }

        // Get the source component expression
        StringBuilder srcExpr;
        srcExpr << storageVarName;
        if (formatChannels > 1)
            srcExpr << "." << components[i];

        switch (info.formatKind)
        {
        case ImageFormatKind::Unorm:
            // storage is UINT8/UINT16 -> float: val / maxVal
            if (elemBaseType == BaseType::Float)
            {
                m_writer->emit("(");
                m_writer->emit(srcExpr);
                m_writer->emit(" / ");
                m_writer->emit(
                    (info.scalarType == SLANG_SCALAR_TYPE_UINT8) ? "255.0f" : "65535.0f");
                m_writer->emit(")");
            }
            else
            {
                m_writer->emit("(");
                emitSimpleType(scalarType);
                m_writer->emit(")(");
                m_writer->emit(srcExpr);
                m_writer->emit(")");
            }
            break;

        case ImageFormatKind::Snorm:
            // storage is UINT8/UINT16 reinterpreted as signed -> float: max(val / maxVal,
            // -1.0f)
            if (elemBaseType == BaseType::Float)
            {
                const char* signedType =
                    (info.scalarType == SLANG_SCALAR_TYPE_UINT8) ? "char" : "short";
                const char* maxValStr =
                    (info.scalarType == SLANG_SCALAR_TYPE_UINT8) ? "127.0f" : "32767.0f";
                m_writer->emit("fmaxf((float)(");
                m_writer->emit(signedType);
                m_writer->emit(")(");
                m_writer->emit(srcExpr);
                m_writer->emit(") / ");
                m_writer->emit(maxValStr);
                m_writer->emit(", -1.0f)");
            }
            else
            {
                m_writer->emit("(");
                emitSimpleType(scalarType);
                m_writer->emit(")(");
                m_writer->emit(srcExpr);
                m_writer->emit(")");
            }
            break;

        case ImageFormatKind::HalfFloat:
            // storage is ushort -> float via half: __half2float(__ushort_as_half(val))
            if (elemBaseType == BaseType::Float)
            {
                m_writer->emit("__half2float(__ushort_as_half(");
                m_writer->emit(srcExpr);
                m_writer->emit("))");
            }
            else if (elemBaseType == BaseType::Half)
            {
                m_writer->emit("__ushort_as_half(");
                m_writer->emit(srcExpr);
                m_writer->emit(")");
            }
            else
            {
                m_writer->emit("(");
                emitSimpleType(scalarType);
                m_writer->emit(")(");
                m_writer->emit(srcExpr);
                m_writer->emit(")");
            }
            break;

        case ImageFormatKind::Uint:
        case ImageFormatKind::Sint:
            // Direct cast/widen
            m_writer->emit("(");
            emitSimpleType(scalarType);
            m_writer->emit(")(");
            m_writer->emit(srcExpr);
            m_writer->emit(")");
            break;

        default:
            // Float or unknown - direct cast
            m_writer->emit("(");
            emitSimpleType(scalarType);
            m_writer->emit(")(");
            m_writer->emit(srcExpr);
            m_writer->emit(")");
            break;
        }
    }

    if (elemCount > 1)
    {
        m_writer->emit(")");
    }
}

void CUDASourceEmitter::_emitCUDAPackExpr(
    const ImageFormatInfo& info,
    IRType* elementType,
    const char* valueVarName)
{
    // Determine the element's scalar base type
    IRType* scalarType = elementType;
    int elemCount = 1;
    if (auto vecType = as<IRVectorType>(elementType))
    {
        elemCount = int(getIntVal(vecType->getElementCount()));
        scalarType = vecType->getElementType();
    }

    BaseType elemBaseType = BaseType::Void;
    if (auto basicType = as<IRBasicType>(scalarType))
        elemBaseType = basicType->getBaseType();

    int formatChannels = info.channelCount;
    const char* components[] = {"x", "y", "z", "w"};

    // For vector storage types, emit make_StorageType(...); for scalar, just emit the expression
    bool isVectorStorage = formatChannels > 1;
    if (isVectorStorage)
    {
        m_writer->emit("make_");
        _emitCUDAStorageTypeName(info);
        m_writer->emit("(");
    }

    for (int i = 0; i < formatChannels; i++)
    {
        if (i > 0)
            m_writer->emit(", ");

        // Get the source component expression
        StringBuilder srcExpr;
        srcExpr << valueVarName;
        if (elemCount > 1 && i < elemCount)
            srcExpr << "." << components[i];

        // If element has fewer channels than format, use 0
        if (i >= elemCount)
        {
            const char* storageCastType = _getCUDAStorageScalarTypeName(info.scalarType);
            m_writer->emit("(");
            m_writer->emit(storageCastType);
            m_writer->emit(")0");
            continue;
        }

        switch (info.formatKind)
        {
        case ImageFormatKind::Unorm:
            // float -> UINT8/UINT16: (uchar)(__saturatef(val) * 255.0f + 0.5f)
            if (elemBaseType == BaseType::Float)
            {
                const char* storageCastType = _getCUDAStorageScalarTypeName(info.scalarType);
                const char* maxValStr =
                    (info.scalarType == SLANG_SCALAR_TYPE_UINT8) ? "255.0f" : "65535.0f";
                m_writer->emit("(");
                m_writer->emit(storageCastType);
                m_writer->emit(")(__saturatef(");
                m_writer->emit(srcExpr);
                m_writer->emit(") * ");
                m_writer->emit(maxValStr);
                m_writer->emit(" + 0.5f)");
            }
            else
            {
                const char* storageCastType = _getCUDAStorageScalarTypeName(info.scalarType);
                m_writer->emit("(");
                m_writer->emit(storageCastType);
                m_writer->emit(")(");
                m_writer->emit(srcExpr);
                m_writer->emit(")");
            }
            break;

        case ImageFormatKind::Snorm:
            // float -> INT8/INT16: (char)(fmaxf(fminf(val, 1.0f), -1.0f) * 127.0f + (val >= 0
            // ? 0.5f : -0.5f))
            if (elemBaseType == BaseType::Float)
            {
                const char* storageCastType = _getCUDAStorageScalarTypeName(info.scalarType);
                const char* maxValStr =
                    (info.scalarType == SLANG_SCALAR_TYPE_UINT8) ? "127.0f" : "32767.0f";
                m_writer->emit("(");
                m_writer->emit(storageCastType);
                m_writer->emit(")(fmaxf(fminf(");
                m_writer->emit(srcExpr);
                m_writer->emit(", 1.0f), -1.0f) * ");
                m_writer->emit(maxValStr);
                m_writer->emit(" + (");
                m_writer->emit(srcExpr);
                m_writer->emit(" >= 0 ? 0.5f : -0.5f))");
            }
            else
            {
                const char* storageCastType = _getCUDAStorageScalarTypeName(info.scalarType);
                m_writer->emit("(");
                m_writer->emit(storageCastType);
                m_writer->emit(")(");
                m_writer->emit(srcExpr);
                m_writer->emit(")");
            }
            break;

        case ImageFormatKind::HalfFloat:
            // float -> ushort: __half_as_ushort(__float2half(val))
            if (elemBaseType == BaseType::Float)
            {
                m_writer->emit("__half_as_ushort(__float2half(");
                m_writer->emit(srcExpr);
                m_writer->emit("))");
            }
            else if (elemBaseType == BaseType::Half)
            {
                m_writer->emit("__half_as_ushort(");
                m_writer->emit(srcExpr);
                m_writer->emit(")");
            }
            else
            {
                m_writer->emit("(ushort)(");
                m_writer->emit(srcExpr);
                m_writer->emit(")");
            }
            break;

        case ImageFormatKind::Uint:
        case ImageFormatKind::Sint:
        {
            // Direct cast
            const char* storageCastType = _getCUDAStorageScalarTypeName(info.scalarType);
            m_writer->emit("(");
            m_writer->emit(storageCastType);
            m_writer->emit(")(");
            m_writer->emit(srcExpr);
            m_writer->emit(")");
            break;
        }

        default:
        {
            const char* storageCastType = _getCUDAStorageScalarTypeName(info.scalarType);
            m_writer->emit("(");
            m_writer->emit(storageCastType);
            m_writer->emit(")(");
            m_writer->emit(srcExpr);
            m_writer->emit(")");
            break;
        }
        }
    }

    if (isVectorStorage)
    {
        m_writer->emit(")");
    }
}

// Parse a CUDA surface intrinsic definition string to determine:
// - The surface function name (e.g., "surf2Dread", "surf2Dwrite")
// - Whether it is a read or write
// Returns the function name portion before "$C" or returns empty if not a surface call.
static bool _parseCUDASurfaceIntrinsic(
    UnownedStringSlice intrinsicDef,
    StringBuilder& outFuncName,
    bool& outIsWrite)
{
    // Surface intrinsics look like:
    //   "surf2Dread$C<$T0>($0, ($1).x * $E, ($1).y, SLANG_CUDA_BOUNDARY_MODE)"
    //   "surf2Dwrite$C<$T0>($2, $0, ($1).x * $E, ($1).y, SLANG_CUDA_BOUNDARY_MODE)"

    // Check if it starts with "surf"
    if (!intrinsicDef.startsWith(toSlice("surf")))
        return false;

    // Find "$C" in the string - this marks a surface call that can have conversion
    auto dollarC = intrinsicDef.indexOf(toSlice("$C"));
    if (dollarC == Index(-1))
        return false;

    // Extract the function name (everything before $C)
    outFuncName.clear();
    outFuncName.append(intrinsicDef.head(dollarC));

    // Determine read vs write
    outIsWrite = outFuncName.indexOf(toSlice("write")) != Index(-1);
    return true;
}

bool CUDASourceEmitter::_tryEmitCUDASurfaceConvertCall(
    IRCall* inst,
    UnownedStringSlice intrinsicDefinition,
    EmitOpInfo const& inOuterPrec)
{
    // Parse the intrinsic to see if it's a surface read/write
    StringBuilder funcName;
    bool isWrite = false;
    if (!_parseCUDASurfaceIntrinsic(intrinsicDefinition, funcName, isWrite))
        return false;

    // Get the resource argument (always arg 0)
    IRUse* args = inst->getOperands();
    Index argCount = inst->getOperandCount();
    // Skip the callee (first operand)
    args++;
    argCount--;

    IRInst* resourceInst = args[0].get();

    // Check if format conversion is required
    IRFormatDecoration* formatDecoration = _findImageFormatDecorationForCUDA(resourceInst);
    if (!formatDecoration)
        return false; // No format decoration, let default handle it

    ImageFormat imageFormat = formatDecoration->getFormat();
    if (!_isCUDAConvertRequired(imageFormat, resourceInst))
        return false; // No conversion needed, let default handle it

    const auto& formatInfo = getImageFormatInfo(imageFormat);

    // Don't handle special formats inline
    if (formatInfo.formatKind == ImageFormatKind::Special || formatInfo.scalarType == SLANG_SCALAR_TYPE_NONE)
        return false;

    // Get the texture element type
    auto textureType = as<IRTextureTypeBase>(resourceInst->getDataType());
    IRType* elementType = textureType->getElementType();

    // If reading half formats, enable half
    if (formatInfo.formatKind == ImageFormatKind::HalfFloat)
    {
        m_extensionTracker->requireBaseType(BaseType::Half);
    }

    if (isWrite)
    {
        // Write case:
        // The intrinsic string pattern for writes is:
        //   "surf2Dwrite$C<$T0>($2, $0, ($1).x * $E, ($1).y, SLANG_CUDA_BOUNDARY_MODE)"
        // args: $0=resource, $1=location, $2=newValue
        //
        // We need to emit:
        // ([&]() {
        //   auto _val = <newValue>;
        //   <StorageType> _packed = make_<StorageType>(<pack expressions>);
        //   surf2Dwrite<StorageType>(_packed, <resource>, <coord>.x * <elemSize>, <coord>.y, SLANG_CUDA_BOUNDARY_MODE);
        // })()

        auto outerPrec = inOuterPrec;
        auto prec = getInfo(EmitOp::Postfix);
        bool needClose = maybeEmitParens(outerPrec, prec);

        m_writer->emit("([&]() {\n");
        m_writer->indent();

        // Emit: auto _val = <newValue>;
        m_writer->emit("auto _slang_val = ");
        emitOperand(args[2].get(), getInfo(EmitOp::General));
        m_writer->emit(";\n");

        // Emit: <StorageType> _packed = make_<StorageType>(<pack expressions>);
        _emitCUDAStorageTypeName(formatInfo);
        m_writer->emit(" _slang_packed = ");
        _emitCUDAPackExpr(formatInfo, elementType, "_slang_val");
        m_writer->emit(";\n");

        // Emit: surfNDwrite<StorageType>(_packed, resource, coord.x * elemSize, coord.y, ...);
        m_writer->emit(funcName);
        m_writer->emit("<");
        _emitCUDAStorageTypeName(formatInfo);
        m_writer->emit(">(_slang_packed, ");
        emitOperand(args[0].get(), getInfo(EmitOp::General));
        m_writer->emit(", ");

        // Emit coordinate arguments with byte-addressing on x
        // Parse the intrinsic string to figure out the dimensionality from the function name
        // For write intrinsics, the coordinates are in args[1]
        // The x coordinate needs to be multiplied by the storage element size
        m_writer->emit("(");
        emitOperand(args[1].get(), getInfo(EmitOp::General));

        // Check if it's a 1D non-array case (where args[1] is scalar, not a vector)
        auto coordType = args[1].get()->getDataType();
        bool isScalarCoord = !as<IRVectorType>(coordType);

        if (isScalarCoord)
        {
            // 1D non-array: coord is scalar
            m_writer->emit(") * ");
            m_writer->emitUInt64(formatInfo.sizeInBytes);
        }
        else
        {
            // Multi-dimensional: coord.x needs byte addressing
            m_writer->emit(").x * ");
            m_writer->emitUInt64(formatInfo.sizeInBytes);
        }

        // Emit remaining coordinate components
        if (!isScalarCoord)
        {
            auto vecType = as<IRVectorType>(coordType);
            int coordCount = int(getIntVal(vecType->getElementCount()));
            const char* components[] = {"y", "z", "w"};
            for (int i = 1; i < coordCount; i++)
            {
                m_writer->emit(", (");
                emitOperand(args[1].get(), getInfo(EmitOp::General));
                m_writer->emit(").");
                m_writer->emit(components[i - 1]);
            }
        }

        m_writer->emit(", SLANG_CUDA_BOUNDARY_MODE);\n");

        m_writer->dedent();
        m_writer->emit("})()");

        maybeCloseParens(needClose);
    }
    else
    {
        // Read case:
        // The intrinsic string pattern for reads is:
        //   "surf2Dread$C<$T0>($0, ($1).x * $E, ($1).y, SLANG_CUDA_BOUNDARY_MODE)"
        // args: $0=resource, $1=location
        //
        // We need to emit:
        // ([&]() {
        //   <StorageType> _storage = surfNDread<StorageType>(resource, coord.x * elemSize, coord.y, SLANG_CUDA_BOUNDARY_MODE);
        //   return <unpack expression>;
        // })()

        auto outerPrec = inOuterPrec;
        auto prec = getInfo(EmitOp::Postfix);
        bool needClose = maybeEmitParens(outerPrec, prec);

        m_writer->emit("([&]() {\n");
        m_writer->indent();

        // Emit: <StorageType> _storage = surfNDread<StorageType>(...);
        _emitCUDAStorageTypeName(formatInfo);
        m_writer->emit(" _slang_storage = ");
        m_writer->emit(funcName);
        m_writer->emit("<");
        _emitCUDAStorageTypeName(formatInfo);
        m_writer->emit(">(");
        emitOperand(args[0].get(), getInfo(EmitOp::General));
        m_writer->emit(", ");

        // Emit coordinate arguments with byte-addressing on x
        auto coordType = args[1].get()->getDataType();
        bool isScalarCoord = !as<IRVectorType>(coordType);

        if (isScalarCoord)
        {
            // 1D non-array: coord is scalar
            m_writer->emit("(");
            emitOperand(args[1].get(), getInfo(EmitOp::General));
            m_writer->emit(") * ");
            m_writer->emitUInt64(formatInfo.sizeInBytes);
        }
        else
        {
            // Multi-dimensional: coord.x needs byte addressing
            m_writer->emit("(");
            emitOperand(args[1].get(), getInfo(EmitOp::General));
            m_writer->emit(").x * ");
            m_writer->emitUInt64(formatInfo.sizeInBytes);

            auto vecType = as<IRVectorType>(coordType);
            int coordCount = int(getIntVal(vecType->getElementCount()));
            const char* components[] = {"y", "z", "w"};
            for (int i = 1; i < coordCount; i++)
            {
                m_writer->emit(", (");
                emitOperand(args[1].get(), getInfo(EmitOp::General));
                m_writer->emit(").");
                m_writer->emit(components[i - 1]);
            }
        }

        m_writer->emit(", SLANG_CUDA_BOUNDARY_MODE);\n");

        // Emit: return <unpack expression>;
        m_writer->emit("return ");
        _emitCUDAUnpackExpr(formatInfo, elementType, "_slang_storage");
        m_writer->emit(";\n");

        m_writer->dedent();
        m_writer->emit("})()");

        maybeCloseParens(needClose);
    }

    return true;
}

void CUDASourceEmitter::emitIntrinsicCallExprImpl(
    IRCall* inst,
    UnownedStringSlice intrinsicDefinition,
    IRInst* intrinsicInst,
    EmitOpInfo const& inOuterPrec)
{
    // This works around the problem, where some intrinsics that require the "half" type enabled
    // don't use the half/float16_t type. For example `f16tof32` can operate on float16_t *and*
    // uint. If the input is uint, although we are using the half feature (as far as CUDA is
    // concerned), the half/float16_t type is not visible/directly used.
    if (intrinsicDefinition.startsWith(toSlice("__half")))
    {
        m_extensionTracker->requireBaseType(BaseType::Half);
    }

    // Try to handle surface read/write with inline format conversion
    if (_tryEmitCUDASurfaceConvertCall(inst, intrinsicDefinition, inOuterPrec))
        return;

    Super::emitIntrinsicCallExprImpl(inst, intrinsicDefinition, intrinsicInst, inOuterPrec);
}

bool CUDASourceEmitter::tryEmitInstStmtImpl(IRInst* inst)
{
    switch (inst->getOp())
    {
    case kIROp_StructuredBufferGetDimensions:
        {
            auto count = _generateUniqueName(UnownedStringSlice("_elementCount"));
            auto stride = _generateUniqueName(UnownedStringSlice("_stride"));

            m_writer->emit("uint ");
            m_writer->emit(count);
            m_writer->emit(";\n");
            m_writer->emit("uint ");
            m_writer->emit(stride);
            m_writer->emit(";\n");
            emitOperand(
                inst->getOperand(0),
                leftSide(getInfo(EmitOp::General), getInfo(EmitOp::Postfix)));
            m_writer->emit(".GetDimensions(&");
            m_writer->emit(count);
            m_writer->emit(", &");
            m_writer->emit(stride);
            m_writer->emit(");\n");
            emitInstResultDecl(inst);
            m_writer->emit("make_uint2(");
            m_writer->emit(count);
            m_writer->emit(", ");
            m_writer->emit(stride);
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_AtomicLoad:
        {
            emitInstResultDecl(inst);
            emitDereferenceOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(";\n");
            return true;
        }
    case kIROp_AtomicStore:
        {
            emitDereferenceOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(" = ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(";\n");
            return true;
        }
    case kIROp_AtomicExchange:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicExch(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_AtomicCompareExchange:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicCAS(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(2), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_AtomicAdd:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicAdd(");
            bool needCloseTypeCast = false;
            if (inst->getDataType()->getOp() == kIROp_Int64Type)
            {
                m_writer->emit("(unsigned long long*)(");
                needCloseTypeCast = true;
            }
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            if (needCloseTypeCast)
            {
                m_writer->emit(")");
            }
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_AtomicSub:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicAdd(");
            bool needCloseTypeCast = false;
            if (inst->getDataType()->getOp() == kIROp_Int64Type)
            {
                m_writer->emit("(unsigned long long*)(");
                needCloseTypeCast = true;
            }
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            if (needCloseTypeCast)
            {
                m_writer->emit(")");
            }
            m_writer->emit(", -(");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit("));\n");
            return true;
        }
    case kIROp_AtomicAnd:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicAnd(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_AtomicOr:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicOr(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_AtomicXor:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicXor(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_AtomicMin:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicMin(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_AtomicMax:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicMax(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_AtomicInc:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicAdd(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", 1);\n");
            return true;
        }
    case kIROp_AtomicDec:
        {
            emitInstResultDecl(inst);
            m_writer->emit("atomicAdd(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", -1);\n");
            return true;
        }
    case kIROp_CoopVecMatMulAdd:
        {
            if (!isOptixCoopVec)
            {
                getSink()->diagnose(Diagnostics::UnsupportedTargetIntrinsic{
                    .operation = "cooperative vector matrix multiply-add",
                    .location = inst->sourceLoc});
                _emitInstAsDefaultInitializedVar(inst, inst->getDataType());
                return true;
            }

            emitInstResultDecl(inst);
            emitInstExpr(inst, getInfo(EmitOp::General));
            m_writer->emit(";\n");
            return true;
        }
    case kIROp_CoopMatMulAdd:
        {
            emitInstResultDecl(inst);
            emitInstExpr(inst, getInfo(EmitOp::General));
            m_writer->emit(";\n");
            return true;
        }
    case kIROp_CoopVecOuterProductAccumulate:
        {
            if (!isOptixCoopVec)
            {
                getSink()->diagnose(Diagnostics::UnsupportedTargetIntrinsic{
                    .operation = "cooperative vector outer-product accumulate",
                    .location = inst->sourceLoc});
                m_writer->emit("/* unsupported cooperative vector outer-product accumulate */\n");
                return true;
            }

            auto outerProduct = cast<IRCoopVecOuterProductAccumulate>(inst);
            auto matrixLayout = cast<IRIntLit>(outerProduct->getMemoryLayout())->getValue();
            auto matrixInterpretation =
                cast<IRIntLit>(outerProduct->getMatrixInterpretation())->getValue();

            if (matrixLayout != SLANG_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL)
            {
                getSink()->diagnose(Diagnostics::UnsupportedTargetIntrinsic{
                    .operation =
                        "cooperative vector outer-product accumulate requires TrainingOptimal "
                        "matrix layout for OptiX",
                    .location = inst->sourceLoc});
                m_writer->emit("/* unsupported cooperative vector outer-product accumulate */\n");
                return true;
            }

            if (matrixInterpretation != SLANG_SCALAR_TYPE_FLOAT16)
            {
                getSink()->diagnose(Diagnostics::UnsupportedTargetIntrinsic{
                    .operation =
                        "cooperative vector outer-product accumulate requires Float16 matrix "
                        "interpretation for OptiX",
                    .location = inst->sourceLoc});
                m_writer->emit("/* unsupported cooperative vector outer-product accumulate */\n");
                return true;
            }

            m_writer->emit("optixCoopVecOuterProductAccumulate(");
            emitOperand(outerProduct->getA(), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(outerProduct->getB(), getInfo(EmitOp::General));
            m_writer->emit(", (CUdeviceptr)(&(");
            emitOperand(outerProduct->getMatrixPtr(), getInfo(EmitOp::General));
            m_writer->emit(")), ");
            emitOperand(outerProduct->getMatrixOffset(), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(outerProduct->getMatrixStride(), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_CoopVecReduceSumAccumulate:
        {
            if (!isOptixCoopVec)
            {
                getSink()->diagnose(Diagnostics::UnsupportedTargetIntrinsic{
                    .operation = "cooperative vector reduce-sum accumulate",
                    .location = inst->sourceLoc});
                m_writer->emit("/* unsupported cooperative vector reduce-sum accumulate */\n");
                return true;
            }

            auto reduceSum = cast<IRCoopVecReduceSumAccumulate>(inst);
            auto valueType = as<IRCoopVectorType>(reduceSum->getValue()->getDataType());
            SLANG_ASSERT(valueType);
            auto valueElementType = as<IRBasicType>(valueType->getElementType());
            SLANG_ASSERT(valueElementType);
            if (valueElementType->getBaseType() != BaseType::Half &&
                valueElementType->getBaseType() != BaseType::Float)
            {
                getSink()->diagnose(Diagnostics::UnsupportedTargetIntrinsic{
                    .operation =
                        "cooperative vector reduce-sum accumulate requires Float16 or Float32 "
                        "vector element type for OptiX",
                    .location = inst->sourceLoc});
                m_writer->emit("/* unsupported cooperative vector reduce-sum accumulate */\n");
                return true;
            }

            m_writer->emit("optixCoopVecReduceSumAccumulate(");
            emitOperand(reduceSum->getValue(), getInfo(EmitOp::General));
            m_writer->emit(", (CUdeviceptr)(&(");
            emitOperand(reduceSum->getBufferPtr(), getInfo(EmitOp::General));
            m_writer->emit(")), ");
            emitOperand(reduceSum->getOffset(), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    case kIROp_SetOptiXPayloadRegister:
        {
            auto idxInst = as<IRIntLit>(inst->getOperand(0));
            IRIntegerValue idx = idxInst->getValue();
            m_writer->emit("optixSetPayload_");
            m_writer->emit(idx);
            m_writer->emit("(");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(");\n");
            return true;
        }
    default:
        return false;
    }
}

bool CUDASourceEmitter::tryEmitInstExprImpl(IRInst* inst, const EmitOpInfo& inOuterPrec)
{
    switch (inst->getOp())
    {
    case kIROp_MakeVector:
    case kIROp_MakeVectorFromScalar:
        {
            m_writer->emit("make_");
            emitType(inst->getDataType());
            m_writer->emit("(");
            bool isFirst = true;
            char xyzwNames[] = "xyzw";
            for (UInt i = 0; i < inst->getOperandCount(); i++)
            {
                auto arg = inst->getOperand(i);
                if (auto vectorType = as<IRVectorType>(arg->getDataType()))
                {
                    for (int j = 0; j < cast<IRIntLit>(vectorType->getElementCount())->getValue();
                         j++)
                    {
                        if (isFirst)
                            isFirst = false;
                        else
                            m_writer->emit(", ");
                        auto outerPrec = getInfo(EmitOp::General);
                        auto prec = getInfo(EmitOp::Postfix);
                        emitOperand(arg, leftSide(outerPrec, prec));
                        m_writer->emit(".");
                        m_writer->emitChar(xyzwNames[j]);
                    }
                }
                else
                {
                    if (isFirst)
                        isFirst = false;
                    else
                        m_writer->emit(", ");
                    emitOperand(arg, getInfo(EmitOp::General));
                }
            }
            m_writer->emit(")");
            return true;
        }
    case kIROp_FloatCast:
    case kIROp_CastIntToFloat:
    case kIROp_IntCast:
    case kIROp_CastFloatToInt:
        {
            if (auto dstVectorType = as<IRVectorType>(inst->getDataType()))
            {
                m_writer->emit("make_");
                emitType(inst->getDataType());
                m_writer->emit("(");
                bool isFirst = true;
                char xyzwNames[] = "xyzw";
                for (UInt i = 0; i < inst->getOperandCount(); i++)
                {
                    auto arg = inst->getOperand(i);
                    if (auto vectorType = as<IRVectorType>(arg->getDataType()))
                    {
                        for (int j = 0;
                             j < cast<IRIntLit>(vectorType->getElementCount())->getValue();
                             j++)
                        {
                            if (isFirst)
                                isFirst = false;
                            else
                                m_writer->emit(", ");
                            m_writer->emit("(");
                            emitType(dstVectorType->getElementType());
                            m_writer->emit(")");
                            auto outerPrec = getInfo(EmitOp::General);
                            auto prec = getInfo(EmitOp::Postfix);
                            emitOperand(arg, leftSide(outerPrec, prec));
                            m_writer->emit(".");
                            m_writer->emitChar(xyzwNames[j]);
                        }
                    }
                    else
                    {
                        if (isFirst)
                            isFirst = false;
                        else
                            m_writer->emit(", ");
                        m_writer->emit("(");
                        emitType(dstVectorType->getElementType());
                        m_writer->emit(")");
                        emitOperand(arg, getInfo(EmitOp::General));
                    }
                }
                m_writer->emit(")");
                return true;
            }
            else if (const auto matrixType = as<IRMatrixType>(inst->getDataType()); matrixType)
            {
                m_writer->emit("make");
                emitType(inst->getDataType());
                m_writer->emit("(");
                for (UInt i = 0; i < inst->getOperandCount(); i++)
                {
                    auto arg = inst->getOperand(i);
                    if (i > 0)
                        m_writer->emit(", ");
                    emitOperand(arg, getInfo(EmitOp::General));
                }
                m_writer->emit(")");
                return true;
            }
            return false;
        }
    case kIROp_MakeMatrix:
    case kIROp_MakeMatrixFromScalar:
    case kIROp_MatrixReshape:
        {
            m_writer->emit("make");
            emitType(inst->getDataType());
            m_writer->emit("(");
            for (UInt i = 0; i < inst->getOperandCount(); i++)
            {
                auto arg = inst->getOperand(i);
                if (i > 0)
                    m_writer->emit(", ");
                emitOperand(arg, getInfo(EmitOp::General));
            }
            m_writer->emit(")");
            return true;
        }
    case kIROp_MakeCoopMatrixFromScalar:
        {
            StringBuilder typeSB;
            emitWMMAFragmentType(as<IRCoopMatrixType>(inst->getDataType()), typeSB);
            m_writer->emit(typeSB);
            m_writer->emit("(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(")");
            return true;
        }
    case kIROp_CoopMatMulAdd:
        {
            auto coopMatMulAdd = cast<IRCoopMatMulAdd>(inst);
            auto matA = coopMatMulAdd->getMatA();
            auto matB = coopMatMulAdd->getMatB();
            auto matC = coopMatMulAdd->getMatC();
            auto saturatingAccumulation =
                cast<IRBoolLit>(coopMatMulAdd->getSaturatingAccumulation())->getValue();

            auto aElemType = cast<IRCoopMatrixType>(matA->getDataType())->getElementType();
            auto bElemType = cast<IRCoopMatrixType>(matB->getDataType())->getElementType();
            auto cElemType = cast<IRCoopMatrixType>(matC->getDataType())->getElementType();
            auto dElemType = cast<IRCoopMatrixType>(coopMatMulAdd->getDataType())->getElementType();
            if (!coopMatMulAddTypeCombinationIsValid(
                    aElemType->getOp(),
                    bElemType->getOp(),
                    cElemType->getOp(),
                    dElemType->getOp()))
            {
                auto formatElem = [&](IRType* type) -> String
                {
                    StringBuilder sb;
                    calcTypeName(type, CodeGenTarget::CUDASource, sb);
                    return sb.toString();
                };
                getSink()->diagnose(Diagnostics::CooperativeMatrixInvalidMmaTypeCombination{
                    .aType = formatElem(aElemType),
                    .bType = formatElem(bElemType),
                    .cType = formatElem(cElemType),
                    .dType = formatElem(dElemType),
                    .location = inst->sourceLoc});
                // The DiagnosticSink has already recorded the error, but the
                // surrounding statement-emit path expects an expression to
                // follow `Type _Sname = ` (otherwise we'd emit the syntactically
                // invalid `Type _Sname = ;`).  Emit a default-constructed
                // value of the result type as a placeholder; the recorded
                // error makes the overall compile fail anyway, so the
                // placeholder never reaches NVRTC.
                m_writer->emit("(");
                emitType(inst->getDataType());
                m_writer->emit("{})");
                return true;
            }

            m_writer->emit("Slang_CUDA_WMMA::coopMatMulAdd<");
            emitType(matA->getDataType());
            m_writer->emit("::ElementType, ");
            emitType(matB->getDataType());
            m_writer->emit("::ElementType, ");
            emitType(matC->getDataType());
            m_writer->emit("::ElementType, ");
            emitType(coopMatMulAdd->getDataType());
            m_writer->emit("::ElementType, ");
            emitType(matA->getDataType());
            m_writer->emit("::m_M, ");
            emitType(matA->getDataType());
            m_writer->emit("::m_N, ");
            emitType(matA->getDataType());
            m_writer->emit("::m_K, ");
            m_writer->emit(saturatingAccumulation ? "true" : "false");
            m_writer->emit(">(");
            emitOperand(matA, getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(matB, getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(matC, getInfo(EmitOp::General));
            m_writer->emit(")");
            return true;
        }
    case kIROp_CoopVecMatMulAdd:
        {
            // CoopVec matmul ops are always emitted as statements, so non-OptiX handling lives in
            // tryEmitInstStmtImpl().
            SLANG_ASSERT(isOptixCoopVec);

            auto coopVecMatMulAdd = cast<IRCoopVecMatMulAdd>(inst);
            auto inputInterpretationPackingFactor =
                cast<IRIntLit>(coopVecMatMulAdd->getInputInterpretationPackingFactor())->getValue();
            auto inputInterpretation =
                cast<IRIntLit>(coopVecMatMulAdd->getInputInterpretation())->getValue();
            auto matrixInterpretation =
                cast<IRIntLit>(coopVecMatMulAdd->getMatrixInterpretation())->getValue();
            auto biasInterpretation = coopVecMatMulAdd->getBiasInterpretation();
            const bool hasBias = biasInterpretation != nullptr;

            if (inputInterpretationPackingFactor != 1)
            {
                emitUnsupportedTargetIntrinsicExpr(
                    this,
                    inst,
                    "cooperative vector matrix multiply-add with packed input is not implemented "
                    "yet",
                    inst->sourceLoc);
                return true;
            }

            auto inputInterpretationName =
                getOptixCoopVecComponentTypeName((uint32_t)inputInterpretation);
            if (!inputInterpretationName.getLength())
            {
                emitUnsupportedTargetIntrinsicExpr(
                    this,
                    inst,
                    "cooperative vector matrix multiply-add with unsupported OptiX input "
                    "interpretation type",
                    inst->sourceLoc);
                return true;
            }

            auto matrixInterpretationName =
                getOptixCoopVecComponentTypeName((uint32_t)matrixInterpretation);
            if (!matrixInterpretationName.getLength())
            {
                emitUnsupportedTargetIntrinsicExpr(
                    this,
                    inst,
                    "cooperative vector matrix multiply-add with unsupported OptiX matrix "
                    "interpretation type",
                    inst->sourceLoc);
                return true;
            }

            auto matrixLayout = cast<IRIntLit>(coopVecMatMulAdd->getMemoryLayout())->getValue();
            auto matrixLayoutName = getOptixCoopVecMatrixLayoutName((uint32_t)matrixLayout);

            auto transposeValue = cast<IRBoolLit>(coopVecMatMulAdd->getTranspose())->getValue();
            if (transposeValue)
            {
                if (matrixInterpretation != SLANG_SCALAR_TYPE_FLOAT16 ||
                    (matrixLayout != SLANG_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL &&
                     matrixLayout != SLANG_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL))
                {
                    emitUnsupportedTargetIntrinsicExpr(
                        this,
                        inst,
                        "cooperative vector matrix multiply-add with transpose requires Float16 "
                        "matrix interpretation and InferencingOptimal or TrainingOptimal matrix "
                        "layout for OptiX",
                        inst->sourceLoc);
                    return true;
                }
            }

            UnownedStringSlice biasInterpretationName;
            if (hasBias)
            {
                biasInterpretationName = getOptixCoopVecComponentTypeName(
                    (uint32_t)cast<IRIntLit>(biasInterpretation)->getValue());
                if (!biasInterpretationName.getLength())
                {
                    emitUnsupportedTargetIntrinsicExpr(
                        this,
                        inst,
                        "cooperative vector matrix multiply-add with unsupported OptiX bias "
                        "interpretation type",
                        inst->sourceLoc);
                    return true;
                }
            }

            m_writer->emit("(");
            m_writer->emit("slangOptixCoopVecMatMul<");
            emitType(inst->getDataType());
            m_writer->emit(", ");
            emitType(coopVecMatMulAdd->getInput()->getDataType());
            m_writer->emit(", ");
            m_writer->emit(inputInterpretationName);
            m_writer->emit(", ");
            m_writer->emit(matrixInterpretationName);
            m_writer->emit(", ");
            m_writer->emit(matrixLayoutName);
            if (hasBias)
            {
                m_writer->emit(", ");
                m_writer->emit(biasInterpretationName);
            }
            m_writer->emit(">((");
            emitOperand(coopVecMatMulAdd->getInput(), getInfo(EmitOp::General));
            m_writer->emit("), (CUdeviceptr)(&((");
            emitOperand(coopVecMatMulAdd->getMatrixPtr(), getInfo(EmitOp::General));
            m_writer->emit("))), ");
            emitOperand(coopVecMatMulAdd->getMatrixOffset(), getInfo(EmitOp::General));
            if (hasBias)
            {
                m_writer->emit(", (CUdeviceptr)(&((");
                emitOperand(coopVecMatMulAdd->getBiasPtr(), getInfo(EmitOp::General));
                m_writer->emit("))), ");
                emitOperand(coopVecMatMulAdd->getBiasOffset(), getInfo(EmitOp::General));
            }
            else if (
                as<IRHLSLStructuredBufferTypeBase>(
                    coopVecMatMulAdd->getMatrixPtr()->getDataType()) == nullptr)
            {
                m_writer->emit(", ");
                emitOperand(coopVecMatMulAdd->getTranspose(), getInfo(EmitOp::General));
            }
            m_writer->emit(", ");
            emitOperand(coopVecMatMulAdd->getMatrixStride(), getInfo(EmitOp::General));
            m_writer->emit("))");
            return true;
        }
    case kIROp_MakeArray:
        {
            IRType* dataType = inst->getDataType();
            IRArrayType* arrayType = as<IRArrayType>(dataType);

            IRType* elementType = arrayType->getElementType();

            // Emit braces for the FixedArray struct.

            _emitInitializerList(elementType, inst->getOperands(), Index(inst->getOperandCount()));

            return true;
        }
    case kIROp_WaveMaskBallot:
        {
            m_extensionTracker->requireSMVersion(SemanticVersion(7, 0));

            m_writer->emit("__ballot_sync(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(")");
            return true;
        }
    case kIROp_WaveMaskMatch:
        {
            m_extensionTracker->requireSMVersion(SemanticVersion(7, 0));

            m_writer->emit("__match_any_sync(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(inst->getOperand(1), getInfo(EmitOp::General));
            m_writer->emit(")");
            return true;
        }
    case kIROp_GetOptiXRayPayloadPtr:
        {
            m_writer->emit("((");
            emitType(inst->getDataType());
            m_writer->emit(")getOptiXRayPayloadPtr())");
            return true;
        }
    case kIROp_GetOptiXHitAttribute:
        {
            auto typeToFetch = inst->getOperand(0);
            auto idxInst = as<IRIntLit>(inst->getOperand(1));
            IRIntegerValue idx = idxInst->getValue();
            if (typeToFetch->getOp() == kIROp_FloatType)
            {
                m_writer->emit("__int_as_float(optixGetAttribute_");
            }
            else
            {
                m_writer->emit("optixGetAttribute_");
            }
            m_writer->emit(idx);
            if (typeToFetch->getOp() == kIROp_FloatType)
            {
                m_writer->emit("())");
            }
            else
            {
                m_writer->emit("()");
            }
            return true;
        }
    case kIROp_GetOptiXSbtDataPtr:
        {
            m_writer->emit("((");
            emitType(inst->getDataType());
            m_writer->emit(")optixGetSbtDataPointer())");
            return true;
        }
    case kIROp_GetOptiXPayloadRegister:
        {
            auto idxInst = as<IRIntLit>(inst->getOperand(0));
            IRIntegerValue idx = idxInst->getValue();
            m_writer->emit("optixGetPayload_");
            m_writer->emit(idx);
            m_writer->emit("()");
            return true;
        }
    case kIROp_DispatchKernel:
        {
            auto dispatchInst = as<IRDispatchKernel>(inst);
            emitOperand(dispatchInst->getBaseFn(), getInfo(EmitOp::Atomic));
            m_writer->emit("<<<");
            emitOperand(dispatchInst->getThreadGroupSize(), getInfo(EmitOp::General));
            m_writer->emit(", ");
            emitOperand(dispatchInst->getDispatchSize(), getInfo(EmitOp::General));
            m_writer->emit(">>>(");
            for (UInt i = 0; i < dispatchInst->getArgCount(); i++)
            {
                if (i > 0)
                    m_writer->emit(", ");
                emitOperand(dispatchInst->getArg(i), getInfo(EmitOp::General));
            }
            m_writer->emit(")");
            return true;
        }
    case kIROp_CUDALDG:
        {
            m_writer->emit("__ldg(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(")");
        }
        return true;
    case kIROp_GetStructuredBufferPtr:
    case kIROp_GetUntypedBufferPtr:
        {
            m_writer->emit("(&(");
            emitOperand(inst->getOperand(0), getInfo(EmitOp::General));
            m_writer->emit(").data)");
            return true;
        }
    default:
        break;
    }

    return Super::tryEmitInstExprImpl(inst, inOuterPrec);
}

void CUDASourceEmitter::handleRequiredCapabilitiesImpl(IRInst* inst)
{
    // Does this function declare any requirements on CUDA capabilities
    // that should affect output?

    for (auto decoration : inst->getDecorations())
    {
        if (auto smDecoration = as<IRRequireCUDASMVersionDecoration>(decoration))
        {
            SemanticVersion version = smDecoration->getCUDASMVersion();
            m_extensionTracker->requireSMVersion(version);
        }
    }
}

void CUDASourceEmitter::emitVectorTypeNameImpl(IRType* elementType, IRIntegerValue elementCount)
{
    m_writer->emit(getVectorPrefix(elementType->getOp()));
    m_writer->emit(elementCount);
}

void CUDASourceEmitter::_emitType(IRType* type, DeclaratorInfo* declarator)
{
    // Handle Ptr<T[]> on CUDA, we shouldn't emit it as Array<T>*.
    // Instead, we should emit it as T**.
    // e.g. Array<T>* a;    a[1] == a + sizeof(Array<T>)
    // but T** b;    b[1] == b + sizeof(T*)
    if (type->getOp() == kIROp_PtrType || type->getOp() == kIROp_NativePtrType)
    {
        auto ptrType = cast<IRPtrTypeBase>(type);
        if (auto unsizedArrayType = as<IRUnsizedArrayType>(ptrType->getValueType()))
        {
            PtrDeclaratorInfo outerPtr(declarator);
            PtrDeclaratorInfo innerPtr(&outerPtr);
            _emitType(unsizedArrayType->getElementType(), &innerPtr);
            return;
        }
    }
    Super::_emitType(type, declarator);
}

void CUDASourceEmitter::emitSimpleTypeImpl(IRType* type)
{
    switch (type->getOp())
    {
    case kIROp_VectorType:
        {
            auto vectorType = as<IRVectorType>(type);
            m_writer->emit(getVectorPrefix(vectorType->getElementType()->getOp()));
            m_writer->emit(as<IRIntLit>(vectorType->getElementCount())->getValue());
            break;
        }
    default:
        m_writer->emit(_getTypeName(type));
        break;
    }
}

void CUDASourceEmitter::emitRateQualifiersAndAddressSpaceImpl(
    IRRate* rate,
    [[maybe_unused]] AddressSpace addressSpace)
{
    if (as<IRGroupSharedRate>(rate))
    {
        m_writer->emit("__shared__ ");
    }
}

void CUDASourceEmitter::emitSimpleFuncParamsImpl(IRFunc* func)
{
    m_writer->emit("(");

    bool hasEmittedParam = false;
    auto firstParam = func->getFirstParam();
    for (auto pp = firstParam; pp; pp = pp->getNextParam())
    {
        auto varLayout = getVarLayout(pp);
        if (varLayout && varLayout->findSystemValueSemanticAttr())
        {
            // If it has a semantic don't output, it will be accessed via a global
            continue;
        }

        if (hasEmittedParam)
            m_writer->emit(", ");

        emitSimpleFuncParamImpl(pp);
        hasEmittedParam = true;
    }

    m_writer->emit(")");
}

void CUDASourceEmitter::emitSimpleFuncImpl(IRFunc* func)
{
    // Skip the CPP impl - as it does some processing we don't need here for entry points.
    CLikeSourceEmitter::emitSimpleFuncImpl(func);
}

void CUDASourceEmitter::emitSemanticsImpl(IRInst* inst, bool allowOffsetLayout)
{
    Super::emitSemanticsImpl(inst, allowOffsetLayout);
}

void CUDASourceEmitter::emitInterpolationModifiersImpl(
    IRInst* varInst,
    IRType* valueType,
    IRVarLayout* layout)
{
    Super::emitInterpolationModifiersImpl(varInst, valueType, layout);
}

void CUDASourceEmitter::emitVarDecorationsImpl(IRInst* varDecl)
{
    Super::emitVarDecorationsImpl(varDecl);
}

void CUDASourceEmitter::emitMatrixLayoutModifiersImpl(IRType* varType)
{
    Super::emitMatrixLayoutModifiersImpl(varType);
}

bool CUDASourceEmitter::tryEmitGlobalParamImpl(IRGlobalParam* varDecl, IRType* varType)
{
    // A global shader parameter in the IR for CUDA output will
    // either be the unique constant buffer that wraps all the
    // global-scope parameters in the original code (which is
    // handled as a special-case before this routine would be
    // called), or it is one of the system-defined varying inputs
    // like `threadIdx`. We won't need to emit anything in the
    // output code for the latter case, so we need to emit
    // nothing here and return `true` so that the base class
    // uses our logic instead of the default.
    //
    SLANG_UNUSED(varDecl);
    SLANG_UNUSED(varType);
    return true;
}


void CUDASourceEmitter::emitModuleImpl(IRModule* module, DiagnosticSink* sink)
{
    // Set up with all of the base types used in the module
    m_extensionTracker->requireBaseTypes(_findBaseTypesUsed(module));

    CLikeSourceEmitter::emitModuleImpl(module, sink);

    // Emit all witness table definitions.
    _emitWitnessTableDefinitions();
}

static bool typeCheck(IROp op, uint32_t matrixUse)
{
    switch (matrixUse)
    {
    case SLANG_COOPERATIVE_MATRIX_USE_A:
    case SLANG_COOPERATIVE_MATRIX_USE_B:
        // PTX m16n8k16 supports f16, bf16, 8-bit integer (s8 / u8), and 8-bit
        // float (e4m3 / e5m2) inputs.
        return op == kIROp_HalfType || op == kIROp_BFloat16Type || op == kIROp_Int8Type ||
               op == kIROp_UInt8Type || op == kIROp_FloatE4M3Type || op == kIROp_FloatE5M2Type;
    case SLANG_COOPERATIVE_MATRIX_USE_ACCUMULATOR:
        // Union of the legal accumulator element types across all
        // currently-supported A/B element types: half/float (for f16, bf16, f8
        // inputs) and int (for s8 / u8 inputs).  The full A/B/C/D combination
        // is checked separately by `coopMatMulAddTypeCombinationIsValid`
        // before code emission.
        return op == kIROp_HalfType || op == kIROp_FloatType || op == kIROp_IntType;
    }
    return false;
}

// Validate that a `coopMatMulAdd` (A * B + C -> D) is one of the legal
// (AType, BType, CType, DType) tuples for the CUDA backend.  The helper
// templates in `prelude/slang-cuda-prelude.h` only have specializations for
// these tuples; without this check, an illegal combination would compile
// through Slang and only fail later inside NVRTC with a hard-to-read C++
// template error.
static bool coopMatMulAddTypeCombinationIsValid(IROp aType, IROp bType, IROp cType, IROp dType)
{
    // Both A and B must share the same element type — every supported
    // CUDA mma form has matching `.atype` and `.btype`.
    if (aType != bType)
        return false;

    auto isHalfOrFloat = [](IROp t) { return t == kIROp_HalfType || t == kIROp_FloatType; };
    auto isFloat = [](IROp t) { return t == kIROp_FloatType; };
    auto isInt32 = [](IROp t) { return t == kIROp_IntType; };

    switch (aType)
    {
    case kIROp_HalfType:
        // f16 mma supports both f16 and f32 accumulator/output, and CType
        // and DType may be picked independently from {half, float}.
        return isHalfOrFloat(cType) && isHalfOrFloat(dType);
    case kIROp_BFloat16Type:
        // bf16 mma only allows an f32 accumulator and output on PTX.
        return isFloat(cType) && isFloat(dType);
    case kIROp_Int8Type:
    case kIROp_UInt8Type:
        // Integer mma only allows an s32 accumulator and output.
        return isInt32(cType) && isInt32(dType);
    case kIROp_FloatE4M3Type:
    case kIROp_FloatE5M2Type:
        // fp8 mma supports half or float accumulator/output; the prelude only
        // provides specializations where CType == DType for these.
        return (cType == kIROp_HalfType && dType == kIROp_HalfType) ||
               (cType == kIROp_FloatType && dType == kIROp_FloatType);
    default:
        return false;
    }
}

static UnownedStringSlice getMatrixUseName(uint32_t matrixUse)
{
    switch (matrixUse)
    {
    case SLANG_COOPERATIVE_MATRIX_USE_A:
        return UnownedStringSlice("Slang_CUDA_WMMA::MatrixA");
    case SLANG_COOPERATIVE_MATRIX_USE_B:
        return UnownedStringSlice("Slang_CUDA_WMMA::MatrixB");
    case SLANG_COOPERATIVE_MATRIX_USE_ACCUMULATOR:
        return UnownedStringSlice("Slang_CUDA_WMMA::MatrixC");
    default:
        SLANG_UNEXPECTED("invalid cooperative matrix use");
    }
}

/*
 * Shape Validation Strategy:
 * Maps CoopMat dimensions to the canonical MMA shape (m, n, k).
 * Only m16n16k16 is supported (internally uses 2x mma.sync.m16n8k16).
 *
 * Supported shapes:
 *   - m16n16k16: Matrix A (16x16), Matrix B (16x16), Matrix C/D (16x16)
 */
inline FragmentShape computeShapeCombination(uint32_t /*matrixUse*/, uint32_t row, uint32_t col)
{
    if (row == 16 && col == 16)
        return {16, 16, 16};
    return {0, 0, 0};
}

SlangResult CUDASourceEmitter::emitWMMAFragmentType(
    IRCoopMatrixType* coopMatType,
    StringBuilder& outStr)
{
    uint32_t rowCount = (uint32_t) static_cast<IRIntLit*>(coopMatType->getRowCount())->getValue();
    uint32_t colCount =
        (uint32_t) static_cast<IRIntLit*>(coopMatType->getColumnCount())->getValue();
    uint32_t matrixUse = (uint32_t) static_cast<IRIntLit*>(coopMatType->getMatrixUse())->getValue();

    auto elementType = coopMatType->getElementType();
    StringBuilder elementTypeSB;
    calcTypeName(elementType, CodeGenTarget::CUDASource, elementTypeSB);
    auto typeName = elementTypeSB.toString();

    // TODO: We should add a pass in IR to validate the coop matrix types, such that
    // we can provide better diagnostic messages here.
    if (!typeCheck(elementType->getOp(), matrixUse))
    {
        getSink()->diagnose(Diagnostics::CooperativeMatrixUnsupportedElementType{
            .elementType = typeName,
            .matrixUse = matrixUse == SLANG_COOPERATIVE_MATRIX_USE_A
                             ? "A"
                             : (matrixUse == SLANG_COOPERATIVE_MATRIX_USE_B ? "B" : "C")});
        SLANG_RELEASE_ASSERT(false);
        return SLANG_FAIL;
    }

    outStr << "Slang_CUDA_WMMA::WmmaFragment<";

    FragmentShape shape = computeShapeCombination(matrixUse, rowCount, colCount);
    if (!shape.isValid())
    {
        getSink()->diagnose(Diagnostics::CooperativeMatrixInvalidShape{
            .rowCount = String(rowCount),
            .colCount = String(colCount),
            .matrixUse = matrixUse == SLANG_COOPERATIVE_MATRIX_USE_A
                             ? "A"
                             : (matrixUse == SLANG_COOPERATIVE_MATRIX_USE_B ? "B" : "C")});
        SLANG_RELEASE_ASSERT(false);
        return SLANG_FAIL;
    }

    outStr << typeName << "," << shape.m << ", " << shape.n << ", " << shape.k << ", "
           << getMatrixUseName(matrixUse) << ">";

    return SLANG_OK;
}

} // namespace Slang
