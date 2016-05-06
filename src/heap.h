#ifndef HEAP_H
#define HEAP_H

#include <QList>  // for keeping track of allocations
#include "graph.h"

class Node; // forward declaration (graph.h)

// Priority queues are implemented as pairing heaps

// Entry is the type of nodes in the skew heap.
class Entry
{
  public:
    Entry(Node *ptrNode,quint64 cost);
    Node  *ptrNode; // the node with which this entry is associated
    quint64 cost;   // the current cost of the vertex in the dijkstra algorithm

    // These are used by the Heap class
    Entry *ptrChild;
    Entry *ptrSibling;
    Entry *ptrPrev; // parent if first child, else previous sibling
    bool used;      // flag for marking the entry as removed from the heap
};


class Heap
{
  public:
    Heap();
    Heap(int expectedSize);
    ~Heap();
    bool isEmpty();
    Entry* extractMin();
    Entry* insert(Node *ptrNode, quint64 cost); // Create a new entry and merge it into root
    void decreaseCost(Entry *ptrEntry, quint64 toCost);

  private:
    Entry* merge(Entry *a,Entry *b);

    Entry *ptrRoot;
    QList<Entry*> allocations;
};

#endif // HEAP_H
