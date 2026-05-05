# Plan: CUDA Surface Format Conversion via Inline Codegen

## TL;DR

Replace the C++ template metaprogramming approach in `slang-cuda-prelude.h` with inline conversion code emitted directly by the CUDA source emitter. Since the image format is known at compile time (from `IRFormatDecoration`), the emitter can generate the exact storage-type surface read/write call plus the conversion math inline, eliminating ~400 lines of prelude boilerplate and supporting all formats uniformly.

## Approach

Instead of emitting `surf2Dread_convert<float4, _slang_image_format_rgba8>(...)` and relying on template specializations in the prelude to do the conversion, the compiler will emit something like:

```cuda
// Read: RWTexture2D<float4> with format rgba8 (unorm)
([&]() {
    uchar4 _storage = surf2Dread<uchar4>(surfObj, x * 4, y, SLANG_CUDA_BOUNDARY_MODE);
    return make_float4(_storage.x / 255.0f, _storage.y / 255.0f, _storage.z / 255.0f, _storage.w / 255.0f);
})()

// Write: RWTexture2D<float4> with format rgba8 (unorm)  
{
    float4 _val = newValue;
    uchar4 _storage = make_uchar4(
        (uchar)(__saturatef(_val.x) * 255.0f + 0.5f),
        (uchar)(__saturatef(_val.y) * 255.0f + 0.5f),
        (uchar)(__saturatef(_val.z) * 255.0f + 0.5f),
        (uchar)(__saturatef(_val.w) * 255.0f + 0.5f));
    surf2Dwrite<uchar4>(_storage, surfObj, x * 4, y, SLANG_CUDA_BOUNDARY_MODE);
}
```

## Steps

### Phase 1: Add helper emission functions in CUDA emitter

1. **Add a format conversion helper to `CUDASourceEmitter`** (`source/slang/slang-emit-cuda.h`, `source/slang/slang-emit-cuda.cpp`)
   - Add methods that, given an `ImageFormat` and target element type (`float`/`float2`/`float4`/`int`/`uint`/etc.), emit:
     - The storage type name (e.g., `uchar4`, `ushort2`)
     - Per-component unpack code (storage→element: unorm→float, snorm→float, half→float, uint→uint, int→int)
     - Per-component pack code (element→storage: float→unorm, float→snorm, float→half, uint→uint, int→int)
   - Use `getImageFormatInfo()` to get `scalarType`, `channelCount`, `sizeInBytes` — all needed info is already there.

2. **Map `ImageFormat` scalar types to CUDA type names** — a simple lookup table:
   - `SLANG_SCALAR_TYPE_UINT8` → `uchar`
   - `SLANG_SCALAR_TYPE_INT8` → `char`  
   - `SLANG_SCALAR_TYPE_UINT16` → `ushort`
   - `SLANG_SCALAR_TYPE_INT16` → `short`
   - `SLANG_SCALAR_TYPE_FLOAT16` → `ushort` (stored as ushort, converted via `__ushort_as_half`)
   - `SLANG_SCALAR_TYPE_UINT32` → `uint`
   - `SLANG_SCALAR_TYPE_INT32` → `int`
   - `SLANG_SCALAR_TYPE_FLOAT32` → `float`

3. **Determine format "kind"** from scalar type + format name:
   - Float formats (`r16f`, `rg16f`, `rgba16f`): scalar is FLOAT16 → half-as-ushort conversion
   - Unorm formats (`r8`, `rg8`, `rgba8`, `r16`, `rg16`, `rgba16`): scalar is UINT8/UINT16 + name has no suffix → unorm conversion
   - Snorm formats (`*_snorm`): scalar is UINT8/UINT16 + name ends in `_snorm` → snorm conversion  
   - Uint formats (`*ui`): direct cast/widen
   - Sint formats (`*i`): direct cast/widen
   - 32-bit float: no conversion needed (caught by `_isImageFormatCompatible`)

   Note: The format kind (unorm/snorm/float/uint/sint) isn't currently stored in `ImageFormatInfo`. We need to either:
   - (a) Derive it from the format name + scalar type (fragile but no struct changes), or
   - (b) Add a `formatKind` enum to `ImageFormatInfo` and extend `slang-image-format-defs.h` (clean but more invasive)
   - **Recommendation**: Option (b) — add a kind enum. It's a small change to the defs file and makes the logic robust.

### Phase 2: Modify intrinsic expansion to emit inline conversion

4. **Replace `$C` behavior for reads** — Instead of appending `_convert` to the function name, emit a complete inline expression:
   - When `_isConvertRequired()` is true and it's a read:
     - Emit a lambda or comma-expression that:
       1. Calls the raw `surf*read<StorageType>(...)` with correct byte-addressed x
       2. Converts each component from storage to the target element type
   - When no conversion needed: emit the plain `surf*read<T>(...)` as before.

5. **Replace `$C` behavior for writes** — Instead of calling `surf*write_convert`, emit:
   - Pack the source value into the storage type
   - Call `surf*write<StorageType>(packed, ...)` with correct byte-addressed x

6. **Remove `$f` token** — No longer needed since the format struct name won't be emitted.

7. **Fix `$E` token** — The x-coordinate byte scaling needs to use storage type size when converting. Currently `$E` is set to 1 when converting (to avoid double-scaling since the prelude templates did it internally). With inline codegen, `$E` should emit the storage type's `sizeInBytes` from `getImageFormatInfo()` regardless.

### Phase 3: Implementation approach choice

There are two sub-approaches for where to put the inline emission logic:

**Option A1: Keep it in `_emitSpecial` (intrinsic expand)**
- Modify the `$C` handler to emit the full inline conversion instead of just `_convert`
- Pros: Minimal architectural change, stays in existing mechanism
- Cons: The `_emitSpecial` function handles single tokens; emitting a multi-line lambda is awkward

**Option A2: Override `emitIntrinsicCallExprImpl` in `CUDASourceEmitter`**
- Intercept surface read/write intrinsic calls before they go through the generic expand path
- When conversion is needed, emit the full read+convert or pack+write sequence directly
- When no conversion needed, fall through to the parent implementation
- Pros: Clean separation, full control over emitted code, no need for `$C`/`$f`/`$E` hacks
- Cons: Need to detect surface read/write calls and parse the texture type

**Recommendation**: **Option A2** — it's cleaner and avoids stretching the `$` token mechanism beyond its design. The `emitIntrinsicCallExprImpl` override in `CUDASourceEmitter` already exists (line 650 of `slang-emit-cuda.cpp`) and just delegates to `Super`. We can add conversion logic there.

### Phase 4: Clean up prelude

8. **Remove all prelude template conversion code**:
   - Remove `_slang_vector_traits`, `SlangFormatKind`, `SLANG_IMAGE_FORMAT(...)` structs, all `_slang_image_format_converter` specializations, `_slang_image_format_unpack`, `SLANG_SURFACE_READ_CONVERT_IMPL`
   - Remove `SLANG_SURFACE_READ` / `SLANG_SURFACE_WRITE` half-specialization macros (the `__nv_isurf_trait<__half>` etc.) — these were for the old half conversion path
   - Keep `SLANG_SURF*WRITE_CONVERT_IMPL` using `sust.p` PTX only if they're still used for the non-format-conversion half path. Actually, since `sust.p` is removed from PTX ISA 3.0+, these should also be removed.
   - **Actually**: Per user's finding, `sust.p` was removed. So all write_convert PTX code should go too. The new inline codegen replaces all of it.

9. **Remove `$f` from `hlsl.meta.slang`** — Revert the `$f` additions from the intrinsic asm strings.

10. **Clean up `_emitSpecial`** — Remove the `case 'f':` handler. Simplify `case 'C':` if it's no longer needed for CUDA (check if GLSL still uses it — it appears GLSL does not use `$C`).

### Phase 5: Testing

11. **Verify existing test passes**: `tests/compute/half-rw-texture-convert2.slang` (CUDA half-float conversion test)
12. **Add new tests for unorm/snorm/uint/sint formats** — similar structure with `[format("rgba8")]`, `[format("rgba8_snorm")]`, `[format("rgba8ui")]`, `[format("rgba8i")]` etc.
13. **Verify the RHI test can be unblocked** — The `test-texture-view.cpp` in `external/slang-rhi` currently skips CUDA format conversion. After this change, those tests should be enabled.

## Relevant files

- `source/slang/slang-emit-cuda.h` — Add conversion emission method declarations
- `source/slang/slang-emit-cuda.cpp` — Implement conversion emission in `emitIntrinsicCallExprImpl` override (line ~650)
- `source/slang/slang-intrinsic-expand.cpp` — Clean up `$C`/`$f`/`$E` handlers; possibly simplify `$C` to no-op for CUDA if all logic moves to emitter
- `source/slang/hlsl.meta.slang` — Remove `$f` from CUDA surface read asm strings (lines ~4892-4912). May also need to rethink the asm pattern approach for reads/writes
- `prelude/slang-cuda-prelude.h` — Remove ~400 lines of template conversion code, `sust.p` PTX write code
- `source/slang/slang-ast-support-types.h` — Optionally add format kind enum to `ImageFormatInfo` (line ~208)
- `include/slang-image-format-defs.h` — Optionally extend with format kind data
- `tests/compute/half-rw-texture-convert2.slang` — Existing test, must keep passing
- New test files under `tests/compute/` for unorm/snorm/uint/sint formats

## Verification

1. Build with `cmake --build --preset debug --target slangc slang-test`
2. Run `slang-test tests/compute/half-rw-texture-convert2.slang` — existing CUDA half-float test must pass
3. Compile a test shader with `slangc -target cuda` using `[format("rgba8")] RWTexture2D<float4>` and inspect the emitted CUDA code to verify inline conversion is correct
4. Add and run new test cases for representative format combinations (at minimum: rgba8 unorm, rgba8_snorm, rgba8ui, rgba8i, r16f)
5. The GPU-dependent tests (CUDA) will need CI verification since the local environment has no GPU

## Decisions

- **Format kind storage**: Adding a `formatKind` field to `ImageFormatInfo` is preferred over deriving it from names. This is a minor extension to the defs file.
- **Emission site**: Use `emitIntrinsicCallExprImpl` override in `CUDASourceEmitter` rather than extending the `$` token mechanism.
- **PTX `sust.p`**: Confirmed removed from PTX ISA 3.0+. All PTX inline assembly for write conversion will be removed.
- **Packed formats**: `rgb10_a2`, `r11f_g11f_b10f`, `bgra8` are out of scope for the initial implementation (they have `SLANG_SCALAR_TYPE_NONE` and require special bit-packing logic). They can be added later.
- **`$C` token**: Will be kept for CUDA writes in a simplified form or removed entirely if all surface read/write handling moves to the emitter override. Need to check if any non-CUDA target uses `$C` (appears not).

## Further Considerations

1. **Emission approach for reads**: CUDA surface reads return a value, so wrapping in a lambda `([&]() { ... })()` works but is ugly. An alternative is to emit a separate statement before the expression that stores the raw read, then emit only the conversion expression inline. This requires the emitter to know it's in a read context and inject a preceding statement. The `emitIntrinsicCallExprImpl` approach gives us this control naturally.

2. **CUDA RTC compatibility**: The emitted code must work with both nvcc and CUDA RTC (runtime compilation). Lambdas work in both since CUDA 7.0+ supports C++11 device lambdas with `--extended-lambda`. But simpler approaches (temp variable + conversion expression) are safer.

3. **Channel count mismatch**: When `RWTexture2D<float4>` has format `r8` (1 channel), the read should return `float4(val, 0, 0, 0)` and the write should only store `.x`. The `channelCount` from `ImageFormatInfo` tells us this. Need to handle this zero-fill / truncation.
