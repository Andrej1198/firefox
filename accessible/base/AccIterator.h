/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccIterator_h__
#define mozilla_a11y_AccIterator_h__

#include "Filters.h"
#include "mozilla/a11y/DocAccessible.h"
#include "nsTArray.h"

#include <memory>

class nsITreeView;

namespace mozilla {
namespace dom {
class Element;
}

namespace a11y {
class DocAccessibleParent;

/**
 * AccIterable is a basic interface for iterators over accessibles.
 */
class AccIterable {
 public:
  virtual ~AccIterable() {}
  virtual Accessible* Next() = 0;

 private:
  friend class Relation;
  std::unique_ptr<AccIterable> mNextIter;
};

/**
 * Allows to iterate through accessible children or subtree complying with
 * filter function.
 */
class AccIterator : public AccIterable {
 public:
  AccIterator(const LocalAccessible* aRoot, filters::FilterFuncPtr aFilterFunc);
  virtual ~AccIterator();

  /**
   * Return next accessible complying with filter function. Return the first
   * accessible for the first time.
   */
  virtual LocalAccessible* Next() override;

 private:
  AccIterator();
  AccIterator(const AccIterator&);
  AccIterator& operator=(const AccIterator&);

  struct IteratorState {
    explicit IteratorState(const LocalAccessible* aParent,
                           IteratorState* mParentState = nullptr);

    const LocalAccessible* mParent;
    int32_t mIndex;
    IteratorState* mParentState;
  };

  filters::FilterFuncPtr mFilterFunc;
  IteratorState* mState;
};

/**
 * Allows to traverse through related accessibles that are pointing to the given
 * dependent accessible by relation attribute. This is typically used to query
 * implicit reverse relations; e.g. calculating the LABEL_FOR relation for a
 * label where that label was referenced using aria-labelledby.
 */
class RelatedAccIterator : public AccIterable {
 public:
  /**
   * Constructor.
   *
   * @param aDocument         [in] the document accessible the related
   * &                         accessibles belong to.
   * @param aDependentContent [in] the content of dependent accessible that
   *                           relations were requested for
   * @param aRelAttr          [in] relation attribute that relations are
   *                           pointed by, null for all relations
   */
  RelatedAccIterator(DocAccessible* aDocument, nsIContent* aDependentContent,
                     nsAtom* aRelAttr);

  virtual ~RelatedAccIterator() {}

  /**
   * Return next related accessible for the given dependent accessible.
   */
  virtual LocalAccessible* Next() override;

 private:
  RelatedAccIterator();
  RelatedAccIterator(const RelatedAccIterator&);
  RelatedAccIterator& operator=(const RelatedAccIterator&);

  DocAccessible* mDocument;
  nsIContent* mDependentContent;
  nsAtom* mRelAttr;
  DocAccessible::AttrRelProviders* mProviders;
  uint32_t mIndex;
  bool mIsWalkingDependentElements;
};

/**
 * Used to iterate through HTML labels associated with the given accessible.
 */
class HTMLLabelIterator : public AccIterable {
 public:
  enum LabelFilter { eAllLabels, eSkipAncestorLabel };

  HTMLLabelIterator(DocAccessible* aDocument,
                    const LocalAccessible* aAccessible,
                    LabelFilter aFilter = eAllLabels);

  virtual ~HTMLLabelIterator() {}

  /**
   * Return next label accessible associated with the given element.
   */
  virtual LocalAccessible* Next() override;

 private:
  HTMLLabelIterator();
  HTMLLabelIterator(const HTMLLabelIterator&);
  HTMLLabelIterator& operator=(const HTMLLabelIterator&);

  bool IsLabel(LocalAccessible* aLabel);

  RelatedAccIterator mRelIter;
  // XXX: replace it on weak reference (bug 678429), it's safe to use raw
  // pointer now because iterators life cycle is short.
  const LocalAccessible* mAcc;
  LabelFilter mLabelFilter;
};

/**
 * Used to iterate through HTML outputs associated with the given element.
 */
class HTMLOutputIterator : public AccIterable {
 public:
  HTMLOutputIterator(DocAccessible* aDocument, nsIContent* aElement);
  virtual ~HTMLOutputIterator() {}

  /**
   * Return next output accessible associated with the given element.
   */
  virtual LocalAccessible* Next() override;

 private:
  HTMLOutputIterator();
  HTMLOutputIterator(const HTMLOutputIterator&);
  HTMLOutputIterator& operator=(const HTMLOutputIterator&);

  RelatedAccIterator mRelIter;
};

/**
 * Used to iterate through XUL labels associated with the given element.
 */
class XULLabelIterator : public AccIterable {
 public:
  XULLabelIterator(DocAccessible* aDocument, nsIContent* aElement);
  virtual ~XULLabelIterator() {}

  /**
   * Return next label accessible associated with the given element.
   */
  virtual LocalAccessible* Next() override;

 private:
  XULLabelIterator();
  XULLabelIterator(const XULLabelIterator&);
  XULLabelIterator& operator=(const XULLabelIterator&);

  RelatedAccIterator mRelIter;
};

/**
 * Used to iterate through XUL descriptions associated with the given element.
 */
class XULDescriptionIterator : public AccIterable {
 public:
  XULDescriptionIterator(DocAccessible* aDocument, nsIContent* aElement);
  virtual ~XULDescriptionIterator() {}

  /**
   * Return next description accessible associated with the given element.
   */
  virtual LocalAccessible* Next() override;

 private:
  XULDescriptionIterator();
  XULDescriptionIterator(const XULDescriptionIterator&);
  XULDescriptionIterator& operator=(const XULDescriptionIterator&);

  RelatedAccIterator mRelIter;
};

/**
 * Used to iterate through elements referenced through explicitly set
 * attr-elements or IDs listed in a content attribute. Note, any method used to
 * iterate through IDs, elements, or accessibles moves iterator to next
 * position.
 */
class AssociatedElementsIterator : public AccIterable {
 public:
  AssociatedElementsIterator(DocAccessible* aDoc, nsIContent* aContent,
                             nsAtom* aIDRefsAttr);
  virtual ~AssociatedElementsIterator() {}

  /**
   * Return next ID.
   */
  const nsDependentSubstring NextID();

  /**
   * Return next element.
   */
  dom::Element* NextElem();

  /**
   * Return the element with the given ID.
   */
  static dom::Element* GetElem(nsIContent* aContent, const nsAString& aID);
  dom::Element* GetElem(const nsDependentSubstring& aID);

  // AccIterable
  virtual LocalAccessible* Next() override;

 private:
  AssociatedElementsIterator();
  AssociatedElementsIterator(const AssociatedElementsIterator&);
  AssociatedElementsIterator operator=(const AssociatedElementsIterator&);

  nsString mIDs;
  nsIContent* mContent;
  DocAccessible* mDoc;
  nsAString::index_type mCurrIdx;
  nsTArray<dom::Element*> mElements;
  uint32_t mElemIdx;
};

/**
 * Iterator that points to a single accessible returning it on the first call
 * to Next().
 */
class SingleAccIterator : public AccIterable {
 public:
  explicit SingleAccIterator(Accessible* aTarget) : mAcc(aTarget) {}
  virtual ~SingleAccIterator() {}

  virtual Accessible* Next() override;

 private:
  SingleAccIterator();
  SingleAccIterator(const SingleAccIterator&);
  SingleAccIterator& operator=(const SingleAccIterator&);

  Accessible* mAcc;
};

/**
 * Used to iterate items of the given item container.
 */
class ItemIterator : public AccIterable {
 public:
  explicit ItemIterator(const Accessible* aItemContainer)
      : mContainer(aItemContainer), mAnchor(nullptr) {}

  virtual Accessible* Next() override;

 private:
  ItemIterator() = delete;
  ItemIterator(const ItemIterator&) = delete;
  ItemIterator& operator=(const ItemIterator&) = delete;

  const Accessible* mContainer;
  Accessible* mAnchor;
};

/**
 * Used to iterate through XUL tree items of the same level.
 */
class XULTreeItemIterator : public AccIterable {
 public:
  XULTreeItemIterator(const XULTreeAccessible* aXULTree, nsITreeView* aTreeView,
                      int32_t aRowIdx);
  virtual ~XULTreeItemIterator() {}

  virtual LocalAccessible* Next() override;

 private:
  XULTreeItemIterator() = delete;
  XULTreeItemIterator(const XULTreeItemIterator&) = delete;
  XULTreeItemIterator& operator=(const XULTreeItemIterator&) = delete;

  const XULTreeAccessible* mXULTree;
  nsITreeView* mTreeView;
  int32_t mRowCount;
  int32_t mContainerLevel;
  int32_t mCurrRowIdx;
};

/**
 * Used to iterate through a sequence of RemoteAccessibles supplied as an array
 * of ids. Such id arrays are included in the RemoteAccessible cache.
 */
class RemoteAccIterator : public AccIterable {
 public:
  /**
   * Construct with a reference to an array owned somewhere else; e.g. a
   * RemoteAccessible cache.
   */
  RemoteAccIterator(const nsTArray<uint64_t>& aIds, DocAccessibleParent* aDoc)
      : mIds(aIds), mDoc(aDoc), mIndex(0) {}

  virtual ~RemoteAccIterator() = default;

  virtual Accessible* Next() override;

 private:
  const nsTArray<uint64_t>& mIds;
  DocAccessibleParent* mDoc;
  uint32_t mIndex;
};

}  // namespace a11y
}  // namespace mozilla

#endif
