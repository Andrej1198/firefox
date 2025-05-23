/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLIPDL_H_
#define WEBGLIPDL_H_

#include "gfxTypes.h"
#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/dom/BindingIPCUtils.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/ParamTraits_IsEnumCase.h"
#include "mozilla/ParamTraits_STL.h"
#include "mozilla/ParamTraits_TiedFields.h"
#include "WebGLTypes.h"

namespace mozilla {
namespace webgl {

// TODO: This should probably replace Shmem, or at least this should move to
// ipc/glue.

class RaiiShmem final {
  RefPtr<mozilla::ipc::ActorLifecycleProxy> mWeakRef;
  mozilla::ipc::Shmem mShmem = {};

 public:
  /// Returns zeroed data.
  static RaiiShmem Alloc(mozilla::ipc::IProtocol* const allocator,
                         const size_t size) {
    mozilla::ipc::Shmem shmem;
    if (!allocator->AllocShmem(size, &shmem)) return {};
    return {allocator, shmem};
  }

  static RaiiShmem AllocUnsafe(mozilla::ipc::IProtocol* const allocator,
                               const size_t size) {
    mozilla::ipc::Shmem shmem;
    if (!allocator->AllocUnsafeShmem(size, &shmem)) return {};
    return {allocator, shmem};
  }

  // -

  RaiiShmem() = default;

  RaiiShmem(mozilla::ipc::IProtocol* const allocator,
            const mozilla::ipc::Shmem& shmem) {
    if (!allocator || !allocator->CanSend()) {
      return;
    }

    // Shmems are handled by the top-level, so use that or we might leak after
    // the actor dies.
    mWeakRef = allocator->ToplevelProtocol()->GetLifecycleProxy();
    mShmem = shmem;
    if (!mWeakRef || !mWeakRef->Get() || !IsShmem()) {
      reset();
    }
  }

  void reset() {
    if (IsShmem()) {
      const auto& allocator = mWeakRef->Get();
      if (allocator) {
        allocator->DeallocShmem(mShmem);
      }
    }
    mWeakRef = nullptr;
    mShmem = {};
  }

  ~RaiiShmem() { reset(); }

  // -

  RaiiShmem(RaiiShmem&& rhs) { *this = std::move(rhs); }
  RaiiShmem& operator=(RaiiShmem&& rhs) {
    reset();
    mWeakRef = rhs.mWeakRef;
    mShmem = rhs.Extract();
    return *this;
  }

  // -

  bool IsShmem() const { return mShmem.IsReadable(); }

  explicit operator bool() const { return IsShmem(); }

  // -

  const auto& Shmem() const {
    MOZ_ASSERT(IsShmem());
    return mShmem;
  }

  Range<uint8_t> ByteRange() const {
    if (!IsShmem()) {
      return {};
    }
    return {mShmem.get<uint8_t>(), mShmem.Size<uint8_t>()};
  }

  mozilla::ipc::Shmem Extract() {
    auto ret = mShmem;
    mShmem = {};
    reset();
    return ret;
  }
};

using Int32Vector = std::vector<int32_t>;

}  // namespace webgl

namespace ipc {

template <>
struct IPDLParamTraits<mozilla::webgl::FrontBufferSnapshotIpc> final {
  using T = mozilla::webgl::FrontBufferSnapshotIpc;

  static void Write(IPC::MessageWriter* const writer, IProtocol* actor, T& in) {
    WriteParam(writer, in.surfSize);
    WriteParam(writer, in.byteStride);
    WriteIPDLParam(writer, actor, std::move(in.shmem));
  }

  static bool Read(IPC::MessageReader* const reader, IProtocol* actor,
                   T* const out) {
    return ReadParam(reader, &out->surfSize) &&
           ReadParam(reader, &out->byteStride) &&
           ReadIPDLParam(reader, actor, &out->shmem);
  }
};

// -

template <>
struct IPDLParamTraits<mozilla::webgl::ReadPixelsResultIpc> final {
  using T = mozilla::webgl::ReadPixelsResultIpc;

  static void Write(IPC::MessageWriter* const writer, IProtocol* actor, T& in) {
    WriteParam(writer, in.subrect);
    WriteParam(writer, in.byteStride);
    WriteIPDLParam(writer, actor, std::move(in.shmem));
  }

  static bool Read(IPC::MessageReader* const reader, IProtocol* actor,
                   T* const out) {
    return ReadParam(reader, &out->subrect) &&
           ReadParam(reader, &out->byteStride) &&
           ReadIPDLParam(reader, actor, &out->shmem);
  }
};

// -

template <>
struct IPDLParamTraits<mozilla::webgl::TexUnpackBlobDesc> final {
  using T = mozilla::webgl::TexUnpackBlobDesc;

  static void Write(IPC::MessageWriter* const writer, IProtocol* actor,
                    T&& in) {
    WriteParam(writer, in.imageTarget);
    WriteParam(writer, in.size);
    WriteParam(writer, in.srcAlphaType);
    MOZ_RELEASE_ASSERT(!in.cpuData);
    MOZ_RELEASE_ASSERT(!in.pboOffset);
    WriteParam(writer, in.structuredSrcSize);
    MOZ_RELEASE_ASSERT(!in.image);
    WriteIPDLParam(writer, actor, std::move(in.sd));
    MOZ_RELEASE_ASSERT(!in.sourceSurf);
    WriteParam(writer, in.unpacking);
    WriteParam(writer, in.applyUnpackTransforms);
  }

  static bool Read(IPC::MessageReader* const reader, IProtocol* actor,
                   T* const out) {
    return ReadParam(reader, &out->imageTarget) &&
           ReadParam(reader, &out->size) &&
           ReadParam(reader, &out->srcAlphaType) &&
           ReadParam(reader, &out->structuredSrcSize) &&
           ReadIPDLParam(reader, actor, &out->sd) &&
           ReadParam(reader, &out->unpacking) &&
           ReadParam(reader, &out->applyUnpackTransforms);
  }
};

}  // namespace ipc

namespace webgl {
using Int32Vector = std::vector<int32_t>;
}  // namespace webgl
}  // namespace mozilla

namespace IPC {

// -

template <class U, size_t PaddedSize>
struct ParamTraits<mozilla::webgl::Padded<U, PaddedSize>> final {
  using T = mozilla::webgl::Padded<U, PaddedSize>;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, *in);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &**out);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::AttribBaseType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::webgl::AttribBaseType,
          mozilla::webgl::AttribBaseType::Boolean,
          mozilla::webgl::AttribBaseType::Uint> {};

template <>
struct ParamTraits<mozilla::webgl::ContextLossReason>
    : public ContiguousEnumSerializerInclusive<
          mozilla::webgl::ContextLossReason,
          mozilla::webgl::ContextLossReason::None,
          mozilla::webgl::ContextLossReason::Guilty> {};

template <>
struct ParamTraits<gfxAlphaType>
    : public ContiguousEnumSerializerInclusive<
          gfxAlphaType, gfxAlphaType::Opaque, gfxAlphaType::NonPremult> {};

template <>
struct ParamTraits<mozilla::dom::WebGLPowerPreference> final
    : public mozilla::dom::WebIDLEnumSerializer<
          mozilla::dom::WebGLPowerPreference> {};

template <>
struct ParamTraits<mozilla::dom::PredefinedColorSpace> final
    : public mozilla::dom::WebIDLEnumSerializer<
          mozilla::dom::PredefinedColorSpace> {};

// -
// ParamTraits_IsEnumCase

#define USE_IS_ENUM_CASE(T) \
  template <>               \
  struct ParamTraits<T> : public ParamTraits_IsEnumCase<T> {};

USE_IS_ENUM_CASE(mozilla::webgl::OptionalRenderableFormatBits)

#undef USE_IS_ENUM_CASE

// -
// ParamTraits_TiedFields

template <>
struct ParamTraits<mozilla::webgl::InitContextDesc> final
    : public ParamTraits_TiedFields<mozilla::webgl::InitContextDesc> {};

template <>
struct ParamTraits<mozilla::WebGLContextOptions> final
    : public ParamTraits_TiedFields<mozilla::WebGLContextOptions> {};

template <>
struct ParamTraits<mozilla::webgl::OpaqueFramebufferOptions> final
    : public ParamTraits_TiedFields<mozilla::webgl::OpaqueFramebufferOptions> {
};

template <>
struct ParamTraits<mozilla::webgl::ShaderPrecisionFormat> final
    : public ParamTraits_TiedFields<mozilla::webgl::ShaderPrecisionFormat> {};

// -

template <>
struct ParamTraits<mozilla::gl::GLVendor>
    : public ContiguousEnumSerializerInclusive<mozilla::gl::GLVendor,
                                               mozilla::gl::GLVendor::Intel,
                                               mozilla::gl::kHighestGLVendor> {
};

template <typename U>
struct ParamTraits<mozilla::webgl::EnumMask<U>> final
    : public ParamTraits_TiedFields<mozilla::webgl::EnumMask<U>> {};

template <>
struct ParamTraits<mozilla::webgl::InitContextResult> final
    : public ParamTraits_TiedFields<mozilla::webgl::InitContextResult> {};

template <>
struct ParamTraits<mozilla::webgl::Limits> final
    : public ParamTraits_TiedFields<mozilla::webgl::Limits> {};

template <>
struct ParamTraits<mozilla::webgl::PixelPackingState> final
    : public ParamTraits_TiedFields<mozilla::webgl::PixelPackingState> {};
template <>
struct ParamTraits<mozilla::webgl::PixelUnpackStateWebgl> final
    : public ParamTraits_TiedFields<mozilla::webgl::PixelUnpackStateWebgl> {};

// -

template <>
struct ParamTraits<mozilla::webgl::ReadPixelsDesc> final {
  using T = mozilla::webgl::ReadPixelsDesc;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.srcOffset);
    WriteParam(writer, in.size);
    WriteParam(writer, in.pi);
    WriteParam(writer, in.packState);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &out->srcOffset) &&
           ReadParam(reader, &out->size) && ReadParam(reader, &out->pi) &&
           ReadParam(reader, &out->packState);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::PackingInfo> final {
  using T = mozilla::webgl::PackingInfo;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.format);
    WriteParam(writer, in.type);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &out->format) && ReadParam(reader, &out->type);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::CompileResult> final {
  using T = mozilla::webgl::CompileResult;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.pending);
    WriteParam(writer, in.log);
    WriteParam(writer, in.translatedSource);
    WriteParam(writer, in.success);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &out->pending) && ReadParam(reader, &out->log) &&
           ReadParam(reader, &out->translatedSource) &&
           ReadParam(reader, &out->success);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::LinkResult> final {
  using T = mozilla::webgl::LinkResult;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.pending);
    WriteParam(writer, in.log);
    WriteParam(writer, in.success);
    WriteParam(writer, in.active);
    WriteParam(writer, in.tfBufferMode);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &out->pending) && ReadParam(reader, &out->log) &&
           ReadParam(reader, &out->success) &&
           ReadParam(reader, &out->active) &&
           ReadParam(reader, &out->tfBufferMode);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::LinkActiveInfo> final {
  using T = mozilla::webgl::LinkActiveInfo;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.activeAttribs);
    WriteParam(writer, in.activeUniforms);
    WriteParam(writer, in.activeUniformBlocks);
    WriteParam(writer, in.activeTfVaryings);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &out->activeAttribs) &&
           ReadParam(reader, &out->activeUniforms) &&
           ReadParam(reader, &out->activeUniformBlocks) &&
           ReadParam(reader, &out->activeTfVaryings);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::ActiveInfo> final {
  using T = mozilla::webgl::ActiveInfo;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.elemType);
    WriteParam(writer, in.elemCount);
    WriteParam(writer, in.name);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &out->elemType) &&
           ReadParam(reader, &out->elemCount) && ReadParam(reader, &out->name);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::ActiveAttribInfo> final {
  using T = mozilla::webgl::ActiveAttribInfo;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, static_cast<const mozilla::webgl::ActiveInfo&>(in));
    WriteParam(writer, in.location);
    WriteParam(writer, in.baseType);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, static_cast<mozilla::webgl::ActiveInfo*>(out)) &&
           ReadParam(reader, &out->location) &&
           ReadParam(reader, &out->baseType);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::ActiveUniformInfo> final {
  using T = mozilla::webgl::ActiveUniformInfo;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, static_cast<const mozilla::webgl::ActiveInfo&>(in));
    WriteParam(writer, in.locByIndex);
    WriteParam(writer, in.block_index);
    WriteParam(writer, in.block_offset);
    WriteParam(writer, in.block_arrayStride);
    WriteParam(writer, in.block_matrixStride);
    WriteParam(writer, in.block_isRowMajor);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, static_cast<mozilla::webgl::ActiveInfo*>(out)) &&
           ReadParam(reader, &out->locByIndex) &&
           ReadParam(reader, &out->block_index) &&
           ReadParam(reader, &out->block_offset) &&
           ReadParam(reader, &out->block_arrayStride) &&
           ReadParam(reader, &out->block_matrixStride) &&
           ReadParam(reader, &out->block_isRowMajor);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::ActiveUniformBlockInfo> final {
  using T = mozilla::webgl::ActiveUniformBlockInfo;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.name);
    WriteParam(writer, in.dataSize);
    WriteParam(writer, in.activeUniformIndices);
    WriteParam(writer, in.referencedByVertexShader);
    WriteParam(writer, in.referencedByFragmentShader);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &out->name) && ReadParam(reader, &out->dataSize) &&
           ReadParam(reader, &out->activeUniformIndices) &&
           ReadParam(reader, &out->referencedByVertexShader) &&
           ReadParam(reader, &out->referencedByFragmentShader);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::GetUniformData> final {
  using T = mozilla::webgl::GetUniformData;

  static void Write(MessageWriter* const writer, const T& in) {
    ParamTraits<decltype(in.data)>::Write(writer, in.data);
    WriteParam(writer, in.type);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ParamTraits<decltype(out->data)>::Read(reader, &out->data) &&
           ReadParam(reader, &out->type);
  }
};

// -

template <typename U>
struct ParamTraits<mozilla::avec2<U>> final {
  using T = mozilla::avec2<U>;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.x);
    WriteParam(writer, in.y);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &out->x) && ReadParam(reader, &out->y);
  }
};

// -

template <typename U>
struct ParamTraits<mozilla::avec3<U>> final {
  using T = mozilla::avec3<U>;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.x);
    WriteParam(writer, in.y);
    WriteParam(writer, in.z);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    return ReadParam(reader, &out->x) && ReadParam(reader, &out->y) &&
           ReadParam(reader, &out->z);
  }
};

}  // namespace IPC

#endif
