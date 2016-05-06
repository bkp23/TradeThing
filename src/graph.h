#ifndef GRAPH_H
#define GRAPH_H

#include <QHash> // for storing the nameMap
#include "heap.h"
#include "javarand.h"

#define MAX_VALUE  9223372036854775807ULL   // (2^63 - 1)

// Forward class declarations
class Graph; // Contains nodes; handles graph algorithms
class Node;  // Contains edges and name data
class Edge;  // Connects two nodes together
class Entry; // (heap.h)
class Heap;  // (heap.h)

typedef QList< QList<Node>* > CyclesType;

// WANTS node edges represent all the wants on an item's list.
// SENDS node edges represent all the items wanting this item.
// In other words, the edges are items this item is willing to receive (for
// WANTS) or to whom the item might be sent (for SENDS).
typedef enum _DirectionEnum
{
  WANTS, // Called "RECEIVER" in TradeMaximizer code
  SENDS  // Called "SENDER" in TradeMaximizer code
} DirectionEnum;

class Node
{
  public:
    Node(QString name, QString owner, bool isDummy, DirectionEnum type);
    bool containsEdge(Node *ptrToNode);
    void removeBadEdges(QList<Edge*> edgeDelQueue); // used as part of culling impossible edges
    QString show(bool sortByItem);

    // Very basic node information
    QString name;  // The name of this item
    QString owner; // The owner (username) of this item
    bool isDummy;
    DirectionEnum type; // The type of node this is

    // A list of edges (connections) between this node and others
    QList<Edge*> edges;

    Node *ptrTwin;  // the other half of this node WANTS-SENDS node pair
    Node *ptrMatch; // the node to which this node is currently matched

    quint64 matchCost;

    // Internal data for graph algorithms
    quint64 minimumInCost; // only tracked in the SEND nodes
    unsigned int mark; // flag used for marking as visited in dfs and dijkstra
    Node *ptrFrom; // the node associated with the cheapest path in dijkstra
    quint64 price;
    Entry* ptrHeapEntry; // contains the current cost (see "from" node); only valid in Heap scope
    int component; // used for removing impossible edges
};


class Edge
{
  public:
    Edge(Node *ptrWanter, Node *ptrSender, quint64 cost);

    Node *ptrWanter; // Called "receiver" in TradeMaximizer code
    Node *ptrSender;
    quint64 cost;
};


class Graph
{
  public:
    Graph();
    ~Graph();
    Node* getNode(QString name);
    Node* addNode(QString name, QString owner, bool isDummy);
    void  addEdge(Node *ptrWanter, Node *ptrSender, quint64 cost);
    void  freeze();
    void  removeImpossibleEdges();
    CyclesType* findCycles(); // the return must be deallocated by caller
    void  shuffle(JavaRand &random);
    void  copy(Graph *ptrEmptyGraph);

    // Track the wanters and receivers
    QList<Node*> wanters, senders;
    // Keep track of orphaned items that were not connected to other items after
    // culling unusable edges
    QList<Node*> orphans;
    // The nameMap helps make sure we don't duplicate node names
    // and lets us find WANTER nodes by their names
    QHash<QString,Node*> nameMap;
    bool *ptrKeepRunning; // indicates whether operation has been canceled
    bool *ptrPaused;      // indicates whether operation is temporarily paused

    int numCopies; // Keeps track of which graph copy this is (or how many were made)
    int progress; // Tracks findCycles() progress from 1..256
    int viableRealItems; // number of non-dummy items after culling

  private:
    void elideDummies();

    bool frozen; // the graph is unfrozen and ready for additions by default
    unsigned int timestamp; // used for determining which loop iteration we're running

    // These are used for determinng impossible edges (see removeImpossibleEdges())
    // using Kosaraju's algorithm.
    void visitWanters(Node *ptrWanter);
    void visitSenders(Node *ptrSender);
    void removeOrphans(QList<Edge*> edgeDelQueue);
    unsigned int component; // used in determining impossible edges
    // Keep track of which sender nodes have had their wanter twin visited
    QList<Node*> finished;


    // These are used in performing the actual search using Dijkstra's algorithm
    void dijkstra(Heap *ptrHeap);
    Node *ptrSinkFrom; // designates the sending node with the lowest cost
    quint64 sinkCost;  // designates the cost of the cheapest sending node
};

#endif // GRAPH_H

