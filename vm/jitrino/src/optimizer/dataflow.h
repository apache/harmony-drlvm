/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.10.24.4 $
 *
 */

#ifndef _DATAFLOW_H_
#define _DATAFLOW_H_

#include "MemoryManager.h"
#include "BitSet.h"
#include "Log.h"
#include "Stl.h"
#include "FlowGraph.h"

namespace Jitrino {

// Our DataflowValue type must provide 
//    bool meetWith(const ValueType &other); 
// which modifies this, returning true if it is changed.

template <typename DataflowValue>
class DataflowTF {
public:
    // returns true if changed
    virtual bool apply(const DataflowValue &in, DataflowValue &out) = 0;
};

template <typename DataflowValue>
class DataflowInstance {
public:
    typedef DataflowValue ValueType;
    virtual DataflowTF<DataflowValue> *getNodeBehavior(CFGNode *node) = 0;
    virtual DataflowValue getEntryValue() = 0;
};

template <typename DataflowValue>
void
solve(FlowGraph *fg, DataflowInstance<DataflowValue> &instance, bool forwards, 
      MemoryManager &mm, DataflowValue *&solutionBeforeNode, DataflowValue *&solutionAfterNode,
      Category *logCategory, bool ignoreExceptionEdges)
{
    StlVector<CFGNode *> nodes(mm);
    fg->getNodesPostOrder(nodes, forwards);
    uint32 numNodes = fg->getMaxNodeId();
    DataflowValue *beforeNode = new (mm) DataflowValue[numNodes];
    DataflowValue *afterNode = new (mm) DataflowValue[numNodes];
    for(unsigned i=0; i<numNodes; i++) {
        beforeNode[i].init(&mm);
        afterNode[i].init(&mm);
    }
    solutionBeforeNode = beforeNode;
    solutionAfterNode = afterNode;

    StlDeque<CFGNode *> queue(mm);
    BitSet inQueue(mm, numNodes);
    DataflowTF<DataflowValue> **nodeTFs= new (mm) DataflowTF<DataflowValue>*[numNodes];
    // compute TFs and initialize queue
    StlVector<CFGNode *>::reverse_iterator
        riter = nodes.rbegin(),
        rend = nodes.rend();
    for ( ; riter != rend; ++riter) {
        CFGNode *node = *riter;
        uint32 nodeId = node->getId();
        nodeTFs[nodeId] = instance.getNodeBehavior(node);
        queue.push_back(node);
        inQueue.setBit(nodeId);
    }

    CFGNode *entryNode = fg->getEntry();
    CFGNode *exitNode = fg->getExit();
    uint32 inId = forwards ? entryNode->getId() : exitNode->getId();
	uint32 firstId = queue[0]->getId();
    if( !(firstId == inId) ) assert(0);
    beforeNode[firstId] = instance.getEntryValue();

    while (!queue.empty()) {
        CFGNode *node = queue.front();
        queue.pop_front();
        inQueue.setBit(false);
        uint32 nodeId = node->getId();
        DataflowTF<DataflowValue> *nodeTF = nodeTFs[nodeId];
        if (logCategory->isDebugEnabled()) {
            Log::out() << "solve: visiting node " << (int) nodeId << ::std::endl;
        }
        if (logCategory->isDebugEnabled()) {
            Log::out() << "  in was: "; beforeNode[nodeId].print(Log::out());
            Log::out() << ::std::endl << "  out was: "; 
            afterNode[nodeId].print(Log::out());
            Log::out() << ::std::endl;
        }
        if (nodeTF->apply(beforeNode[nodeId], afterNode[nodeId])) {
            // check whether to update successors
            if (logCategory->isDebugEnabled()) {
                Log::out() << "  node changed" << ::std::endl;
                Log::out() << "  in became: "; beforeNode[nodeId].print(Log::out());
                Log::out() << ::std::endl << "  out became: "; 
                afterNode[nodeId].print(Log::out());
                Log::out() << ::std::endl;
            }
            const CFGEdgeDeque &outEdges = node->getOutEdges();
            CFGEdgeDeque::const_iterator
                eiter = outEdges.begin(),
                eend = outEdges.end();
            for ( ; eiter != eend; ++eiter) {
                CFGEdge *e = *eiter;
                CFGNode *target = e->getTargetNode();
                if (ignoreExceptionEdges && target->isDispatchNode()) continue;
                uint32 targetId = target->getId();
                if (logCategory->isDebugEnabled()) {
                    Log::out() << "    (checking successor " 
                               << (int) targetId << ", was ";
                    beforeNode[targetId].print(Log::out());
                    Log::out() << ")" << ::std::endl;
                }
                if (beforeNode[targetId].meetWith(afterNode[nodeId])) {
                    // value at successor has changed
                    if (logCategory->isDebugEnabled()) {
                        Log::out() << "    (changed successor " 
                                   << (int) targetId << ", now ";
                        beforeNode[targetId].print(Log::out());
                        Log::out() << ")" << ::std::endl;
                    }
                    if (!inQueue.getBit(targetId)) {
                        // if so, add it to queue
                        if (logCategory->isDebugEnabled()) {
                            Log::out() << "    (queuing " 
                                       << (int) targetId << ")" << ::std::endl;
                        }
                        queue.push_back(target);
                        inQueue.setBit(targetId);
                    } else {
                        if (logCategory->isDebugEnabled()) {
                            Log::out() << "    (already in queue: " 
                                       << (int) targetId << ")" << ::std::endl;
                        }
                    }
                }
            }
        } else {
            if (logCategory->isDebugEnabled()) {
                Log::out() << "  no change:" << ::std::endl;
            }
        }

    }
}

} //namespace Jitrino 

#endif // _DATAFLOW_H_
