/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbortSignal_h
#define mozilla_dom_AbortSignal_h

#include "mozilla/RefPtr.h"
#include "mozilla/dom/AbortFollower.h"
#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla::dom {

// AbortSignal the spec concept includes the concept of a child signal
// "following" a parent signal -- internally, adding abort steps to the parent
// signal that will then signal abort on the child signal -- to propagate
// signaling abort from one signal to another.  See
// <https://dom.spec.whatwg.org/#abortsignal-follow>.
//
// This requires that AbortSignal also inherit from AbortFollower.
//
// This ability to follow isn't directly exposed in the DOM; as of this writing
// it appears only to be used internally in the Fetch API.  It might be a good
// idea to split AbortSignal into an implementation that can follow, and an
// implementation that can't, to provide this complexity only when it's needed.
class AbortSignal : public DOMEventTargetHelper, public AbortSignalImpl {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(AbortSignal,
                                                         DOMEventTargetHelper)

  static already_AddRefed<AbortSignal> Create(nsIGlobalObject* aGlobalObject,
                                              SignalAborted aAborted,
                                              JS::Handle<JS::Value> aReason);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  IMPL_EVENT_HANDLER(abort);

  static already_AddRefed<AbortSignal> Abort(GlobalObject& aGlobal,
                                             JS::Handle<JS::Value> aReason);

  static already_AddRefed<AbortSignal> Timeout(GlobalObject& aGlobal,
                                               uint64_t aMilliseconds,
                                               ErrorResult& aRv);

  static already_AddRefed<AbortSignal> Any(
      GlobalObject& aGlobal,
      const Sequence<OwningNonNull<AbortSignal>>& aSignals);
  static already_AddRefed<AbortSignal> Any(
      nsIGlobalObject* aGlobal,
      const Span<const OwningNonNull<AbortSignal>>& aSignals,
      FunctionRef<already_AddRefed<AbortSignal>(nsIGlobalObject* aGlobal)>
          aCreateResultSignal);

  void ThrowIfAborted(JSContext* aCx, ErrorResult& aRv);

  virtual bool IsTaskSignal() const { return false; }

  bool Dependent() const;

 protected:
  AbortSignal(nsIGlobalObject* aGlobalObject, SignalAborted aAborted,
              JS::Handle<JS::Value> aReason);

  void Init();

  virtual ~AbortSignal();

  void MakeDependentOn(AbortSignal* aSignal);

  void SignalAbortWithDependents() override;

  void RunAbortSteps() override;

  nsTArray<WeakPtr<AbortSignal>> mSourceSignals;
  nsTArray<RefPtr<AbortSignal>> mDependentSignals;

  bool mDependent;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AbortSignal_h
