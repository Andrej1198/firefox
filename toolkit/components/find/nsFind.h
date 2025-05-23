/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFind_h__
#define nsFind_h__

#include "nsIFind.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsINode.h"
#include "mozilla/intl/Segmenter.h"
#include "mozilla/RangeBoundary.h"

#define NS_FIND_CONTRACTID "@mozilla.org/embedcomp/rangefind;1"

#define NS_FIND_CID \
  {0x471f4944, 0x1dd2, 0x11b2, {0x87, 0xac, 0x90, 0xbe, 0x0a, 0x51, 0xd6, 0x09}}

class nsFind : public nsIFind {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIFIND
  NS_DECL_CYCLE_COLLECTION_CLASS(nsFind)

  // The IDL interface allows to toggle the "find entire words" search mode
  // by calling `SetEntireWord()`.
  // Internally, it is possible to specify word boundaries at the beginning
  // and/or the end of the search pattern.
  // `GetEntireWord()` returns true if `mWordStartBounded` and `mWordEndBounded`
  // are true.
  void SetWordStartBounded(bool aWordStartBounded) {
    mWordStartBounded = aWordStartBounded;
  }
  void SetWordEndBounded(bool aWordEndBounded) {
    mWordEndBounded = aWordEndBounded;
  }

  void SetNodeIndexCache(nsContentUtils::NodeIndexCache* aCache) {
    mNodeIndexCache = aCache;
  }

  already_AddRefed<nsRange> FindFromRangeBoundaries(
      const nsAString& aPatText, const mozilla::RangeBoundary& aStartPoint,
      const mozilla::RangeBoundary& aEndPoint);

 protected:
  virtual ~nsFind() = default;

  // Parameters set from the interface:
  bool mFindBackward = false;
  bool mCaseSensitive = false;
  bool mMatchDiacritics = false;

  bool mWordStartBounded = false;
  bool mWordEndBounded = false;
  mozilla::intl::WordBreakIteratorUtf16 mWordBreakIter{nullptr};
  nsContentUtils::NodeIndexCache* mNodeIndexCache = nullptr;
  struct State;
  class StateRestorer;

  // Extract a character from a string, handling surrogate pairs and
  // incrementing the index if a surrogate pair is encountered
  char32_t DecodeChar(const char16_t* t2b, int32_t* index) const;

  // Determine if a line break can occur between two characters
  //
  // This could be improved because some languages require more context than two
  // characters to determine where line breaks can occur
  bool BreakInBetween(char32_t x, char32_t y);

  // Get the first character from the next node (last if mFindBackward).
  //
  // This will mutate the state, but then restore it afterwards.
  char32_t PeekNextChar(State&, bool aAlreadyMatching) const;
};

#endif  // nsFind_h__
