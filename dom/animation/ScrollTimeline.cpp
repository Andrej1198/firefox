/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollTimeline.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/DisplayPortUtils.h"
#include "mozilla/ElementAnimationData.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScrollContainerFrame.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"

namespace mozilla::dom {

// ---------------------------------
// Methods of ScrollTimeline
// ---------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(ScrollTimeline)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ScrollTimeline,
                                                AnimationTimeline)
  tmp->Teardown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource.mElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ScrollTimeline,
                                                  AnimationTimeline)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource.mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ScrollTimeline,
                                               AnimationTimeline)

ScrollTimeline::ScrollTimeline(Document* aDocument, const Scroller& aScroller,
                               StyleScrollAxis aAxis)
    : AnimationTimeline(aDocument->GetParentObject(),
                        aDocument->GetScopeObject()->GetRTPCallerType()),
      mDocument(aDocument),
      mSource(aScroller),
      mAxis(aAxis) {
  MOZ_ASSERT(aDocument);
  RegisterWithScrollSource();
}

/* static */ std::pair<const Element*, PseudoStyleRequest>
ScrollTimeline::FindNearestScroller(Element* aSubject,
                                    const PseudoStyleRequest& aPseudoRequest) {
  MOZ_ASSERT(aSubject);
  Element* subject = aSubject->GetPseudoElement(aPseudoRequest);
  Element* curr = subject->GetFlattenedTreeParentElement();
  Element* root = subject->OwnerDoc()->GetDocumentElement();
  while (curr && curr != root) {
    const ComputedStyle* style = Servo_Element_GetMaybeOutOfDateStyle(curr);
    MOZ_ASSERT(style, "The ancestor should be styled.");
    if (style->StyleDisplay()->IsScrollableOverflow()) {
      break;
    }
    curr = curr->GetFlattenedTreeParentElement();
  }
  // If there is no scroll container, we use root.
  if (!curr) {
    return {root, PseudoStyleRequest::NotPseudo()};
  }
  return AnimationUtils::GetElementPseudoPair(curr);
}

/* static */
already_AddRefed<ScrollTimeline> ScrollTimeline::MakeAnonymous(
    Document* aDocument, const NonOwningAnimationTarget& aTarget,
    StyleScrollAxis aAxis, StyleScroller aScroller) {
  MOZ_ASSERT(aTarget);
  Scroller scroller;
  switch (aScroller) {
    case StyleScroller::Root:
      scroller = Scroller::Root(aTarget.mElement->OwnerDoc());
      break;

    case StyleScroller::Nearest: {
      auto [element, pseudo] =
          FindNearestScroller(aTarget.mElement, aTarget.mPseudoRequest);
      scroller = Scroller::Nearest(const_cast<Element*>(element), pseudo.mType);
      break;
    }
    case StyleScroller::SelfElement:
      scroller = Scroller::Self(aTarget.mElement, aTarget.mPseudoRequest.mType);
      break;
  }

  // Each use of scroll() corresponds to its own instance of ScrollTimeline in
  // the Web Animations API, even if multiple elements use scroll() to refer to
  // the same scroll container with the same arguments.
  // https://drafts.csswg.org/scroll-animations-1/#scroll-notation
  return MakeAndAddRef<ScrollTimeline>(aDocument, scroller, aAxis);
}

/* static*/
already_AddRefed<ScrollTimeline> ScrollTimeline::MakeNamed(
    Document* aDocument, Element* aReferenceElement,
    const PseudoStyleRequest& aPseudoRequest,
    const StyleScrollTimeline& aStyleTimeline) {
  MOZ_ASSERT(NS_IsMainThread());

  Scroller scroller = Scroller::Named(aReferenceElement, aPseudoRequest.mType);
  return MakeAndAddRef<ScrollTimeline>(aDocument, std::move(scroller),
                                       aStyleTimeline.GetAxis());
}

Nullable<TimeDuration> ScrollTimeline::GetCurrentTimeAsDuration() const {
  // If no layout box, this timeline is inactive.
  if (!mSource || !mSource.mElement->GetPrimaryFrame()) {
    return nullptr;
  }

  // if this is not a scroller container, this timeline is inactive.
  const ScrollContainerFrame* scrollContainerFrame = GetScrollContainerFrame();
  if (!scrollContainerFrame) {
    return nullptr;
  }

  const auto orientation = Axis();

  // If there is no scrollable overflow, then the ScrollTimeline is inactive.
  // https://drafts.csswg.org/scroll-animations-1/#scrolltimeline-interface
  if (!scrollContainerFrame->GetAvailableScrollingDirections().contains(
          orientation)) {
    return nullptr;
  }

  const bool isHorizontal = orientation == layers::ScrollDirection::eHorizontal;
  const nsPoint& scrollPosition = scrollContainerFrame->GetScrollPosition();
  const Maybe<ScrollOffsets>& offsets =
      ComputeOffsets(scrollContainerFrame, orientation);
  if (!offsets) {
    return nullptr;
  }

  // Note: For RTL, scrollPosition.x or scrollPosition.y may be negative,
  // e.g. the range of its value is [0, -range], so we have to use the
  // absolute value.
  nscoord position =
      std::abs(isHorizontal ? scrollPosition.x : scrollPosition.y);
  double progress = static_cast<double>(position - offsets->mStart) /
                    static_cast<double>(offsets->mEnd - offsets->mStart);
  return TimeDuration::FromMilliseconds(progress *
                                        PROGRESS_TIMELINE_DURATION_MILLISEC);
}

layers::ScrollDirection ScrollTimeline::Axis() const {
  MOZ_ASSERT(mSource && mSource.mElement->GetPrimaryFrame());

  const WritingMode wm = mSource.mElement->GetPrimaryFrame()->GetWritingMode();
  return mAxis == StyleScrollAxis::X ||
                 (!wm.IsVertical() && mAxis == StyleScrollAxis::Inline) ||
                 (wm.IsVertical() && mAxis == StyleScrollAxis::Block)
             ? layers::ScrollDirection::eHorizontal
             : layers::ScrollDirection::eVertical;
}

StyleOverflow ScrollTimeline::SourceScrollStyle() const {
  MOZ_ASSERT(mSource && mSource.mElement->GetPrimaryFrame());

  const ScrollContainerFrame* scrollContainerFrame = GetScrollContainerFrame();
  MOZ_ASSERT(scrollContainerFrame);

  const ScrollStyles scrollStyles = scrollContainerFrame->GetScrollStyles();

  return Axis() == layers::ScrollDirection::eHorizontal
             ? scrollStyles.mHorizontal
             : scrollStyles.mVertical;
}

bool ScrollTimeline::APZIsActiveForSource() const {
  MOZ_ASSERT(mSource);
  return gfxPlatform::AsyncPanZoomEnabled() &&
         !nsLayoutUtils::ShouldDisableApzForElement(mSource.mElement) &&
         DisplayPortUtils::HasNonMinimalNonZeroDisplayPort(mSource.mElement);
}

bool ScrollTimeline::ScrollingDirectionIsAvailable() const {
  const ScrollContainerFrame* scrollContainerFrame = GetScrollContainerFrame();
  MOZ_ASSERT(scrollContainerFrame);
  return scrollContainerFrame->GetAvailableScrollingDirections().contains(
      Axis());
}

void ScrollTimeline::ReplacePropertiesWith(
    const Element* aReferenceElement, const PseudoStyleRequest& aPseudoRequest,
    const StyleScrollTimeline& aNew) {
  MOZ_ASSERT(aReferenceElement == mSource.mElement &&
             aPseudoRequest.mType == mSource.mPseudoType);
  mAxis = aNew.GetAxis();

  for (auto* anim = mAnimationOrder.getFirst(); anim;
       anim = static_cast<LinkedListElement<Animation>*>(anim)->getNext()) {
    MOZ_ASSERT(anim->GetTimeline() == this);
    // Set this so we just PostUpdate() for this animation.
    anim->SetTimeline(this);
  }
}

Maybe<ScrollTimeline::ScrollOffsets> ScrollTimeline::ComputeOffsets(
    const ScrollContainerFrame* aScrollContainerFrame,
    layers::ScrollDirection aOrientation) const {
  const nsRect& scrollRange = aScrollContainerFrame->GetScrollRange();
  nscoord range = aOrientation == layers::ScrollDirection::eHorizontal
                      ? scrollRange.width
                      : scrollRange.height;
  MOZ_ASSERT(range > 0);
  return Some(ScrollOffsets{0, range});
}

void ScrollTimeline::RegisterWithScrollSource() {
  if (!mSource) {
    return;
  }

  auto& scheduler = ProgressTimelineScheduler::Ensure(
      mSource.mElement, PseudoStyleRequest(mSource.mPseudoType));
  scheduler.AddTimeline(this);
}

void ScrollTimeline::UnregisterFromScrollSource() {
  if (!mSource) {
    return;
  }

  auto* scheduler = ProgressTimelineScheduler::Get(
      mSource.mElement, PseudoStyleRequest(mSource.mPseudoType));
  if (!scheduler) {
    return;
  }

  // If we are trying to unregister this timeline from the scheduler in
  // ProgressTimelineScheduler::ScheduleAnimations(), we have to unregister this
  // after we finish the scheduling, to avoid mutating the hashtset
  // and destroying the scheduler here.
  if (scheduler->IsInScheduling()) {
    mState = TimelineState::PendingRemove;
    return;
  }

  scheduler->RemoveTimeline(this);
  if (scheduler->IsEmpty()) {
    ProgressTimelineScheduler::Destroy(mSource.mElement,
                                       PseudoStyleRequest(mSource.mPseudoType));
  }
}

const ScrollContainerFrame* ScrollTimeline::GetScrollContainerFrame() const {
  if (!mSource) {
    return nullptr;
  }

  switch (mSource.mType) {
    case Scroller::Type::Root:
      if (const PresShell* presShell =
              mSource.mElement->OwnerDoc()->GetPresShell()) {
        return presShell->GetRootScrollContainerFrame();
      }
      return nullptr;
    case Scroller::Type::Nearest:
    case Scroller::Type::Name:
    case Scroller::Type::Self:
      return nsLayoutUtils::FindScrollContainerFrameFor(mSource.mElement);
  }

  MOZ_ASSERT_UNREACHABLE("Unsupported scroller type");
  return nullptr;
}

void ScrollTimeline::NotifyAnimationContentVisibilityChanged(
    Animation* aAnimation, bool aIsVisible) {
  AnimationTimeline::NotifyAnimationContentVisibilityChanged(aAnimation,
                                                             aIsVisible);
  if (mAnimationOrder.isEmpty()) {
    UnregisterFromScrollSource();
  } else {
    RegisterWithScrollSource();
  }
}

// ------------------------------------
// Methods of ProgressTimelineScheduler
// ------------------------------------
/* static */ ProgressTimelineScheduler* ProgressTimelineScheduler::Get(
    const Element* aElement, const PseudoStyleRequest& aPseudoRequest) {
  MOZ_ASSERT(aElement);
  auto* data = aElement->GetAnimationData();
  if (!data) {
    return nullptr;
  }

  return data->GetProgressTimelineScheduler(aPseudoRequest);
}

/* static */ ProgressTimelineScheduler& ProgressTimelineScheduler::Ensure(
    Element* aElement, const PseudoStyleRequest& aPseudoRequest) {
  MOZ_ASSERT(aElement);
  return aElement->EnsureAnimationData().EnsureProgressTimelineScheduler(
      aPseudoRequest);
}

/* static */
void ProgressTimelineScheduler::Destroy(
    const Element* aElement, const PseudoStyleRequest& aPseudoRequest) {
  auto* data = aElement->GetAnimationData();
  MOZ_ASSERT(data);
  data->ClearProgressTimelineScheduler(aPseudoRequest);
}

/* static */
void ProgressTimelineScheduler::ScheduleAnimations(
    const Element* aElement, const PseudoStyleRequest& aRequest) {
  auto* scheduler = Get(aElement, aRequest);
  if (!scheduler) {
    return;
  }

  // Note: We only need to handle the removal. It's impossible to iterate the
  // non-existing timelines and add them into the hashset.
  nsTArray<ScrollTimeline*> timelinesToBeRemoved;

  scheduler->mIsInScheduling = true;
  for (auto iter = scheduler->mTimelines.iter(); !iter.done(); iter.next()) {
    auto* timeline = iter.get();
    const auto state = timeline->ScheduleAnimations();
    if (state == ScrollTimeline::TimelineState::PendingRemove) {
      timelinesToBeRemoved.AppendElement(timeline);
    }
  }
  MOZ_ASSERT(Get(aElement, aRequest), "Make sure the scheduler still exists");
  scheduler->mIsInScheduling = false;

  // In the common case, this array is empty. It could be non-empty only when
  // we change the content-visibility in their Tick()s.
  for (auto* timeline : timelinesToBeRemoved) {
    timeline->ResetState();
    scheduler->RemoveTimeline(timeline);
  }

  if (scheduler->IsEmpty()) {
    ProgressTimelineScheduler::Destroy(aElement, aRequest);
  }
}

}  // namespace mozilla::dom
