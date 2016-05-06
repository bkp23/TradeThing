#include "heap.h"

Entry::Entry(Node *ptrNode, quint64 cost)
{
  this->ptrNode = ptrNode;
  this->cost    = cost;
  ptrChild     = NULL;
  ptrSibling   = NULL;
  ptrPrev      = NULL;
  used         = false;
}


Heap::Heap()
{
  ptrRoot = NULL;
}

Heap::Heap(int expectedSize)
{
  ptrRoot = NULL;
  allocations.reserve(expectedSize);
}

Heap::~Heap()
{
  // deallocate everything added via insert()
  while (!allocations.isEmpty())
    delete allocations.takeFirst();
}

bool Heap::isEmpty()
{
  return ptrRoot == NULL;
}


Entry* Heap::extractMin()
{
  Q_ASSERT(ptrRoot != NULL);
  Entry *ptrMinEntry = ptrRoot;
  Entry *ptrList = ptrRoot->ptrChild;

  ptrRoot->used = true;
  if (ptrList != NULL)
  {
    // The (new) root can't have any siblings, so we re-merge them
    while (ptrList->ptrSibling != NULL)
    {
      Entry *ptrNextList = NULL;
      while (ptrList != NULL  &&  ptrList->ptrSibling != NULL)
      {
        Entry *a = ptrList;
        Entry *b = a->ptrSibling;
        ptrList = b->ptrSibling;

        // link a and b and add result to ptrNextList
        a->ptrSibling = b->ptrSibling = NULL;
        a = merge(a,b);
        a->ptrSibling = ptrNextList;
        ptrNextList = a;
      }
      if (ptrList == NULL)
        ptrList = ptrNextList;
      else
        ptrList->ptrSibling = ptrNextList;
    }
    ptrList->ptrPrev = NULL; // separate this from the old root
  }
  ptrRoot = ptrList;
  return ptrMinEntry; // return the old root (the smallest value)
}


// Create a new entry and merge it into root
// The insert method returns the new Entry object, so that the user can
// later call the decreaseCost method.
Entry* Heap::insert(Node *ptrNode, quint64 cost)
{
  Entry *ptrEntry = new Entry(ptrNode, cost);
  ptrRoot = (ptrRoot==NULL) ? ptrEntry : merge(ptrEntry, ptrRoot);
  allocations.append(ptrEntry);
  return ptrEntry;
}


void Heap::decreaseCost(Entry *ptrEntry, quint64 toCost)
{
  Q_ASSERT(!ptrEntry->used);
  Q_ASSERT(toCost < ptrEntry->cost);
  ptrEntry->cost = toCost;

  // Do we need to move this node? If not, then we're done
  if (ptrEntry == ptrRoot || ptrEntry->cost >= ptrEntry->ptrPrev->cost)
    return;

  // Detach node from prev
  if (ptrEntry == ptrEntry->ptrPrev->ptrChild)
    ptrEntry->ptrPrev->ptrChild = ptrEntry->ptrSibling;
  else
  {
    Q_ASSERT(ptrEntry == ptrEntry->ptrPrev->ptrSibling); // if this isn't prev's child, it must be the sibling;
    ptrEntry->ptrPrev->ptrSibling = ptrEntry->ptrSibling;
  }
  if (ptrEntry->ptrSibling != NULL)
    ptrEntry->ptrSibling->ptrPrev = ptrEntry->ptrPrev;
  ptrEntry->ptrPrev = NULL;

  ptrRoot = merge(ptrEntry, ptrRoot);
}


Entry* Heap::merge(Entry *a,Entry *b)
{
  Q_ASSERT(a != NULL  &&  b != NULL); // check parameter validity

  // Make sure that a's root <= b's root, swap if necessary
  if (b->cost < a->cost)
  {
    Entry *tmp = a;
    a = b;
    b = tmp;
  }

  // Add b to a's children
  b->ptrPrev = a;
  b->ptrSibling = a->ptrChild;
  if (b->ptrSibling != NULL)
    b->ptrSibling->ptrPrev = b;
  a->ptrChild = b;

  return a;
}

