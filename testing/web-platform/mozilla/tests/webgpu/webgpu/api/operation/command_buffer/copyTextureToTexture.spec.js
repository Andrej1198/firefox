/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `copyTextureToTexture operation tests`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert, ErrorWithExtra, memcpy } from '../../../../common/util/util.js';
import {
  kBufferSizeAlignment,
  kMinDynamicBufferOffsetAlignment,
  kTextureDimensions } from
'../../../capability_info.js';
import {


  depthStencilFormatAspectSize,
  getBaseFormatForTextureFormat,
  getBlockInfoForColorTextureFormat,
  isCompressedTextureFormat,
  isDepthTextureFormat,
  isRegularTextureFormat,
  isStencilTextureFormat,
  kCompressedTextureFormats,
  kDepthStencilFormats,
  kRegularTextureFormats,

  textureFormatAndDimensionPossiblyCompatible,
  textureFormatsAreViewCompatible } from
'../../../format_info.js';
import { AllFeaturesMaxLimitsGPUTest } from '../../../gpu_test.js';
import * as ttu from '../../../texture_test_utils.js';
import { checkElementsEqual } from '../../../util/check_contents.js';
import { align } from '../../../util/math.js';
import { physicalMipSize } from '../../../util/texture/base.js';
import { DataArrayGenerator } from '../../../util/texture/data_generation.js';
import { kBytesPerRowAlignment, dataBytesForCopyOrFail } from '../../../util/texture/layout.js';
import { TexelView } from '../../../util/texture/texel_view.js';
import { findFailedPixels } from '../../../util/texture/texture_ok.js';

const dataGenerator = new DataArrayGenerator();

class F extends AllFeaturesMaxLimitsGPUTest {
  GetInitialDataPerMipLevel(
  dimension,
  textureSize,
  format,
  mipLevel)
  {
    const textureSizeAtLevel = physicalMipSize(textureSize, format, dimension, mipLevel);
    const { bytesPerBlock, blockWidth, blockHeight } = getBlockInfoForColorTextureFormat(format);
    const blocksPerSubresource =
    textureSizeAtLevel.width / blockWidth * (textureSizeAtLevel.height / blockHeight);

    const byteSize = bytesPerBlock * blocksPerSubresource * textureSizeAtLevel.depthOrArrayLayers;
    return dataGenerator.generateView(byteSize);
  }

  GetInitialStencilDataPerMipLevel(
  textureSize,
  format,
  mipLevel)
  {
    const textureSizeAtLevel = physicalMipSize(textureSize, format, '2d', mipLevel);
    const aspectBytesPerBlock = depthStencilFormatAspectSize(format, 'stencil-only');
    const byteSize =
    aspectBytesPerBlock *
    textureSizeAtLevel.width *
    textureSizeAtLevel.height *
    textureSizeAtLevel.depthOrArrayLayers;
    return dataGenerator.generateView(byteSize);
  }

  DoCopyTextureToTextureTest(
  dimension,
  srcTextureSize,
  dstTextureSize,
  srcFormat,
  dstFormat,
  copyBoxOffsets,




  srcCopyLevel,
  dstCopyLevel)
  {
    this.skipIfTextureFormatNotSupported(srcFormat, dstFormat);
    this.skipIfCopyTextureToTextureNotSupportedForFormat(srcFormat, dstFormat);
    this.skipIfTextureFormatAndDimensionNotCompatible(srcFormat, dimension);
    this.skipIfTextureFormatAndDimensionNotCompatible(dstFormat, dimension);

    // If we're in compatibility mode and it's a compressed texture
    // then we need to render the texture to test the results of the copy.
    const extraTextureUsageFlags =
    isCompressedTextureFormat(dstFormat) && this.isCompatibility ?
    GPUTextureUsage.TEXTURE_BINDING :
    0;
    const mipLevelCount = dimension === '1d' ? 1 : 4;

    // Create srcTexture and dstTexture
    const srcTextureDesc = {
      dimension,
      size: srcTextureSize,
      format: srcFormat,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
      mipLevelCount
    };
    const srcTexture = this.createTextureTracked(srcTextureDesc);
    const dstTextureDesc = {
      dimension,
      size: dstTextureSize,
      format: dstFormat,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST | extraTextureUsageFlags,
      mipLevelCount
    };
    const dstTexture = this.createTextureTracked(dstTextureDesc);

    // Fill the whole subresource of srcTexture at srcCopyLevel with initialSrcData.
    const initialSrcData = this.GetInitialDataPerMipLevel(
      dimension,
      srcTextureSize,
      srcFormat,
      srcCopyLevel
    );
    const srcTextureSizeAtLevel = physicalMipSize(
      srcTextureSize,
      srcFormat,
      dimension,
      srcCopyLevel
    );
    const { bytesPerBlock, blockWidth, blockHeight } = getBlockInfoForColorTextureFormat(srcFormat);
    const srcBlocksPerRow = srcTextureSizeAtLevel.width / blockWidth;
    const srcBlockRowsPerImage = srcTextureSizeAtLevel.height / blockHeight;
    this.device.queue.writeTexture(
      { texture: srcTexture, mipLevel: srcCopyLevel },
      initialSrcData,
      {
        bytesPerRow: srcBlocksPerRow * bytesPerBlock,
        rowsPerImage: srcBlockRowsPerImage
      },
      srcTextureSizeAtLevel
    );

    // Copy the region specified by copyBoxOffsets from srcTexture to dstTexture.
    const dstTextureSizeAtLevel = physicalMipSize(
      dstTextureSize,
      dstFormat,
      dimension,
      dstCopyLevel
    );
    const minWidth = Math.min(srcTextureSizeAtLevel.width, dstTextureSizeAtLevel.width);
    const minHeight = Math.min(srcTextureSizeAtLevel.height, dstTextureSizeAtLevel.height);
    const minDepth = Math.min(
      srcTextureSizeAtLevel.depthOrArrayLayers,
      dstTextureSizeAtLevel.depthOrArrayLayers
    );

    const appliedSrcOffset = {
      x: Math.min(copyBoxOffsets.srcOffset.x * blockWidth, minWidth),
      y: Math.min(copyBoxOffsets.srcOffset.y * blockHeight, minHeight),
      z: Math.min(copyBoxOffsets.srcOffset.z, minDepth)
    };
    const appliedDstOffset = {
      x: Math.min(copyBoxOffsets.dstOffset.x * blockWidth, minWidth),
      y: Math.min(copyBoxOffsets.dstOffset.y * blockHeight, minHeight),
      z: Math.min(copyBoxOffsets.dstOffset.z, minDepth)
    };

    const appliedCopyWidth = Math.max(
      minWidth +
      copyBoxOffsets.copyExtent.width * blockWidth -
      Math.max(appliedSrcOffset.x, appliedDstOffset.x),
      0
    );
    const appliedCopyHeight = Math.max(
      minHeight +
      copyBoxOffsets.copyExtent.height * blockHeight -
      Math.max(appliedSrcOffset.y, appliedDstOffset.y),
      0
    );
    assert(appliedCopyWidth % blockWidth === 0 && appliedCopyHeight % blockHeight === 0);

    const appliedCopyDepth = Math.max(
      0,
      minDepth +
      copyBoxOffsets.copyExtent.depthOrArrayLayers -
      Math.max(appliedSrcOffset.z, appliedDstOffset.z)
    );
    assert(appliedCopyDepth >= 0);

    const appliedSize = {
      width: appliedCopyWidth,
      height: appliedCopyHeight,
      depthOrArrayLayers: appliedCopyDepth
    };

    {
      const encoder = this.device.createCommandEncoder();
      encoder.copyTextureToTexture(
        { texture: srcTexture, mipLevel: srcCopyLevel, origin: appliedSrcOffset },
        { texture: dstTexture, mipLevel: dstCopyLevel, origin: appliedDstOffset },
        appliedSize
      );
      this.device.queue.submit([encoder.finish()]);
    }

    const dstBlocksPerRow = dstTextureSizeAtLevel.width / blockWidth;
    const dstBlockRowsPerImage = dstTextureSizeAtLevel.height / blockHeight;
    const bytesPerDstAlignedBlockRow = align(dstBlocksPerRow * bytesPerBlock, 256);
    const dstBufferSize =
    (dstBlockRowsPerImage * dstTextureSizeAtLevel.depthOrArrayLayers - 1) *
    bytesPerDstAlignedBlockRow +
    align(dstBlocksPerRow * bytesPerBlock, 4);

    if (isCompressedTextureFormat(dstTexture.format) && this.isCompatibility) {
      assert(textureFormatsAreViewCompatible(this.device, srcFormat, dstFormat));
      // compare by rendering. We need the expected texture to match
      // the dstTexture so we'll create a texture where we supply
      // all of the data in JavaScript.
      const expectedTexture = this.createTextureTracked({
        size: [dstTexture.width, dstTexture.height, dstTexture.depthOrArrayLayers],
        mipLevelCount: dstTexture.mipLevelCount,
        format: dstTexture.format,
        usage: dstTexture.usage
      });
      const expectedData = new Uint8Array(dstBufferSize);

      // Execute the equivalent of `copyTextureToTexture`, copying
      // from `initialSrcData` to `expectedData`.
      ttu.updateLinearTextureDataSubBox(this, dstFormat, appliedSize, {
        src: {
          dataLayout: {
            bytesPerRow: srcBlocksPerRow * bytesPerBlock,
            rowsPerImage: srcBlockRowsPerImage,
            offset: 0
          },
          origin: appliedSrcOffset,
          data: initialSrcData
        },
        dest: {
          dataLayout: {
            bytesPerRow: dstBlocksPerRow * bytesPerBlock,
            rowsPerImage: dstBlockRowsPerImage,
            offset: 0
          },
          origin: appliedDstOffset,
          data: expectedData
        }
      });

      // Upload `expectedData` to `expectedTexture`. If `copyTextureToTexture`
      // worked then the contents of `dstTexture` should match `expectedTexture`
      this.queue.writeTexture(
        { texture: expectedTexture, mipLevel: dstCopyLevel },
        expectedData,
        {
          bytesPerRow: dstBlocksPerRow * bytesPerBlock,
          rowsPerImage: dstBlockRowsPerImage
        },
        dstTextureSizeAtLevel
      );

      ttu.expectTexturesToMatchByRendering(
        this,
        dstTexture,
        expectedTexture,
        dstCopyLevel,
        appliedDstOffset,
        appliedSize
      );
      return;
    }

    // Copy the whole content of dstTexture at dstCopyLevel to dstBuffer.
    const dstBufferDesc = {
      size: dstBufferSize,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST
    };
    const dstBuffer = this.createBufferTracked(dstBufferDesc);

    {
      const encoder = this.device.createCommandEncoder();
      encoder.copyTextureToBuffer(
        { texture: dstTexture, mipLevel: dstCopyLevel },
        {
          buffer: dstBuffer,
          bytesPerRow: bytesPerDstAlignedBlockRow,
          rowsPerImage: dstBlockRowsPerImage
        },
        dstTextureSizeAtLevel
      );
      this.device.queue.submit([encoder.finish()]);
    }

    // Fill expectedUint8DataWithPadding with the expected data of dstTexture. The other values in
    // expectedUint8DataWithPadding are kept 0 to check if the texels untouched by the copy are 0
    // (their previous values).
    const expectedUint8DataWithPadding = new Uint8Array(dstBufferSize);
    const expectedUint8Data = new Uint8Array(initialSrcData);

    const appliedCopyBlocksPerRow = appliedCopyWidth / blockWidth;
    const appliedCopyBlockRowsPerImage = appliedCopyHeight / blockHeight;
    const srcCopyOffsetInBlocks = {
      x: appliedSrcOffset.x / blockWidth,
      y: appliedSrcOffset.y / blockHeight,
      z: appliedSrcOffset.z
    };
    const dstCopyOffsetInBlocks = {
      x: appliedDstOffset.x / blockWidth,
      y: appliedDstOffset.y / blockHeight,
      z: appliedDstOffset.z
    };
    const bytesInRow = appliedCopyBlocksPerRow * bytesPerBlock;

    for (let z = 0; z < appliedCopyDepth; ++z) {
      const srcOffsetZ = srcCopyOffsetInBlocks.z + z;
      const dstOffsetZ = dstCopyOffsetInBlocks.z + z;
      for (let y = 0; y < appliedCopyBlockRowsPerImage; ++y) {
        const dstOffsetYInBlocks = dstCopyOffsetInBlocks.y + y;
        const expectedDataWithPaddingOffset =
        bytesPerDstAlignedBlockRow * (dstBlockRowsPerImage * dstOffsetZ + dstOffsetYInBlocks) +
        dstCopyOffsetInBlocks.x * bytesPerBlock;

        const srcOffsetYInBlocks = srcCopyOffsetInBlocks.y + y;
        const expectedDataOffset =
        bytesPerBlock *
        srcBlocksPerRow * (
        srcBlockRowsPerImage * srcOffsetZ + srcOffsetYInBlocks) +
        srcCopyOffsetInBlocks.x * bytesPerBlock;

        memcpy(
          { src: expectedUint8Data, start: expectedDataOffset, length: bytesInRow },
          { dst: expectedUint8DataWithPadding, start: expectedDataWithPaddingOffset }
        );
      }
    }

    if (isCompressedTextureFormat(dstFormat)) {
      this.expectGPUBufferValuesPassCheck(
        dstBuffer,
        (vals) => checkElementsEqual(vals, expectedUint8DataWithPadding),
        {
          srcByteOffset: 0,
          type: Uint8Array,
          typedLength: expectedUint8DataWithPadding.length
        }
      );
      return;
    }

    assert(isRegularTextureFormat(dstFormat));
    const regularDstFormat = dstFormat;

    // Verify the content of the whole subresource of dstTexture at dstCopyLevel (in dstBuffer) is expected.
    const checkByTextureFormat = (actual) => {
      const zero = { x: 0, y: 0, z: 0 };

      const actTexelView = TexelView.fromTextureDataByReference(regularDstFormat, actual, {
        bytesPerRow: bytesInRow,
        rowsPerImage: dstBlockRowsPerImage,
        subrectOrigin: zero,
        subrectSize: dstTextureSizeAtLevel
      });
      const expTexelView = TexelView.fromTextureDataByReference(
        regularDstFormat,
        expectedUint8DataWithPadding,
        {
          bytesPerRow: bytesInRow,
          rowsPerImage: dstBlockRowsPerImage,
          subrectOrigin: zero,
          subrectSize: dstTextureSizeAtLevel
        }
      );

      const failedPixelsMessage = findFailedPixels(
        regularDstFormat,
        zero,
        dstTextureSizeAtLevel,
        { actTexelView, expTexelView },
        {
          maxFractionalDiff: 0
        }
      );

      if (failedPixelsMessage !== undefined) {
        const msg = 'Texture level had unexpected contents:\n' + failedPixelsMessage;
        return new ErrorWithExtra(msg, () => ({
          expTexelView,
          actTexelView
        }));
      }

      return undefined;
    };

    this.expectGPUBufferValuesPassCheck(dstBuffer, checkByTextureFormat, {
      srcByteOffset: 0,
      type: Uint8Array,
      typedLength: expectedUint8DataWithPadding.length
    });
  }

  InitializeStencilAspect(
  sourceTexture,
  initialStencilData,
  srcCopyLevel,
  srcCopyBaseArrayLayer,
  copySize)
  {
    this.queue.writeTexture(
      {
        texture: sourceTexture,
        mipLevel: srcCopyLevel,
        aspect: 'stencil-only',
        origin: { x: 0, y: 0, z: srcCopyBaseArrayLayer }
      },
      initialStencilData,
      { bytesPerRow: copySize[0], rowsPerImage: copySize[1] },
      copySize
    );
  }

  VerifyStencilAspect(
  destinationTexture,
  initialStencilData,
  dstCopyLevel,
  dstCopyBaseArrayLayer,
  copySize)
  {
    const bytesPerRow = align(copySize[0], kBytesPerRowAlignment);
    const rowsPerImage = copySize[1];
    const outputBufferSize = align(
      dataBytesForCopyOrFail({
        layout: { bytesPerRow, rowsPerImage },
        format: 'stencil8',
        copySize,
        method: 'CopyT2B'
      }),
      kBufferSizeAlignment
    );
    const outputBuffer = this.createBufferTracked({
      size: outputBufferSize,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST
    });
    const encoder = this.device.createCommandEncoder();
    encoder.copyTextureToBuffer(
      {
        texture: destinationTexture,
        aspect: 'stencil-only',
        mipLevel: dstCopyLevel,
        origin: { x: 0, y: 0, z: dstCopyBaseArrayLayer }
      },
      { buffer: outputBuffer, bytesPerRow, rowsPerImage },
      copySize
    );
    this.queue.submit([encoder.finish()]);

    const expectedStencilData = new Uint8Array(outputBufferSize);
    for (let z = 0; z < copySize[2]; ++z) {
      const initialOffsetPerLayer = z * copySize[0] * copySize[1];
      const expectedOffsetPerLayer = z * bytesPerRow * rowsPerImage;
      for (let y = 0; y < copySize[1]; ++y) {
        const initialOffsetPerRow = initialOffsetPerLayer + y * copySize[0];
        const expectedOffsetPerRow = expectedOffsetPerLayer + y * bytesPerRow;
        memcpy(
          { src: initialStencilData, start: initialOffsetPerRow, length: copySize[0] },
          { dst: expectedStencilData, start: expectedOffsetPerRow }
        );
      }
    }
    this.expectGPUBufferValuesEqual(outputBuffer, expectedStencilData);
  }

  GetRenderPipelineForT2TCopyWithDepthTests(
  bindGroupLayout,
  hasColorAttachment,
  depthStencil)
  {
    const renderPipelineDescriptor = {
      layout: this.device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] }),
      vertex: {
        module: this.device.createShaderModule({
          code: `
            struct Params {
              copyLayer: f32
            };
            @group(0) @binding(0) var<uniform> param: Params;
            @vertex
            fn main(@builtin(vertex_index) VertexIndex : u32)-> @builtin(position) vec4<f32> {
              var depthValue = 0.5 + 0.2 * sin(param.copyLayer);
              var pos : array<vec3<f32>, 6> = array<vec3<f32>, 6>(
                  vec3<f32>(-1.0,  1.0, depthValue),
                  vec3<f32>(-1.0, -1.0, 0.0),
                  vec3<f32>( 1.0,  1.0, 1.0),
                  vec3<f32>(-1.0, -1.0, 0.0),
                  vec3<f32>( 1.0,  1.0, 1.0),
                  vec3<f32>( 1.0, -1.0, depthValue));
              return vec4<f32>(pos[VertexIndex], 1.0);
            }`
        }),
        entryPoint: 'main'
      },
      depthStencil
    };
    if (hasColorAttachment) {
      renderPipelineDescriptor.fragment = {
        module: this.device.createShaderModule({
          code: `
            @fragment
            fn main() -> @location(0) vec4<f32> {
              return vec4<f32>(0.0, 1.0, 0.0, 1.0);
            }`
        }),
        entryPoint: 'main',
        targets: [{ format: 'rgba8unorm' }]
      };
    }
    return this.device.createRenderPipeline(renderPipelineDescriptor);
  }

  GetBindGroupLayoutForT2TCopyWithDepthTests() {
    return this.device.createBindGroupLayout({
      entries: [
      {
        binding: 0,
        visibility: GPUShaderStage.VERTEX,
        buffer: {
          type: 'uniform',
          minBindingSize: 4,
          hasDynamicOffset: true
        }
      }]

    });
  }

  GetBindGroupForT2TCopyWithDepthTests(
  bindGroupLayout,
  totalCopyArrayLayers)
  {
    // Prepare the uniform buffer that contains all the copy layers to generate different depth
    // values for different copy layers.
    assert(totalCopyArrayLayers > 0);
    const uniformBufferSize = kMinDynamicBufferOffsetAlignment * (totalCopyArrayLayers - 1) + 4;
    const uniformBufferData = new Float32Array(uniformBufferSize / 4);
    for (let i = 1; i < totalCopyArrayLayers; ++i) {
      uniformBufferData[kMinDynamicBufferOffsetAlignment / 4 * i] = i;
    }
    const uniformBuffer = this.makeBufferWithContents(
      uniformBufferData,
      GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM
    );
    return this.device.createBindGroup({
      layout: bindGroupLayout,
      entries: [
      {
        binding: 0,
        resource: {
          buffer: uniformBuffer,
          size: 4
        }
      }]

    });
  }

  /** Initialize the depth aspect of sourceTexture with draw calls */
  InitializeDepthAspect(
  sourceTexture,
  depthFormat,
  srcCopyLevel,
  srcCopyBaseArrayLayer,
  copySize)
  {
    // Prepare a renderPipeline with depthCompareFunction == 'always' and depthWriteEnabled == true
    // for the initializations of the depth attachment.
    const bindGroupLayout = this.GetBindGroupLayoutForT2TCopyWithDepthTests();
    const renderPipeline = this.GetRenderPipelineForT2TCopyWithDepthTests(bindGroupLayout, false, {
      format: depthFormat,
      depthWriteEnabled: true,
      depthCompare: 'always'
    });
    const bindGroup = this.GetBindGroupForT2TCopyWithDepthTests(bindGroupLayout, copySize[2]);

    const hasStencil = isStencilTextureFormat(sourceTexture.format);
    const encoder = this.device.createCommandEncoder();
    for (let srcCopyLayer = 0; srcCopyLayer < copySize[2]; ++srcCopyLayer) {
      const renderPass = encoder.beginRenderPass({
        colorAttachments: [],
        depthStencilAttachment: {
          view: sourceTexture.createView({
            baseArrayLayer: srcCopyLayer + srcCopyBaseArrayLayer,
            arrayLayerCount: 1,
            baseMipLevel: srcCopyLevel,
            mipLevelCount: 1
          }),
          depthClearValue: 0.0,
          depthLoadOp: 'clear',
          depthStoreOp: 'store',
          stencilLoadOp: hasStencil ? 'load' : undefined,
          stencilStoreOp: hasStencil ? 'store' : undefined
        }
      });
      renderPass.setBindGroup(0, bindGroup, [srcCopyLayer * kMinDynamicBufferOffsetAlignment]);
      renderPass.setPipeline(renderPipeline);
      renderPass.draw(6);
      renderPass.end();
    }
    this.queue.submit([encoder.finish()]);
  }

  VerifyDepthAspect(
  destinationTexture,
  depthFormat,
  dstCopyLevel,
  dstCopyBaseArrayLayer,
  copySize)
  {
    // Prepare a renderPipeline with depthCompareFunction == 'equal' and depthWriteEnabled == false
    // for the comparison of the depth attachment.
    const bindGroupLayout = this.GetBindGroupLayoutForT2TCopyWithDepthTests();
    const renderPipeline = this.GetRenderPipelineForT2TCopyWithDepthTests(bindGroupLayout, true, {
      format: depthFormat,
      depthWriteEnabled: false,
      depthCompare: 'equal'
    });
    const bindGroup = this.GetBindGroupForT2TCopyWithDepthTests(bindGroupLayout, copySize[2]);

    const outputColorTexture = this.createTextureTracked({
      format: 'rgba8unorm',
      size: copySize,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC
    });
    const hasStencil = isStencilTextureFormat(destinationTexture.format);
    const encoder = this.device.createCommandEncoder();
    for (let dstCopyLayer = 0; dstCopyLayer < copySize[2]; ++dstCopyLayer) {
      // If the depth value is not expected, the color of outputColorTexture will remain Red after
      // the render pass.
      const renderPass = encoder.beginRenderPass({
        colorAttachments: [
        {
          view: outputColorTexture.createView({
            baseArrayLayer: dstCopyLayer,
            arrayLayerCount: 1
          }),
          clearValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
          loadOp: 'clear',
          storeOp: 'store'
        }],

        depthStencilAttachment: {
          view: destinationTexture.createView({
            baseArrayLayer: dstCopyLayer + dstCopyBaseArrayLayer,
            arrayLayerCount: 1,
            baseMipLevel: dstCopyLevel,
            mipLevelCount: 1
          }),
          depthLoadOp: 'load',
          depthStoreOp: 'store',
          stencilLoadOp: hasStencil ? 'load' : undefined,
          stencilStoreOp: hasStencil ? 'store' : undefined
        }
      });
      renderPass.setBindGroup(0, bindGroup, [dstCopyLayer * kMinDynamicBufferOffsetAlignment]);
      renderPass.setPipeline(renderPipeline);
      renderPass.draw(6);
      renderPass.end();
    }
    this.queue.submit([encoder.finish()]);

    this.expectSingleColor(outputColorTexture, 'rgba8unorm', {
      size: copySize,
      exp: { R: 0.0, G: 1.0, B: 0.0, A: 1.0 }
    });
  }
}

const kCopyBoxOffsetsForWholeDepth = [
// From (0, 0) of src to (0, 0) of dst.
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: 0 }
},
// From (0, 0) of src to (blockWidth, 0) of dst.
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 1, y: 0, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: 0 }
},
// From (0, 0) of src to (0, blockHeight) of dst.
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 1, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: 0 }
},
// From (blockWidth, 0) of src to (0, 0) of dst.
{
  srcOffset: { x: 1, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: 0 }
},
// From (0, blockHeight) of src to (0, 0) of dst.
{
  srcOffset: { x: 0, y: 1, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: 0 }
},
// From (blockWidth, 0) of src to (0, 0) of dst, and the copy extent will not cover the last
// texel block column of both source and destination texture.
{
  srcOffset: { x: 1, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: -1, height: 0, depthOrArrayLayers: 0 }
},
// From (0, blockHeight) of src to (0, 0) of dst, and the copy extent will not cover the last
// texel block row of both source and destination texture.
{
  srcOffset: { x: 0, y: 1, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: -1, depthOrArrayLayers: 0 }
}];


const kCopyBoxOffsetsFor2DArrayTextures = [
// Copy the whole array slices from the source texture to the destination texture.
// The copy extent will cover the whole subresource of either source or the
// destination texture
...kCopyBoxOffsetsForWholeDepth,

// Copy 1 texture slice from the 1st slice of the source texture to the 1st slice of the
// destination texture.
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: -2 }
},
// Copy 1 texture slice from the 2nd slice of the source texture to the 2nd slice of the
// destination texture.
{
  srcOffset: { x: 0, y: 0, z: 1 },
  dstOffset: { x: 0, y: 0, z: 1 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: -3 }
},
// Copy 1 texture slice from the 1st slice of the source texture to the 2nd slice of the
// destination texture.
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 1 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: -1 }
},
// Copy 1 texture slice from the 2nd slice of the source texture to the 1st slice of the
// destination texture.
{
  srcOffset: { x: 0, y: 0, z: 1 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: -1 }
},
// Copy 2 texture slices from the 1st slice of the source texture to the 1st slice of the
// destination texture.
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: -3 }
},
// Copy 3 texture slices from the 2nd slice of the source texture to the 2nd slice of the
// destination texture.
{
  srcOffset: { x: 0, y: 0, z: 1 },
  dstOffset: { x: 0, y: 0, z: 1 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: -1 }
}];


export const g = makeTestGroup(F);

g.test('color_textures,non_compressed,non_array').
desc(
  `
  Validate the correctness of the copy by filling the srcTexture with testable data and any
  non-compressed color format supported by WebGPU, doing CopyTextureToTexture() copy, and verifying
  the content of the whole dstTexture.

  Copy {1 texel block, part of, the whole} srcTexture to the dstTexture {with, without} a non-zero
  valid srcOffset that
  - covers the whole dstTexture subresource
  - covers the corners of the dstTexture
  - doesn't cover any texels that are on the edge of the dstTexture
  - covers the mipmap level > 0

  Tests for all pairs of valid source/destination formats, and all texture dimensions.
  `
).
params((u) =>
u.
combine('srcFormat', kRegularTextureFormats).
combine('dstFormat', kRegularTextureFormats).
filter(({ srcFormat, dstFormat }) => {
  const srcBaseFormat = getBaseFormatForTextureFormat(srcFormat);
  const dstBaseFormat = getBaseFormatForTextureFormat(dstFormat);
  return (
    srcFormat === dstFormat ||
    srcBaseFormat !== undefined &&
    dstBaseFormat !== undefined &&
    srcBaseFormat === dstBaseFormat);

}).
combine('dimension', kTextureDimensions).
filter(
  ({ dimension, srcFormat, dstFormat }) =>
  textureFormatAndDimensionPossiblyCompatible(dimension, srcFormat) &&
  textureFormatAndDimensionPossiblyCompatible(dimension, dstFormat)
).
beginSubcases().
expandWithParams((p) => {
  const params = [
  {
    srcTextureSize: { width: 32, height: 32, depthOrArrayLayers: 1 },
    dstTextureSize: { width: 32, height: 32, depthOrArrayLayers: 1 }
  },
  {
    srcTextureSize: { width: 31, height: 33, depthOrArrayLayers: 1 },
    dstTextureSize: { width: 31, height: 33, depthOrArrayLayers: 1 }
  },
  {
    srcTextureSize: { width: 32, height: 32, depthOrArrayLayers: 1 },
    dstTextureSize: { width: 64, height: 64, depthOrArrayLayers: 1 }
  },
  {
    srcTextureSize: { width: 32, height: 32, depthOrArrayLayers: 1 },
    dstTextureSize: { width: 63, height: 61, depthOrArrayLayers: 1 }
  }];

  if (p.dimension === '1d') {
    for (const param of params) {
      param.srcTextureSize.height = 1;
      param.dstTextureSize.height = 1;
    }
  }

  return params;
}).
combine('copyBoxOffsets', kCopyBoxOffsetsForWholeDepth).
unless(
  (p) =>
  p.dimension === '1d' && (
  p.copyBoxOffsets.copyExtent.height !== 0 ||
  p.copyBoxOffsets.srcOffset.y !== 0 ||
  p.copyBoxOffsets.dstOffset.y !== 0)
).
combine('srcCopyLevel', [0, 3]).
combine('dstCopyLevel', [0, 3]).
unless((p) => p.dimension === '1d' && (p.srcCopyLevel !== 0 || p.dstCopyLevel !== 0))
).
fn((t) => {
  const {
    dimension,
    srcTextureSize,
    dstTextureSize,
    srcFormat,
    dstFormat,
    copyBoxOffsets,
    srcCopyLevel,
    dstCopyLevel
  } = t.params;

  t.DoCopyTextureToTextureTest(
    dimension,
    srcTextureSize,
    dstTextureSize,
    srcFormat,
    dstFormat,
    copyBoxOffsets,
    srcCopyLevel,
    dstCopyLevel
  );
});

g.test('color_textures,compressed,non_array').
desc(
  `
  Validate the correctness of the copy by filling the srcTexture with testable data and any
  compressed color format supported by WebGPU, doing CopyTextureToTexture() copy, and verifying
  the content of the whole dstTexture.

  Tests for all pairs of valid source/destination formats, and all texture dimensions.
  `
).
params((u) =>
u.
combine('srcFormat', kCompressedTextureFormats).
combine('dstFormat', kCompressedTextureFormats).
filter(({ srcFormat, dstFormat }) => {
  const srcBaseFormat = getBaseFormatForTextureFormat(srcFormat);
  const dstBaseFormat = getBaseFormatForTextureFormat(dstFormat);
  return (
    srcFormat === dstFormat ||
    srcBaseFormat !== undefined &&
    dstBaseFormat !== undefined &&
    srcBaseFormat === dstBaseFormat);

}).
combine('dimension', kTextureDimensions).
beginSubcases().
combine('textureSizeInBlocks', [
// The heights and widths in blocks are all power of 2
{ src: { width: 16, height: 8 }, dst: { width: 16, height: 8 } },
// The virtual width of the source texture at mipmap level 2 (15) is not a multiple of 4 blocks
{ src: { width: 15, height: 8 }, dst: { width: 16, height: 8 } },
// The virtual width of the destination texture at mipmap level 2 (15) is not a multiple
// of 4 blocks
{ src: { width: 16, height: 8 }, dst: { width: 15, height: 8 } },
// The virtual height of the source texture at mipmap level 2 (13) is not a multiple of 4 blocks
{ src: { width: 16, height: 13 }, dst: { width: 16, height: 8 } },
// The virtual height of the destination texture at mipmap level 2 (13) is not a
// multiple of 4 blocks
{ src: { width: 16, height: 8 }, dst: { width: 16, height: 13 } },
// None of the widths or heights in blocks are power of 2
{ src: { width: 15, height: 13 }, dst: { width: 15, height: 13 } }]
).
combine('copyBoxOffsets', kCopyBoxOffsetsForWholeDepth).
combine('srcCopyLevel', [0, 2]).
combine('dstCopyLevel', [0, 2])
).
fn((t) => {
  const {
    dimension,
    textureSizeInBlocks,
    srcFormat,
    dstFormat,
    copyBoxOffsets,
    srcCopyLevel,
    dstCopyLevel
  } = t.params;
  t.skipIfTextureFormatAndDimensionNotCompatible(srcFormat, dimension);
  t.skipIfTextureFormatAndDimensionNotCompatible(dstFormat, dimension);
  t.skipIfCopyTextureToTextureNotSupportedForFormat(srcFormat, dstFormat);
  const { blockWidth: srcBlockWidth, blockHeight: srcBlockHeight } =
  getBlockInfoForColorTextureFormat(srcFormat);
  const { blockWidth: dstBlockWidth, blockHeight: dstBlockHeight } =
  getBlockInfoForColorTextureFormat(dstFormat);

  t.DoCopyTextureToTextureTest(
    dimension,
    {
      width: textureSizeInBlocks.src.width * srcBlockWidth,
      height: textureSizeInBlocks.src.height * srcBlockHeight,
      depthOrArrayLayers: 1
    },
    {
      width: textureSizeInBlocks.dst.width * dstBlockWidth,
      height: textureSizeInBlocks.dst.height * dstBlockHeight,
      depthOrArrayLayers: 1
    },
    srcFormat,
    dstFormat,
    copyBoxOffsets,
    srcCopyLevel,
    dstCopyLevel
  );
});

g.test('color_textures,non_compressed,array').
desc(
  `
  Validate the correctness of the texture-to-texture copy on 2D array textures by filling the
  srcTexture with testable data and any non-compressed color format supported by WebGPU, doing
  CopyTextureToTexture() copy, and verifying the content of the whole dstTexture.
  `
).
params((u) =>
u.
combine('srcFormat', kRegularTextureFormats).
combine('dstFormat', kRegularTextureFormats).
filter(({ srcFormat, dstFormat }) => {
  const srcBaseFormat = getBaseFormatForTextureFormat(srcFormat);
  const dstBaseFormat = getBaseFormatForTextureFormat(dstFormat);
  return (
    srcFormat === dstFormat ||
    srcBaseFormat !== undefined &&
    dstBaseFormat !== undefined &&
    srcBaseFormat === dstBaseFormat);

}).
combine('dimension', ['2d', '3d']).
filter(
  ({ dimension, srcFormat, dstFormat }) =>
  textureFormatAndDimensionPossiblyCompatible(dimension, srcFormat) &&
  textureFormatAndDimensionPossiblyCompatible(dimension, dstFormat)
).
beginSubcases().
combine('textureSize', [
{
  srcTextureSize: { width: 64, height: 32, depthOrArrayLayers: 5 },
  dstTextureSize: { width: 64, height: 32, depthOrArrayLayers: 5 }
},
{
  srcTextureSize: { width: 31, height: 33, depthOrArrayLayers: 5 },
  dstTextureSize: { width: 31, height: 33, depthOrArrayLayers: 5 }
},
{
  srcTextureSize: { width: 31, height: 32, depthOrArrayLayers: 33 },
  dstTextureSize: { width: 31, height: 32, depthOrArrayLayers: 33 }
}]
).

combine('copyBoxOffsets', kCopyBoxOffsetsFor2DArrayTextures).
combine('srcCopyLevel', [0, 3]).
combine('dstCopyLevel', [0, 3])
).
fn((t) => {
  const {
    dimension,
    textureSize,
    srcFormat,
    dstFormat,
    copyBoxOffsets,
    srcCopyLevel,
    dstCopyLevel
  } = t.params;

  t.DoCopyTextureToTextureTest(
    dimension,
    textureSize.srcTextureSize,
    textureSize.dstTextureSize,
    srcFormat,
    dstFormat,
    copyBoxOffsets,
    srcCopyLevel,
    dstCopyLevel
  );
});

g.test('color_textures,compressed,array').
desc(
  `
  Validate the correctness of the texture-to-texture copy on 2D array textures by filling the
  srcTexture with testable data and any compressed color format supported by WebGPU, doing
  CopyTextureToTexture() copy, and verifying the content of the whole dstTexture.

  Tests for all pairs of valid source/destination formats, and all texture dimensions.
  `
).
params((u) =>
u.
combine('srcFormat', kCompressedTextureFormats).
combine('dstFormat', kCompressedTextureFormats).
filter(({ srcFormat, dstFormat }) => {
  const srcBaseFormat = getBaseFormatForTextureFormat(srcFormat);
  const dstBaseFormat = getBaseFormatForTextureFormat(dstFormat);
  return (
    srcFormat === dstFormat ||
    srcBaseFormat !== undefined &&
    dstBaseFormat !== undefined &&
    srcBaseFormat === dstBaseFormat);

}).
combine('dimension', ['2d', '3d']).
beginSubcases().
combine('textureSizeInBlocks', [
// The heights and widths in blocks are all power of 2
{ src: { width: 2, height: 2 }, dst: { width: 2, height: 2 } },
// None of the widths or heights in blocks are power of 2
{ src: { width: 15, height: 13 }, dst: { width: 15, height: 13 } }]
).
combine('copyBoxOffsets', kCopyBoxOffsetsFor2DArrayTextures).
combine('srcCopyLevel', [0, 2]).
combine('dstCopyLevel', [0, 2])
).
fn((t) => {
  const {
    dimension,
    textureSizeInBlocks,
    srcFormat,
    dstFormat,
    copyBoxOffsets,
    srcCopyLevel,
    dstCopyLevel
  } = t.params;
  const { blockWidth: srcBlockWidth, blockHeight: srcBlockHeight } =
  getBlockInfoForColorTextureFormat(srcFormat);
  const { blockWidth: dstBlockWidth, blockHeight: dstBlockHeight } =
  getBlockInfoForColorTextureFormat(dstFormat);

  t.DoCopyTextureToTextureTest(
    dimension,
    {
      width: textureSizeInBlocks.src.width * srcBlockWidth,
      height: textureSizeInBlocks.src.height * srcBlockHeight,
      depthOrArrayLayers: 5
    },
    {
      width: textureSizeInBlocks.dst.width * dstBlockWidth,
      height: textureSizeInBlocks.dst.height * dstBlockHeight,
      depthOrArrayLayers: 5
    },
    srcFormat,
    dstFormat,
    copyBoxOffsets,
    srcCopyLevel,
    dstCopyLevel
  );
});

g.test('zero_sized').
desc(
  `
  Validate the correctness of zero-sized copies (should be no-ops).

  - For each texture dimension.
  - Copies that are zero-sized in only one dimension {x, y, z}, each touching the {lower, upper} end
  of that dimension.
  `
).
paramsSubcasesOnly((u) =>
u //
.combineWithParams([
{ dimension: '1d', textureSize: { width: 32, height: 1, depthOrArrayLayers: 1 } },
{ dimension: '2d', textureSize: { width: 32, height: 32, depthOrArrayLayers: 5 } },
{ dimension: '3d', textureSize: { width: 32, height: 32, depthOrArrayLayers: 5 } }]
).
combine('copyBoxOffset', [
// copyExtent.width === 0
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: -64, height: 0, depthOrArrayLayers: 0 }
},
// copyExtent.width === 0 && srcOffset.x === textureWidth
{
  srcOffset: { x: 64, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: -64, height: 0, depthOrArrayLayers: 0 }
},
// copyExtent.width === 0 && dstOffset.x === textureWidth
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 64, y: 0, z: 0 },
  copyExtent: { width: -64, height: 0, depthOrArrayLayers: 0 }
},
// copyExtent.height === 0
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: -32, depthOrArrayLayers: 0 }
},
// copyExtent.height === 0 && srcOffset.y === textureHeight
{
  srcOffset: { x: 0, y: 32, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: -32, depthOrArrayLayers: 0 }
},
// copyExtent.height === 0 && dstOffset.y === textureHeight
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 32, z: 0 },
  copyExtent: { width: 0, height: -32, depthOrArrayLayers: 0 }
},
// copyExtent.depthOrArrayLayers === 0
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: -5 }
},
// copyExtent.depthOrArrayLayers === 0 && srcOffset.z === textureDepth
{
  srcOffset: { x: 0, y: 0, z: 5 },
  dstOffset: { x: 0, y: 0, z: 0 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: 0 }
},
// copyExtent.depthOrArrayLayers === 0 && dstOffset.z === textureDepth
{
  srcOffset: { x: 0, y: 0, z: 0 },
  dstOffset: { x: 0, y: 0, z: 5 },
  copyExtent: { width: 0, height: 0, depthOrArrayLayers: 0 }
}]
).
unless(
  (p) =>
  p.dimension === '1d' && (
  p.copyBoxOffset.copyExtent.height !== 0 ||
  p.copyBoxOffset.srcOffset.y !== 0 ||
  p.copyBoxOffset.dstOffset.y !== 0)
).
combine('srcCopyLevel', [0, 3]).
combine('dstCopyLevel', [0, 3]).
unless((p) => p.dimension === '1d' && (p.srcCopyLevel !== 0 || p.dstCopyLevel !== 0))
).
fn((t) => {
  const { dimension, textureSize, copyBoxOffset, srcCopyLevel, dstCopyLevel } = t.params;

  const srcFormat = 'rgba8unorm';
  const dstFormat = 'rgba8unorm';

  t.DoCopyTextureToTextureTest(
    dimension,
    textureSize,
    textureSize,
    srcFormat,
    dstFormat,
    copyBoxOffset,
    srcCopyLevel,
    dstCopyLevel
  );
});

g.test('copy_depth_stencil').
desc(
  `
  Validate the correctness of copyTextureToTexture() with depth and stencil aspect.

  For all the texture formats with stencil aspect:
  - Initialize the stencil aspect of the source texture with writeTexture().
  - Copy the stencil aspect from the source texture into the destination texture
  - Copy the stencil aspect of the destination texture into another staging buffer and check its
    content
  - Test the copies from / into zero / non-zero array layer / mipmap levels
  - Test copying multiple array layers

  For all the texture formats with depth aspect:
  - Initialize the depth aspect of the source texture with a draw call
  - Copy the depth aspect from the source texture into the destination texture
  - Validate the content in the destination texture with the depth comparison function 'equal'
  `
).
params((u) =>
u.
combine('format', kDepthStencilFormats).
beginSubcases().
combine('srcTextureSize', [
{ width: 32, height: 16, depthOrArrayLayers: 1 },
{ width: 32, height: 16, depthOrArrayLayers: 4 },
{ width: 24, height: 48, depthOrArrayLayers: 5 }]
).
combine('srcCopyLevel', [0, 2]).
combine('dstCopyLevel', [0, 2]).
combine('srcCopyBaseArrayLayer', [0, 1]).
combine('dstCopyBaseArrayLayer', [0, 1]).
filter((t) => {
  return (
    t.srcTextureSize.depthOrArrayLayers > t.srcCopyBaseArrayLayer &&
    t.srcTextureSize.depthOrArrayLayers > t.dstCopyBaseArrayLayer);

})
).
fn((t) => {
  const {
    format,
    srcTextureSize,
    srcCopyLevel,
    dstCopyLevel,
    srcCopyBaseArrayLayer,
    dstCopyBaseArrayLayer
  } = t.params;
  t.skipIfTextureFormatNotSupported(format);

  const copySize = [
  srcTextureSize.width >> srcCopyLevel,
  srcTextureSize.height >> srcCopyLevel,
  srcTextureSize.depthOrArrayLayers - Math.max(srcCopyBaseArrayLayer, dstCopyBaseArrayLayer)];

  const sourceTexture = t.createTextureTracked({
    format,
    size: srcTextureSize,
    usage:
    GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    mipLevelCount: srcCopyLevel + 1
  });
  const destinationTexture = t.createTextureTracked({
    format,
    size: [
    copySize[0] << dstCopyLevel,
    copySize[1] << dstCopyLevel,
    srcTextureSize.depthOrArrayLayers],

    usage:
    GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    mipLevelCount: dstCopyLevel + 1
  });

  let initialStencilData = undefined;
  if (isStencilTextureFormat(format)) {
    initialStencilData = t.GetInitialStencilDataPerMipLevel(srcTextureSize, format, srcCopyLevel);
    t.InitializeStencilAspect(
      sourceTexture,
      initialStencilData,
      srcCopyLevel,
      srcCopyBaseArrayLayer,
      copySize
    );
  }
  if (isDepthTextureFormat(format)) {
    t.InitializeDepthAspect(sourceTexture, format, srcCopyLevel, srcCopyBaseArrayLayer, copySize);
  }

  const encoder = t.device.createCommandEncoder();
  encoder.copyTextureToTexture(
    {
      texture: sourceTexture,
      mipLevel: srcCopyLevel,
      origin: { x: 0, y: 0, z: srcCopyBaseArrayLayer }
    },
    {
      texture: destinationTexture,
      mipLevel: dstCopyLevel,
      origin: { x: 0, y: 0, z: dstCopyBaseArrayLayer }
    },
    copySize
  );
  t.queue.submit([encoder.finish()]);

  if (isStencilTextureFormat(format)) {
    assert(initialStencilData !== undefined);
    t.VerifyStencilAspect(
      destinationTexture,
      initialStencilData,
      dstCopyLevel,
      dstCopyBaseArrayLayer,
      copySize
    );
  }
  if (isDepthTextureFormat(format)) {
    t.VerifyDepthAspect(
      destinationTexture,
      format,
      dstCopyLevel,
      dstCopyBaseArrayLayer,
      copySize
    );
  }
});

g.test('copy_multisampled_color').
desc(
  `
  Validate the correctness of copyTextureToTexture() with multisampled color formats.

  - Initialize the source texture with a triangle in a render pass.
  - Copy from the source texture into the destination texture with CopyTextureToTexture().
  - Compare every sub-pixel of source texture and destination texture in another render pass:
    - If they are different, then output RED; otherwise output GREEN
  - Verify the pixels in the output texture are all GREEN.
  - Note that in current WebGPU SPEC the mipmap level count and array layer count of a multisampled
    texture can only be 1.
  `
).
fn((t) => {
  t.skipIf(t.isCompatibility, 'multisample textures are not copyable in compatibility mode');
  const textureSize = [32, 16, 1];
  const kColorFormat = 'rgba8unorm';
  const kSampleCount = 4;

  const sourceTexture = t.createTextureTracked({
    format: kColorFormat,
    size: textureSize,
    usage:
    GPUTextureUsage.COPY_SRC |
    GPUTextureUsage.TEXTURE_BINDING |
    GPUTextureUsage.RENDER_ATTACHMENT,
    sampleCount: kSampleCount
  });
  const destinationTexture = t.createTextureTracked({
    format: kColorFormat,
    size: textureSize,
    usage:
    GPUTextureUsage.COPY_DST |
    GPUTextureUsage.TEXTURE_BINDING |
    GPUTextureUsage.RENDER_ATTACHMENT,
    sampleCount: kSampleCount
  });

  // Initialize sourceTexture with a draw call.
  const renderPipelineForInit = t.device.createRenderPipeline({
    layout: 'auto',
    vertex: {
      module: t.device.createShaderModule({
        code: `
            @vertex
            fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
              var pos = array<vec2<f32>, 3>(
                  vec2<f32>(-1.0,  1.0),
                  vec2<f32>( 1.0,  1.0),
                  vec2<f32>( 1.0, -1.0)
              );
              return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
            }`
      }),
      entryPoint: 'main'
    },
    fragment: {
      module: t.device.createShaderModule({
        code: `
            @fragment
            fn main() -> @location(0) vec4<f32> {
              return vec4<f32>(0.3, 0.5, 0.8, 1.0);
            }`
      }),
      entryPoint: 'main',
      targets: [{ format: kColorFormat }]
    },
    multisample: {
      count: kSampleCount
    }
  });
  const initEncoder = t.device.createCommandEncoder();
  const renderPassForInit = initEncoder.beginRenderPass({
    colorAttachments: [
    {
      view: sourceTexture.createView(),
      clearValue: [1.0, 0.0, 0.0, 1.0],
      loadOp: 'clear',
      storeOp: 'store'
    }]

  });
  renderPassForInit.setPipeline(renderPipelineForInit);
  renderPassForInit.draw(3);
  renderPassForInit.end();
  t.queue.submit([initEncoder.finish()]);

  // Do the texture-to-texture copy
  const copyEncoder = t.device.createCommandEncoder();
  copyEncoder.copyTextureToTexture(
    {
      texture: sourceTexture
    },
    {
      texture: destinationTexture
    },
    textureSize
  );
  t.queue.submit([copyEncoder.finish()]);

  // Verify if all the sub-pixel values at the same location of sourceTexture and
  // destinationTexture are equal.
  const renderPipelineForValidation = t.device.createRenderPipeline({
    layout: 'auto',
    vertex: {
      module: t.device.createShaderModule({
        code: `
          @vertex
          fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
            var pos = array<vec2<f32>, 6>(
              vec2<f32>(-1.0,  1.0),
              vec2<f32>(-1.0, -1.0),
              vec2<f32>( 1.0,  1.0),
              vec2<f32>(-1.0, -1.0),
              vec2<f32>( 1.0,  1.0),
              vec2<f32>( 1.0, -1.0));
            return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
          }`
      }),
      entryPoint: 'main'
    },
    fragment: {
      module: t.device.createShaderModule({
        code: `
          @group(0) @binding(0) var sourceTexture : texture_multisampled_2d<f32>;
          @group(0) @binding(1) var destinationTexture : texture_multisampled_2d<f32>;
          @fragment
          fn main(@builtin(position) coord_in: vec4<f32>) -> @location(0) vec4<f32> {
            var coord_in_vec2 = vec2<i32>(i32(coord_in.x), i32(coord_in.y));
            for (var sampleIndex = 0; sampleIndex < ${kSampleCount};
              sampleIndex = sampleIndex + 1) {
              var sourceSubPixel : vec4<f32> =
                textureLoad(sourceTexture, coord_in_vec2, sampleIndex);
              var destinationSubPixel : vec4<f32> =
                textureLoad(destinationTexture, coord_in_vec2, sampleIndex);
              if (!all(sourceSubPixel == destinationSubPixel)) {
                return vec4<f32>(1.0, 0.0, 0.0, 1.0);
              }
            }
            return vec4<f32>(0.0, 1.0, 0.0, 1.0);
          }`
      }),
      entryPoint: 'main',
      targets: [{ format: kColorFormat }]
    }
  });
  const bindGroup = t.device.createBindGroup({
    layout: renderPipelineForValidation.getBindGroupLayout(0),
    entries: [
    {
      binding: 0,
      resource: sourceTexture.createView()
    },
    {
      binding: 1,
      resource: destinationTexture.createView()
    }]

  });
  const expectedOutputTexture = t.createTextureTracked({
    format: kColorFormat,
    size: textureSize,
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT
  });
  const validationEncoder = t.device.createCommandEncoder();
  const renderPassForValidation = validationEncoder.beginRenderPass({
    colorAttachments: [
    {
      view: expectedOutputTexture.createView(),
      clearValue: [1.0, 0.0, 0.0, 1.0],
      loadOp: 'clear',
      storeOp: 'store'
    }]

  });
  renderPassForValidation.setPipeline(renderPipelineForValidation);
  renderPassForValidation.setBindGroup(0, bindGroup);
  renderPassForValidation.draw(6);
  renderPassForValidation.end();
  t.queue.submit([validationEncoder.finish()]);

  t.expectSingleColor(expectedOutputTexture, 'rgba8unorm', {
    size: [textureSize[0], textureSize[1], textureSize[2]],
    exp: { R: 0.0, G: 1.0, B: 0.0, A: 1.0 }
  });
});

g.test('copy_multisampled_depth').
desc(
  `
  Validate the correctness of copyTextureToTexture() with multisampled depth formats.

  - Initialize the source texture with a triangle in a render pass.
  - Copy from the source texture into the destination texture with CopyTextureToTexture().
  - Validate the content in the destination texture with the depth comparison function 'equal'.
  - Note that in current WebGPU SPEC the mipmap level count and array layer count of a multisampled
    texture can only be 1.
  `
).
params((u) =>
u.combine('format', kDepthStencilFormats).filter((t) => isDepthTextureFormat(t.format))
).
fn((t) => {
  const { format } = t.params;

  t.skipIf(t.isCompatibility, 'multisample textures are not copyable in compatibility mode');
  t.skipIfTextureFormatNotSupported(format);

  const textureSize = [32, 16, 1];
  const kSampleCount = 4;

  const sourceTexture = t.createTextureTracked({
    format,
    size: textureSize,
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    sampleCount: kSampleCount
  });
  const destinationTexture = t.createTextureTracked({
    format,
    size: textureSize,
    usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    sampleCount: kSampleCount
  });

  const vertexState = {
    module: t.device.createShaderModule({
      code: `
          @vertex
          fn main(@builtin(vertex_index) VertexIndex : u32)-> @builtin(position) vec4<f32> {
            var pos : array<vec3<f32>, 6> = array<vec3<f32>, 6>(
                vec3<f32>(-1.0,  1.0, 0.5),
                vec3<f32>(-1.0, -1.0, 0.0),
                vec3<f32>( 1.0,  1.0, 1.0),
                vec3<f32>(-1.0, -1.0, 0.0),
                vec3<f32>( 1.0,  1.0, 1.0),
                vec3<f32>( 1.0, -1.0, 0.5));
            return vec4<f32>(pos[VertexIndex], 1.0);
          }`
    }),
    entryPoint: 'main'
  };

  // Initialize the depth aspect of source texture with a draw call
  const renderPipelineForInit = t.device.createRenderPipeline({
    layout: 'auto',
    vertex: vertexState,
    depthStencil: {
      format,
      depthCompare: 'always',
      depthWriteEnabled: true
    },
    multisample: {
      count: kSampleCount
    }
  });

  const encoderForInit = t.device.createCommandEncoder();
  const renderPassForInit = encoderForInit.beginRenderPass({
    colorAttachments: [],
    depthStencilAttachment: {
      view: sourceTexture.createView(),
      depthClearValue: 0.0,
      depthLoadOp: 'clear',
      depthStoreOp: 'store',
      ...(isStencilTextureFormat(format) && {
        stencilLoadOp: 'clear',
        stencilStoreOp: 'store'
      })
    }
  });
  renderPassForInit.setPipeline(renderPipelineForInit);
  renderPassForInit.draw(6);
  renderPassForInit.end();
  t.queue.submit([encoderForInit.finish()]);

  // Do the texture-to-texture copy
  const copyEncoder = t.device.createCommandEncoder();
  copyEncoder.copyTextureToTexture(
    {
      texture: sourceTexture
    },
    {
      texture: destinationTexture
    },
    textureSize
  );
  t.queue.submit([copyEncoder.finish()]);

  // Verify the depth values in destinationTexture are what we expected with
  // depthCompareFunction == 'equal' and depthWriteEnabled == false in the render pipeline
  const kColorFormat = 'rgba8unorm';
  const renderPipelineForVerify = t.device.createRenderPipeline({
    layout: 'auto',
    vertex: vertexState,
    fragment: {
      module: t.device.createShaderModule({
        code: `
          @fragment
          fn main() -> @location(0) vec4<f32> {
            return vec4<f32>(0.0, 1.0, 0.0, 1.0);
          }`
      }),
      entryPoint: 'main',
      targets: [{ format: kColorFormat }]
    },
    depthStencil: {
      format,
      depthCompare: 'equal',
      depthWriteEnabled: false
    },
    multisample: {
      count: kSampleCount
    }
  });
  const multisampledColorTexture = t.createTextureTracked({
    format: kColorFormat,
    size: textureSize,
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    sampleCount: kSampleCount
  });
  const colorTextureAsResolveTarget = t.createTextureTracked({
    format: kColorFormat,
    size: textureSize,
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT
  });

  const encoderForVerify = t.device.createCommandEncoder();
  const renderPassForVerify = encoderForVerify.beginRenderPass({
    colorAttachments: [
    {
      view: multisampledColorTexture.createView(),
      clearValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
      loadOp: 'clear',
      storeOp: 'discard',
      resolveTarget: colorTextureAsResolveTarget.createView()
    }],

    depthStencilAttachment: {
      view: destinationTexture.createView(),
      depthLoadOp: 'load',
      depthStoreOp: 'store',
      ...(isStencilTextureFormat(format) && {
        stencilLoadOp: 'clear',
        stencilStoreOp: 'store'
      })
    }
  });
  renderPassForVerify.setPipeline(renderPipelineForVerify);
  renderPassForVerify.draw(6);
  renderPassForVerify.end();
  t.queue.submit([encoderForVerify.finish()]);

  t.expectSingleColor(colorTextureAsResolveTarget, kColorFormat, {
    size: [textureSize[0], textureSize[1], textureSize[2]],
    exp: { R: 0.0, G: 1.0, B: 0.0, A: 1.0 }
  });
});

g.test('copy_multisampled_stencil').
desc(
  `
  Validate the correctness of copyTextureToTexture() with multisampled stencil formats.
    `
).
unimplemented();