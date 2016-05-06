#include "graph.h"
#include "javarand.h"
#include <QThread> // for delaying during a pause

#define INFINITY   100000000000000ULL       // (10^14)

Node::Node(QString name, QString owner, bool isDummy, DirectionEnum type)
{
  this->name     = name;
  this->owner    = owner;
  this->isDummy  = isDummy;
  this->type     = type;
  ptrTwin        = NULL;
  ptrMatch       = NULL;
  ptrFrom        = NULL;
  matchCost      = 0;
  mark           = 0;
  price          = 0;
  component      = 0;
  minimumInCost  = MAX_VALUE;
  ptrHeapEntry   = NULL;
}


// Returns true if this node contains an edge to SENDS-node ptrToNode
// This function replaces getEdge() from TradeMaximizer
bool Node::containsEdge(Node *ptrToNode)
{
  int idx;
  for (idx=0; idx<edges.size(); idx++)
    if (edges.at(idx)->ptrSender == ptrToNode)
      return true;
  return false;
}


Edge::Edge(Node *ptrWanter, Node *ptrSender, quint64 cost)
{
  // Test parameter validity
  Q_ASSERT(ptrWanter->type == WANTS);
  Q_ASSERT(ptrSender->type == SENDS);

  // Set local variables
  this->ptrWanter = ptrWanter;
  this->ptrSender = ptrSender;
  this->cost = cost;
}


Graph::Graph()
{
  frozen = false; // the graph is unfrozen and ready for additions by default
  timestamp = 0;
  component = 0;
  numCopies = 0;
  progress  = 0;
  viableRealItems = 0;
  ptrSinkFrom = NULL;
  ptrKeepRunning = (bool*)&timestamp; // temporary non-null assignment
  ptrPaused = ptrKeepRunning; // ditto
}

Graph::~Graph()
{
  int i,j;

  // Unallocate memory assigned to nodes and edges
  for (i=0; i<senders.size(); i++)
  {
    // Senders & wanters share the same edge references, so we make sure
    // to only delete them once.
    for (j=0; j<senders.at(i)->edges.size(); j++)
      delete senders.at(i)->edges[j];
    delete senders[i];
  }
  for (i=0; i<wanters.size(); i++)
    delete wanters[i];
  for (i=0; i<orphans.size(); i++)
    delete orphans[i];
}

Node* Graph::getNode(QString name)
{
  return nameMap.value(name, NULL);  // returns NULL if name isn't in nameMap
}


// Returns a pointer to wanter node
Node* Graph::addNode(QString name, QString owner, bool isDummy)
{
  Q_ASSERT(!frozen); // nothing should be added to a graph once it is frozen
  Q_ASSERT(!nameMap.contains(name)); // make sure this name is unique

  Node *ptrWanter = new Node(name, owner, isDummy, WANTS);
  wanters.append(ptrWanter);
  nameMap.insert(name,ptrWanter);

  Node *ptrSender = new Node(name+" sender", owner, isDummy, SENDS);
  senders.append(ptrSender);
  ptrWanter->ptrTwin = ptrSender;
  ptrSender->ptrTwin = ptrWanter;

  return ptrWanter;
}


void Graph::addEdge(Node *ptrWanter, Node *ptrSender, quint64 cost)
{
  Q_ASSERT(!frozen); // nothing should be added to a graph once it is frozen

  // Create an edge (a connection) and add it to both nodes
  Edge *edge = new Edge(ptrWanter, ptrSender, cost);
  ptrWanter->edges.append(edge);
  ptrSender->edges.append(edge);
  // Don't bother checking if the sending node has a lower minimum cost
  // right now. It will be recalculated anyway after removing edges later.
  // !@#
  // Check if the SENDER node now has a lower minimum cost
  for (int j=0; j<ptrSender->edges.size(); j++)
    if (ptrSender->edges.at(j)->cost < ptrSender->minimumInCost)
      ptrSender->minimumInCost = ptrSender->edges.at(j)->cost;
}


// The graph can be "frozen" from adding new elements.
// TradeMaximizer took advantage of this step by converting its lists
// to arrays, which were better for its iteration speed.
void Graph::freeze()
{
  Q_ASSERT(!frozen); // make sure we're not freezing it twice
  frozen = true;
}


void Graph::visitWanters(Node *ptrWanter)
{
  Q_ASSERT(ptrWanter->type == WANTS);
  // Mark this wanter as visited
  ptrWanter->mark = timestamp;
  // Visit all the wanters of this wanter
  for (int idx=0; idx<ptrWanter->edges.size(); idx++)
  {
    Node *ptrNode = ptrWanter->edges.at(idx)->ptrSender->ptrTwin;
    if (ptrNode->mark != timestamp)
      visitWanters(ptrNode);
  }
  // Add the twin (the sender) node to the queue for visitSenders()
  finished.append(ptrWanter->ptrTwin);
}
void Graph::visitSenders(Node *ptrSender)
{
  Q_ASSERT(ptrSender->type == SENDS);
  ptrSender->mark = timestamp; // mark this sender as visited
  // Visit all the senders of this sender
  for (int idx=0; idx<ptrSender->edges.size(); idx++)
  {
    Node *ptrNode = ptrSender->edges.at(idx)->ptrWanter->ptrTwin;
    if (ptrNode->mark != timestamp)
      visitSenders(ptrNode);
  }
  // Mark both this sender and its twin (wanter) with the current
  // "component" iteration number, a sort of generational code
  // to differentiate groups that never want each other.
  ptrSender->component = ptrSender->ptrTwin->component = component;
}

// Traverses edges and removes entries whose sender/wanter pair have
// unequal component numbers, which indicates they aren't strongly connected
void Node::removeBadEdges(QList<Edge*> edgeDelQueue)
{
  // Remove edges with unequal component numbers
  for (int idx=edges.size()-1; idx>=0; idx--)
  {
    if (edges.at(idx)->ptrWanter->component != edges.at(idx)->ptrSender->component)
    {
      if (!edgeDelQueue.contains(edges.at(idx)))
        edgeDelQueue.append(edges.at(idx));
      edges.removeAt(idx);
    }
  }
}


// Returns the node's name for displaying results
QString Node::show(bool sortByItem)
{
  if (owner.isEmpty() || isDummy)
    return name;
  if (sortByItem)
    return name + " " + owner;
  return owner + " " + name;
}


// Remove unusable edges and resulting orphaned entries from the graph
void Graph::removeImpossibleEdges()
{
  Q_ASSERT(frozen); // the graph should only be cleaned up once we are done adding things
  QList<Edge*> edgeDelQueue; // queue up edges to be deleted

  // We use the timestamp to flag items as visited. It needs to be advanced from
  // its default value of zero, which is also the unvisited flag value.
  timestamp++;
  finished.clear();

  // We use Kosaraju's algorithm to determine which comopnents are strongly connected.
  // Strongly connected means every node is reachable from every other node.

  // The first loop determines the order for the second loop by placing nodes
  // in "finished" in the order that they point towards each other
  for (int idx=0; idx<wanters.size()  &&  *ptrKeepRunning; idx++)
    if (wanters.at(idx)->mark != timestamp)
      visitWanters(wanters.at(idx));
  // Iterate the list of senders is reverse order such that they point
  // in the direction in which we are now traversing the graph
  for (int idx=finished.size()-1; idx>=0; idx--)
  {
    if (finished.at(idx)->mark != timestamp)
    {
      component++; // increment the strongly connected iteration count
      visitSenders(finished.at(idx)); // visit the next group of senders
    }
  }

  // Now remove all edges between two different component counts
  for (int idx=0; idx<wanters.size()  &&  *ptrKeepRunning; idx++)
    wanters.at(idx)->removeBadEdges(edgeDelQueue);
  for (int idx=0; idx<senders.size()  &&  *ptrKeepRunning; idx++)
  {
    Node *ptrNode = senders.at(idx);
    ptrNode->removeBadEdges(edgeDelQueue);

    // Calculate the lowest incoming cost
    ptrNode->minimumInCost = MAX_VALUE;
    for (int j=0; j<ptrNode->edges.size(); j++)
      if (ptrNode->edges.at(j)->cost < ptrNode->minimumInCost)
        ptrNode->minimumInCost = ptrNode->edges.at(j)->cost;
  }

  // Remove orphaned items, which are items no longer wanted by anybody
  // after culling bad edges. (It's not as sad as it sounds.)
  removeOrphans(edgeDelQueue);

  // Delete allocated memory associated with culled edges
  while(!edgeDelQueue.isEmpty())
    delete edgeDelQueue.takeFirst();
}

// Cull nodes that have no edges, except to themselves (wanter-sender pair)
void Graph::removeOrphans(QList<Edge*> edgeDelQueue)
{
  // Count the number of nodes with at least one wanter
  for (int idx=wanters.size()-1; idx>=0  &&  *ptrKeepRunning; idx--)
  {
    if (wanters.at(idx)->edges.size() < 2)
    {
      // There should always be at least one edge (between the sender and wanter)
      Q_ASSERT(wanters.at(idx)->edges.size() == 1);
      Q_ASSERT(wanters.at(idx)->edges.at(0)->ptrSender == wanters.at(idx)->ptrTwin);
      edgeDelQueue.append(wanters.at(idx)->edges.at(0));
      orphans.append(wanters[idx]);
      wanters.removeAt(idx);
    }
    else
      if (!wanters.at(idx)->isDummy) // count viable, non-dummy items
        viableRealItems++;
  }

  for (int idx=senders.size()-1; idx>=0; idx--)
  {
    if (senders.at(idx)->edges.size() < 2)
    {
      edgeDelQueue.append(senders.at(idx)->edges.at(0));
      delete senders[idx];
      senders.removeAt(idx);
    }
  }
}


//////////////////////////////////////////////////////////////////////////////


void Graph::dijkstra(Heap *ptrHeap)
{
  ptrSinkFrom = NULL;
  sinkCost = MAX_VALUE;

  // Insert all nodes, both wanter and sender, into the heap
  for (int idx=0; idx<senders.size(); idx++)
  {
    senders.at(idx)->ptrFrom = NULL;
    // Give entry the highest cost
    senders.at(idx)->ptrHeapEntry = ptrHeap->insert(senders.at(idx), INFINITY);
  }
  for (int idx=0; idx<wanters.size(); idx++)
  {
    wanters.at(idx)->ptrFrom = NULL;
    // Give entry highest cost if unmatched, else lowest
    quint64 cost = (wanters.at(idx)->ptrMatch == NULL) ? 0 : INFINITY;
    wanters.at(idx)->ptrHeapEntry = ptrHeap->insert(wanters.at(idx), cost);
  }

  while (!ptrHeap->isEmpty())
  {
    // Grab the lowest cost entry's node and cost
    Entry *ptrMinEntry = ptrHeap->extractMin();
    Node *ptrNode = ptrMinEntry->ptrNode;
    quint64 cost = ptrMinEntry->cost;
    int size;

    if (cost == INFINITY)
      break; // everything left is unreachable

    if (ptrNode->type == WANTS)
    {
      size = ptrNode->edges.size();
      for(int idx=0; idx<size; idx++)
      {
        Node *ptrOther = ptrNode->edges.at(idx)->ptrSender;
        if (ptrOther == ptrNode->ptrMatch)
          continue; // ignore item's current match
        // Price of wanter->sender is WantPrice + edgeCost - SendPrice
        // Note: The SendPrice is typically the value of the sender's lowest edgeCost
        //       until all edges' nodes have been matched, then it's infinite.
        quint64 c = ptrNode->price + ptrNode->edges.at(idx)->cost - ptrOther->price;
        Q_ASSERT(c <= MAX_VALUE); // per algorithm, all costs must be non-negative
        if (cost + c < ptrOther->ptrHeapEntry->cost)
        {
          // We found a cheaper path between the node and this sender
          ptrHeap->decreaseCost(ptrOther->ptrHeapEntry, cost+c);
          ptrOther->ptrFrom = ptrNode;
        }
      }
    }
    else if (ptrNode->ptrMatch == NULL)
    { // unmatched SENDS
      if (cost < sinkCost)
      {
        ptrSinkFrom = ptrNode;
        sinkCost = cost;
      }
    }
    else
    { // matched SENDER
      Node *ptrOther = ptrNode->ptrMatch;
      // Price of sender->wanter is SendPrice + edgeCost - WantPrice
      // Note: The WantPrice is low until everything wanting the item is matched up
      quint64 c = ptrNode->price - ptrOther->matchCost - ptrOther->price;
      Q_ASSERT(c <= MAX_VALUE); // per algorithm, all costs must be non-negative
      if (cost + c < ptrOther->ptrHeapEntry->cost)
      {
        ptrHeap->decreaseCost(ptrOther->ptrHeapEntry, cost+c);
        ptrOther->ptrFrom = ptrNode;
      }
    }
  } // end while(!heap.isEmpty)
} // end dijkstra()


// The findcycles() function uses dijkstra's algorithm to find perfect matching.
// It iterates once for each WANTER/SENDER pair, doing the following:
//   1) Look at unmatched WANTER
//     a. Mark each want's SENDER as "to be evaluated"
//     [b. If no one else wanted my want last time, enable self-match]
//   2) Look at each SENDER marked "to be evaluated"
//     a. If unmatched, propose as matching candidate (with WANTER that wants it)
//     b. If matched, evaluate the matched WANTER per bullet #1
//     [c. If there's no one else to send to me, self-match]
// A matched WANTER is only evaluated if an unmatched WANTER wants the same thing as it.
// A SENDER is only evaluated if an unmatched WANTER wants it.
// Anything that's not evaluated gets a high price for next iteration.
// Consequently, a high-priced SENDER:
//   a. Has no unmatched WANTERS wanting it
//   b. Becomes a dandidate to match itself
// And a high-priced WANTER:
//   a. Wants something no one else does (or that no unmatched WANTER does, rather)
//
// It's a clever algorith that gets confusing when you start following the price/cost
// values of everything in play.
CyclesType* Graph::findCycles()
{
  Q_ASSERT(frozen); // graph analysis should only be performed when we are done adding things

  // Initialize all nodes
  for (int idx=0; idx<wanters.size(); idx++)
  {
    wanters.at(idx)->ptrMatch = NULL;
    wanters.at(idx)->price = 0;
  }
  for (int idx=0; idx<senders.size(); idx++)
  {
    senders.at(idx)->ptrMatch = NULL;
    senders.at(idx)->price = senders.at(idx)->minimumInCost;
  }

  for (int round = 0; round < wanters.size(); round++)
  {
    if ((round & 0x3F) == 0)
    {
      progress = (round<<8)/wanters.size()+1;
      if (*ptrKeepRunning == false)
        return NULL;
      while (*ptrPaused)
        QThread::sleep(1); // delay for a second
    }

    // Allocate the heap here instead of inside dijkstra() because
    // we want to keep using the heapEntries afterwards.
    Heap heap(senders.size()*2);
    dijkstra(&heap);

    // Update the matching
    Node *ptrSender = ptrSinkFrom;
    Q_ASSERT(ptrSender != NULL);
    while (ptrSender != NULL)
    {
      Node *ptrWanter = ptrSender->ptrFrom;

      // Unlink sender and wanter from current matches
      if (ptrSender->ptrMatch != NULL)
        ptrSender->ptrMatch->ptrMatch = NULL;
      if (ptrWanter->ptrMatch != NULL)
        ptrWanter->ptrMatch->ptrMatch = NULL;

      // Set the sender/receiver match to each other
      ptrSender->ptrMatch = ptrWanter;
      ptrWanter->ptrMatch = ptrSender;

      // Update matchCost
      int size = ptrWanter->edges.size();
      for (int idx=0; idx<size; idx++)
      { // iterate until we find the corresponding edge
        if (ptrWanter->edges.at(idx)->ptrSender == ptrSender)
        {
          ptrWanter->matchCost = ptrWanter->edges.at(idx)->cost;
          break;
        }
      }

      ptrSender = ptrWanter->ptrFrom; // evaluate the sender node this was connected to previously
    }

    // Update the prices
    for (int idx=0; idx<wanters.size(); idx++)
    {
      wanters.at(idx)->price += wanters.at(idx)->ptrHeapEntry->cost;
      if (wanters.at(idx)->price > MAX_VALUE)
        wanters.at(idx)->price = MAX_VALUE; // prevent unsigned from wrapping
    }
    for (int idx=0; idx<senders.size(); idx++)
    {
      senders.at(idx)->price += senders.at(idx)->ptrHeapEntry->cost;
      if (senders.at(idx)->price > MAX_VALUE)
        senders.at(idx)->price = MAX_VALUE; // prevent unsigned from wrapping
    }
  }
  progress = 256;

  // Bypass dummy entries that are matched and match the dummies to themselves
  elideDummies();

  timestamp++;
  CyclesType *ptrCycles = new CyclesType();

  // Assemble the results
  for (int idx=0; idx<wanters.size(); idx++)
  {
    Node *ptrNode = wanters.at(idx);
    if (ptrNode->mark == timestamp || ptrNode->ptrMatch == ptrNode->ptrTwin)
      continue; // don't add unmatched entries

    QList<Node> *ptrCyc = new QList<Node>();
    while (ptrNode->mark != timestamp)
    {
      ptrNode->mark = timestamp;
      ptrCyc->append(*ptrNode);
      ptrNode = ptrNode->ptrMatch->ptrTwin;
    }
    ptrCycles->append(ptrCyc);
  }
  return ptrCycles;
} // end findCycles


//////////////////////////////////////////////////////////////////////////////

// Shuffles receivers for a different result
void Graph::shuffle(JavaRand &random)
{
  // Note: The order of the shuffling should not be optimized because
  // it needs to match the order of TradeMaximizer for the results to
  // match.

  // Shuffle the order of WANTER nodes
  for (int i = wanters.size(); i > 1; i--)
  {
    int j = random.nextInt(i);
    Node *tmp = wanters.at(j);
    wanters[j] = wanters.at(i-1);
    wanters[i-1] = tmp;
  }

  // Shuffle the order of edges on WANTER nodes
  for (int a = 0; a < wanters.size(); a++)
  {
    Node *n = wanters.at(a);
    for (int i = n->edges.size(); i > 1; i--)
    {
      int j = random.nextInt(i);
      Edge *tmp = n->edges.at(j);
      n->edges[j] = n->edges.at(i-1);
      n->edges[i-1] = tmp;
    }
  }
}


// Bypass dummy entries that are matched and match the dummies to themselves
void Graph::elideDummies()
{
  for (int idx=0; idx<wanters.size(); idx++)
  {
    if (wanters.at(idx)->isDummy)
      continue;

    while (wanters.at(idx)->ptrMatch->isDummy)
    {
      Node *ptrDummySender = wanters.at(idx)->ptrMatch;
      Node *ptrNextSender = ptrDummySender->ptrTwin->ptrMatch;
      wanters.at(idx)->ptrMatch = ptrNextSender;
      ptrNextSender->ptrMatch = wanters.at(idx);
      ptrDummySender->ptrMatch = ptrDummySender->ptrTwin;
      ptrDummySender->ptrTwin->ptrMatch = ptrDummySender;
    }
  }
}


// Some of the stuff in this function is a little wonky because we want to
// maintain the same node and edge ordering so that the program remains
// compatible with TradeMaximizer's results.
void Graph::copy(Graph *ptrEmptyGraph)
{
  Q_ASSERT(frozen); // graph shouldn't be duplicated until it is complete
  Q_ASSERT(ptrEmptyGraph != NULL);

  // Create copies of nodes in the new graph. We can't use addNode() becuase
  // we need to maintain the node ordering.
  // First, we copy the receivers in their (shuffled) order
  for (int idx=0; idx<wanters.size(); idx++)
  {
    Node *n = wanters.at(idx);
    Node *ptrWanter = new Node(n->name, n->owner, n->isDummy, WANTS);
    ptrEmptyGraph->wanters.append(ptrWanter);
    ptrEmptyGraph->nameMap.insert(n->name, ptrWanter);
  }
  // Then we copy the senders in their (original) order
  for (int idx=0; idx<senders.size(); idx++)
  {
    Node *n = senders.at(idx);
    Node *ptrSender = new Node(n->name, n->owner, n->isDummy, SENDS);
    ptrEmptyGraph->senders.append(ptrSender);
    // Link twin nodes together
    QString wantName = n->name.left(n->name.length()-7); // take off " sender"
    Node *ptrWanter = ptrEmptyGraph->getNode(wantName);
    ptrWanter->ptrTwin = ptrSender;
    ptrSender->ptrTwin = ptrWanter;
  }

  // Edges go between senders and wanters, so we need only look at one side
  // to copy all the edges over. However, we need to add them in the same order
  // they were originally added, so we traverse the senders, which were never
  // shuffled, then look at their matching wanters, to keep the edge order the
  // same.
  for (int i=0; i<senders.size(); i++)    // in sender order
  {
    Node *n = senders.at(i)->ptrTwin;     // evaluate wanters'
    for (int j=0; j<n->edges.size(); j++) // edges
    {
      Edge *e = n->edges.at(j);
      Node *ptrWanter = ptrEmptyGraph->getNode(e->ptrWanter->name);
      Node *ptrSender = ptrEmptyGraph->getNode(e->ptrSender->ptrTwin->name)->ptrTwin;
      ptrEmptyGraph->addEdge(ptrWanter, ptrSender, e->cost);
    }
  }

  ptrEmptyGraph->numCopies = ++numCopies;
  ptrEmptyGraph->ptrKeepRunning = ptrKeepRunning;
  ptrEmptyGraph->ptrPaused = ptrPaused;
  ptrEmptyGraph->viableRealItems = viableRealItems; // not used in copies, but copy it anyway
  ptrEmptyGraph->freeze(); // lock down the populated graph
}

