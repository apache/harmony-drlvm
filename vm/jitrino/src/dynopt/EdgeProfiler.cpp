/*
*  Licensed to the Apache Software Foundation (ASF) under one or more
*  contributor license agreements.  See the NOTICE file distributed with
*  this work for additional information regarding copyright ownership.
*  The ASF licenses this file to You under the Apache License, Version 2.0
*  (the "License"); you may not use this file except in compliance with
*  the License.  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*/
/* COPYRIGHT_NOTICE */

/**
* @author Jack Liu, Mikhail Y. Fursov, Chen-Dong Yuan
* @version $Revision$
*/


#include "Jitrino.h"
#include "EdgeProfiler.h"
#include "Inst.h"
#include "Stl.h"
#include "StaticProfiler.h"
#include "Dominator.h"
#include "Loop.h"
#include "FlowGraph.h"

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <sstream>

namespace Jitrino {

static bool isMethodTrivial( ControlFlowGraph& cfg );
static uint32 computeCheckSum( MemoryManager& mm, ControlFlowGraph& cfg,  const StlSet<Node*>& nodesToIgnore);
static bool calculateProbsFromProfile(MemoryManager& mm, ControlFlowGraph& fg, const Edges& edges, DominatorTree* dt, LoopTree* lt, EdgeMethodProfile* profile, bool bcLevelProfiling, const StlSet<Node*>& nodesToIgnore);
static Node* selectNodeToInstrument(IRManager& irm, Edge* edge);
static void selectEdgesToInstrument(MemoryManager& mm, IRManager& irm, Edges& result, const StlSet<Node*>& nodesToIgnore);
static uint32 genKey( uint32 n, Edge* edge, bool bcLevel, bool debug);
static bool hasCatch( Node* node );
static void selectedNodesToIgnore(IRManager& irm, LoopTree* lt, StlSet<Node*>& result);


DEFINE_SESSION_ACTION(EdgeProfilerInstrumentationPass, edge_instrument, "Perform edge instrumentation pass")

void EdgeProfilerInstrumentationPass::_run(IRManager& irm)
{
    ControlFlowGraph& flowGraph = irm.getFlowGraph();
    MemoryManager mm( 1024, "Edge InstrumentationPass");
    MethodDesc& md = irm.getMethodDesc();
    InstFactory& instFactory = irm.getInstFactory();
    OptPass::computeDominatorsAndLoops(irm);
    bool debug = Log::isEnabled();
    LoopTree* lt = irm.getLoopTree();    


    //set of nodes with out-edges are not taken into account during instrumentation
    //and checksum calculation
    //such nodes are optional in CFG. E.g. nodes with class initializers insts
    StlSet<Node*> nodesToIgnore(mm);
    selectedNodesToIgnore(irm, lt, nodesToIgnore);

    //compute checksum first
    uint32 _checkSum = computeCheckSum(mm, flowGraph,  nodesToIgnore);

    StlVector<uint32> counterKeys(mm);
    // Instrument method entry first.
    Node* entryNode = flowGraph.getEntryNode();
    entryNode->prependInst(instFactory.makeIncCounter(0));
    bool methodIsTrivial = isMethodTrivial(flowGraph);
    bool bcLevelProfiling = false;
    if (!methodIsTrivial) {
        // Scan the CFG node in topological order and record the blocks and
        // edges where we need to add instrumentation code.
        // The actual instrumentation will be done in a separate phase so that
        // we won't disturb the CFG as we are traversing it.
        
        Edges edgesToInstrument(mm);
        selectEdgesToInstrument(mm, irm, edgesToInstrument, nodesToIgnore);
        //compute edge-ids before CFG modification: edge-ids are part of CFG consistency check.
        for (Edges::const_iterator it = edgesToInstrument.begin(), end = edgesToInstrument.end(); it!=end; ++it) {
            Edge* edge = *it;
            uint32 key = genKey((uint32)counterKeys.size() + 1, edge, bcLevelProfiling, debug);
            assert( key != 0 );
            counterKeys.push_back(key);
        }

        // Now revisit all of the edges that need to be instrumented
        // and generate instrumentation code.
        uint32 i = 0;
        for (Edges::const_iterator it = edgesToInstrument.begin(), end = edgesToInstrument.end(); it!=end; ++it, ++i) {
            Edge* edge = *it;
            Node* nodeToInstrument = selectNodeToInstrument(irm, edge);
            assert(nodeToInstrument!=NULL && nodeToInstrument->isBlockNode());
            uint32 key = counterKeys[i];
            Inst* incInst = instFactory.makeIncCounter( key );
            assert(((Inst*)nodeToInstrument->getFirstInst())->getOpcode() != Op_IncCounter );
            nodeToInstrument->prependInst(incInst);
        }
    } 
    
    irm.getCompilationInterface().lockMethodData();
    
    ProfilingInterface* pi = irm.getProfilingInterface();
    if (!pi->hasMethodProfile(ProfileType_Edge, md, JITProfilingRole_GEN)) {
        pi->createEdgeMethodProfile(mm , md,  (uint32)counterKeys.size(),  counterKeys.empty()?NULL:(uint32*)&counterKeys.front(), _checkSum);
    }

    irm.getCompilationInterface().unlockMethodData();

    if (debug) {
        Log::out() << std::endl << "EdgePC:: instrumented, nCounters="<<counterKeys.size() <<" checksum="<<_checkSum << std::endl;
    }

}

DEFINE_SESSION_ACTION(EdgeProfilerAnnotationPass, edge_annotate, "Perform edge annotation pass")

void EdgeProfilerAnnotationPass::_run(IRManager& irm) {
    ControlFlowGraph& flowGraph = irm.getFlowGraph();
    MethodDesc& md = irm.getMethodDesc();
    MemoryManager mm( 1024, "Edge AnnotationPass");
    bool debug = Log::isEnabled();
    // Create the edge profile structure for the compiled method in 'irm'.
    ProfilingInterface* pi = irm.getProfilingInterface();
    bool edgeProfilerMode = pi->isProfilingEnabled(ProfileType_Edge, JITProfilingRole_USE);
    bool entryBackedgeProfilerMode = !edgeProfilerMode && pi->isProfilingEnabled(ProfileType_EntryBackedge, JITProfilingRole_USE);
    uint32 entryCount = (edgeProfilerMode || entryBackedgeProfilerMode) ? pi->getProfileMethodCount(md) : 0;
    if (isMethodTrivial(flowGraph) || !edgeProfilerMode || entryCount == 0) { 
        // Annotate the CFG using static profiler heuristics.
        if (debug) {
            Log::out()<<"Using static profiler to estimate trivial graph"<<std::endl;
        }
        StaticProfiler::estimateGraph(irm, entryCount);
        return;
    }
        
    OptPass::computeDominatorsAndLoops(irm);
    DominatorTree* dt = irm.getDominatorTree();

    LoopTree* lt = irm.getLoopTree();

    // sync checksum
    StlSet<Node*> nodesToIgnore(mm); //see instrumentation pass for comments
    selectedNodesToIgnore(irm, lt, nodesToIgnore);

    uint32 cfgCheckSum = computeCheckSum(mm, flowGraph, nodesToIgnore);

    EdgeMethodProfile* edgeProfile = pi->getEdgeMethodProfile(mm, md);
    uint32 profileCheckSum = edgeProfile->getCheckSum();
    //assert(profileCheckSum == cfgCheckSum);
    if (cfgCheckSum == profileCheckSum) {
        // Start propagating the CFG from instrumented edges.
        Edges edges(mm);
        selectEdgesToInstrument(mm, irm, edges, nodesToIgnore);
        assert(edges.size() == edgeProfile->getNumCounters());
        bool bcLevelProfiling = false; //TODO:
        bool res = calculateProbsFromProfile(mm, flowGraph, edges, dt, lt, edgeProfile, bcLevelProfiling, nodesToIgnore);
        if (res) {
            flowGraph.setEdgeProfile(true);
        }
    }
    if (!flowGraph.hasEdgeProfile()) {
        if (debug) {
            Log::out()<<"DynProf failed: using static profiler to estimate graph!"<<std::endl;
        }
        StaticProfiler::estimateGraph(irm, 10000, true);
    }
    if (irm.getParent() == NULL) {
        // fix profile: estimate cold paths that was never executed and recalculate frequencies
        StaticProfiler::fixEdgeProbs(irm); 
        flowGraph.smoothEdgeProfile();
    }
}

static inline void setEdgeFreq(StlVector<double>& edgeFreqs, Edge* edge, double freq, bool debug) {
    if (debug) {
        Log::out()<<"\t settings edge(id="<<edge->getId()<<") freq ("; FlowGraph::printLabel(Log::out(), edge->getSourceNode());
        Log::out() << "->"; FlowGraph::printLabel(Log::out(), edge->getTargetNode()); Log::out()<<") to "<<freq << std::endl;
    }
    assert(freq>=0);
    edgeFreqs[edge->getId()] = freq;
}

static void updateDispatchPathsToCatch(Node* node, StlVector<double>& edgeFreqs, bool debug) {
    double freq =  node->getExecCount();
    const Edges& inEdges = node->getInEdges();
    for (Edges::const_iterator it = inEdges.begin(), end = inEdges.end(); it!=end; ++it) {
        Edge* edge = *it;
        Node* srcNode = edge->getSourceNode();
        if (srcNode->isDispatchNode() && freq > edgeFreqs[edge->getId()]) {
            setEdgeFreq(edgeFreqs, edge, freq, debug);
            srcNode->setExecCount(std::max(0.0, srcNode->getExecCount()) + freq);
            updateDispatchPathsToCatch(srcNode, edgeFreqs, debug);
        }
    }
}

static bool calculateProbsFromProfile(MemoryManager& mm, ControlFlowGraph& fg, const Edges& pEdges, 
                                      DominatorTree* dt, LoopTree* lt, EdgeMethodProfile* profile, 
                                      bool bcLevelProfiling, const StlSet<Node*>& nodesToIgnore) 
{
    //calculate edge freqs and use them to calculate edge probs.

    bool debug = Log::isEnabled();
    if (debug) {
        Log::out()<<"Starting probs calculation" <<std::endl;
    }
    uint32 entryCount = *profile->getEntryCounter();
 
    //1. assign default value to nodes and edges, this value is used for consistency checks latter
    StlVector<Node*> nodes(mm);
    fg.getNodesPostOrder(nodes); 
    for (StlVector<Node*>::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        node->setExecCount(-1);
        const Edges& edges = node->getOutEdges();
        for (Edges::const_iterator it2 = edges.begin(), end2 = edges.end(); it2!=end2; ++it2) {
            Edge* edge = *it2;
            edge->setEdgeProb(-1);
        }
    }
    
    //2. calculate edge-freq for every edge.
    
    //2.1 get freqs for instrumented edges from profile
    StlVector<double> edgeFreqs(mm, fg.getMaxEdgeId(), -1);
    uint32 n = 1;
    for (Edges::const_iterator it = pEdges.begin(), end = pEdges.end(); it!=end; ++it, ++n) {
        Edge* edge = *it;
        uint32 key = genKey(n, edge, bcLevelProfiling, debug);
        uint32* counterAddr = profile->getCounter(key);
        assert(counterAddr!=NULL);
        if (counterAddr == NULL) {
            return false;
        }
        uint32 freq = *counterAddr;
        setEdgeFreq(edgeFreqs, edge, freq, debug);
    }

    
    //2.2 fix catch freqs, so we will have estimated D->C edges
    if (debug) {
        Log::out()<<"\tFixing catch freqs"<<std::endl;
    }
    for (StlVector<Node*>::reverse_iterator it = nodes.rbegin(), end = nodes.rend(); it!=end; ++it) {
        Node* node = *it;
        bool ignored = nodesToIgnore.find(node)!=nodesToIgnore.end();
        if (!node->isCatchBlock() || ignored) { 
            continue;
        }
        //set up catch block prob -> will be used for dispatch nodes.
        double nodeFreq = 0;
        const Edges& outEdges = node->getOutEdges();
        for (Edges::const_iterator it2 = outEdges.begin(), end2 = outEdges.end(); it2!=end2; ++it2) {                    
            Edge* edge = *it2;
            double edgeFreq = edgeFreqs[edge->getId()];
            assert(edgeFreq >= 0);
            nodeFreq+=edgeFreq;
        }
        node->setExecCount(nodeFreq);
        if (debug) {
            Log::out()<<"\t\t fixing catch node ";FlowGraph::printLabel(Log::out(), node);Log::out()<<" freq="<<nodeFreq<<std::endl;
        }
        updateDispatchPathsToCatch(node, edgeFreqs, debug);
    }

    //2.3 propagate freqs to every edge and save node freqs
    if (debug) {
        Log::out()<<"\tPropagating edge freqs"<<std::endl;
    }
    for (StlVector<Node*>::reverse_iterator it = nodes.rbegin(), end = nodes.rend(); it!=end; ++it) {
        Node* node = *it;
        bool ignored = nodesToIgnore.find(node)!=nodesToIgnore.end();
        double nodeFreq = 0;
        const Edges& outEdges = node->getOutEdges();
        const Edges& inEdges = node->getInEdges();
        if (debug) {
            Log::out()<<"\t\tNode="; FlowGraph::printLabel(Log::out(), node);Log::out()<<" in-edges="<<inEdges.size() << " was ignored="<<ignored<<std::endl;
        }
        if (node->getInDegree() == 0) {
            assert(node == fg.getEntryNode());
            nodeFreq = entryCount;
        } else if (node->isCatchBlock()) { //set up catch block prob -> will be used for dispatch nodes.
            if (ignored) { 
                // ignored catch edge (successor of ignored node or catch-loop)
                // max we can do here is to try to get catch freq from in-edge
                double _freq = 0;
                Edge* inEdge  = *node->getInEdges().begin();
                if (node->getInDegree() == 1 && edgeFreqs[inEdge->getId()]>=0) {
                    _freq = edgeFreqs[inEdge->getId()];
                } 
                Edge* outEdge = *node->getOutEdges().begin();
                nodeFreq = edgeFreqs[outEdge->getId()] = _freq;
            } else {//all out-edges were instrumented -> easy to calculate freq
                nodeFreq = node->getExecCount(); //already assigned
                assert(nodeFreq>=0);
            } 
        } else {
            for (Edges::const_iterator it2 = inEdges.begin(), end2 = inEdges.end(); it2!=end2; ++it2) {
                Edge* edge = *it2;
                Node* srcNode = edge->getSourceNode();
                //assign edge-freq when available 
                //or 0-freq when source is dispatch (catches handled separately)
                //or freq of src node when src node was removed from instrumentation(equal freq for all out-edges)
                double edgeFreq = 0;
                if (nodesToIgnore.find(srcNode)!=nodesToIgnore.end()) {
                    assert(srcNode->getExecCount()!= -1 || (srcNode->isCatchBlock() && dt->dominates(node, srcNode)) || (hasCatch(node) && srcNode->isEmpty()));
                    edgeFreq = srcNode->getExecCount();
                } else if (srcNode->isBlockNode()) {
                    edgeFreq = edgeFreqs[edge->getId()];
                }
                if (debug) {
                    Log::out()<<"\t\t\tedge id="<<edge->getId()<<" from="; FlowGraph::printLabel(Log::out(), srcNode);
                    Log::out()<<" freq="<<edgeFreq<<std::endl;
                }
                if (edgeFreq == -1) {
#ifdef _DEBUG
                    Node* head = lt->getLoopHeader(node, false); //catch loops are not instrumented
                    assert(head != NULL && (head->isCatchBlock() || head->isDispatchNode() || hasCatch(head)));
#endif
                    edgeFreq = 0;
                    setEdgeFreq(edgeFreqs, edge, edgeFreq, debug);
                }
                nodeFreq +=edgeFreq;
            }
        }
        if (debug) {
            Log::out()<<"\t\t\tnode freq="<<nodeFreq<<std::endl;
        }
        assert(nodeFreq!=-1);
        node->setExecCount(nodeFreq);
        if (node->isExitNode()) {
            continue;
        }
        //now we able to calculate all outgoing edge freqs for current node
        //if node is bb -> only 1 edge is allowed to be without freq here and we will fix it
        //if node is dispatch -> use catch node freqs
        //if node was ignored -> equal freq for every bb edge, 0 to dispatch edge
        double freqLeft = nodeFreq;
        if (ignored) {
            if ( outEdges.size()==1 ) {
                Edge* edge = *outEdges.begin();
                setEdgeFreq(edgeFreqs, edge, nodeFreq, debug);
            } else {
                double freqPerBB = nodeFreq / (outEdges.size() - (node->getExceptionEdge() == NULL ? 0 : 1));
                for (Edges::const_iterator it = outEdges.begin(), end = outEdges.end(); it!=end; ++it) {
                    Edge* edge = *it;
                    setEdgeFreq(edgeFreqs, edge, edge->getTargetNode()->isBlockNode() ? freqPerBB : 0, debug);
                }
            }
        } else if (node->isBlockNode()) {
            Edge* notEstimatedEdge = NULL;
            for (Edges::const_iterator it2 = outEdges.begin(), end2 = outEdges.end(); it2!=end2; ++it2) {
                Edge* edge = *it2;
                double edgeFreq = edgeFreqs[edge->getId()];
                if (edgeFreq != -1) {
                    freqLeft-=edgeFreq;
                } else {
                    assert(notEstimatedEdge==NULL || ignored); // for ignored nodes we can't estimate every edge
                    if (notEstimatedEdge == NULL) {
                        notEstimatedEdge = edge;   
                    } else if (notEstimatedEdge->getTargetNode()->isDispatchNode()) {
                        setEdgeFreq(edgeFreqs, notEstimatedEdge, 0, debug);
                        notEstimatedEdge = edge;
                    } else {
                        setEdgeFreq(edgeFreqs, edge, 0, debug);
                    }
                }
            }
            if (notEstimatedEdge!=NULL) {
                freqLeft = freqLeft < 0 ? 0 : freqLeft;
                setEdgeFreq(edgeFreqs, notEstimatedEdge, freqLeft, debug);
            } 
        } else if (node->isDispatchNode()) {
            //for all not estimated on step 2.2 D->X edges set edge prob to min (0). 
            for (Edges::const_iterator it = outEdges.begin(), end = outEdges.end(); it!=end; ++it) {
                Edge* edge = *it;
                double _freq = edgeFreqs[edge->getId()];
                if (_freq==-1) {
                    setEdgeFreq(edgeFreqs, edge, 0, false);
                }     
            }
        } 
    }
    
#if 0
    //2.99 debug check : check that every child node in dom-tree that is in the same loop as parent
    //has nodeFreq <= parent freq
    if (debug) {
        Log::out()<<"Running check using dominator tree.."<<std::endl;
    }
    uint32 rootHeight = dt->getHeight();
    for (StlVector<Node*>::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it, n++) {
        Node* node = *it;
        DominatorNode* dNode = dt->getDominatorNode(node);
        if (dNode->getHeight()!=rootHeight) {
            Node* parent = dNode->getParent()->getNode();
            //have wrong nodes freqs on BB->DN (see 2.3), but edge freqs are OK.
            //bool parentIgnored = nodesToIgnore.find(parent)!=nodesToIgnore.end();
            if (node->isBlockNode() && parent->isBlockNode() 
                && lt->getLoopHeader(parent, false) == lt->getLoopHeader(node, false))
            { 
                assert(parent->getExecCount() >= node->getExecCount());
            }
        }
    }
#endif
    
    //3. calculate edge probs using edge freqs. 
    if (debug) {
        Log::out()<<"Calculating edge probs using freqs.."<<std::endl;
    }
    for (StlVector<Node*>::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it, n++) {
        Node* node = *it;
        double nodeFreq = node->getExecCount();
        assert(nodeFreq!=-1);
        const Edges& edges = node->getOutEdges();
        for (Edges::const_iterator it2 = edges.begin(), end2 = edges.end(); it2!=end2; ++it2) {
            Edge* edge = *it2;
            double edgeFreq =edgeFreqs[edge->getId()];
            assert(edgeFreq!=-1);
            double edgeProb = nodeFreq == 0 ? 0 : edgeFreq / nodeFreq ;
//            assert(edgeProb >= 0 && edgeProb <= 1);
            edge->setEdgeProb(edgeProb);
        }
    }
    if (debug) {
        Log::out()<<"Finished probs calculation";
    }
    return true;
}

//
// Compute the checksum contribution of the given Edge
//
static uint32 _computeCheckSum(Node* node, const StlSet<Node*>& nodesToIgnore, StlVector<bool>& flags, uint32 depth, bool debug) {
    assert(flags[node->getId()] == false);
    flags[node->getId()] = true;
    //node is removed from checksum calculation if 
    // 1) it's in ignore list
    // 2) it's dispatch or catch -> due to the fact that ignore-list nodes can add more dispatches & catches(finally-blocks)
    bool ignored = nodesToIgnore.find(node)!=nodesToIgnore.end();
    bool skipped = ignored || node->getOutDegree()==1
        || (node->isCatchBlock() && node->isEmpty()) || node->isDispatchNode() || node->isExitNode(); 
    uint32 dSum = skipped ? 0 : depth;
    uint32 childDepth = skipped ? depth : depth + 1;
    const Edges& outEdges = node->getOutEdges();
    uint32 childsSum = 0;
    for (Edges::const_iterator it = outEdges.begin(), end = outEdges.end(); it!=end; ++it) {
        Edge* edge = *it;
        Node* childNode = edge->getTargetNode();
        //visit only those not visited child nodes that are not dispatch edges of ignored nodes
        if (flags[childNode->getId()] == false && !(ignored && childNode->isDispatchNode())) { 
            childsSum+=_computeCheckSum(childNode, nodesToIgnore, flags, childDepth, debug);
        } 
    }
    if (debug){
        Log::out()<< "\t<checksum calculation: node:"; FlowGraph::printLabel(Log::out(), node); 
        Log::out() << "=+"<<dSum<<"  depth="<<depth<< " child sum="<<childsSum<<std::endl;
    }
    return dSum+childsSum;
}

static uint32 computeCheckSum( MemoryManager& mm, ControlFlowGraph& cfg,  const StlSet<Node*>& nodesToIgnore) {
    bool debug = Log::isEnabled();
    if (debug) {
        Log::out()<< "calculating checksum.." << std::endl;
    }
    StlVector<bool> flags(mm, cfg.getMaxNodeId(), false);
    uint32 checkSum = _computeCheckSum(cfg.getEntryNode(), nodesToIgnore, flags, 1, debug);
    if( debug){
        Log::out()<< "checkSum= "<<checkSum<<std::endl;
    }
    return checkSum;
}


static Node* selectNodeToInstrument(IRManager& irm, Edge* edge) {
    ControlFlowGraph& fg = irm.getFlowGraph();
    Node* srcNode = edge->getSourceNode();
    Node* dstNode = edge->getTargetNode();
    Node* result = NULL;
    assert(srcNode->isBlockNode() && dstNode->isBlockNode());
    if (srcNode->isCatchBlock()) { 
        // catch blocks handled separately. We must add counter only after catch inst
        assert(srcNode->hasOnlyOneSuccEdge() || hasCatch(srcNode));
        if (srcNode->hasOnlyOneSuccEdge() && hasCatch(srcNode)) { //filter-out catches with ret -> instrument srcNode 
            result = srcNode;
        } else {
            result = dstNode;
        }
    } else { 
        result = srcNode->hasOnlyOneSuccEdge() ? srcNode : dstNode;
    } 
    if( result->hasTwoOrMorePredEdges() ){
        // Splice a new block on edge from srcNode to dstNode and put 
        // instrumentation code there.
        result = fg.spliceBlockOnEdge(edge, irm.getInstFactory().makeLabel());
    } 
    return result;
}

static bool hasCatch( Node* node ) {
    Inst* inst = (Inst*)node->getFirstInst();
    assert(inst->isLabel());
    // Op_Catch is allowed to be not in the second position only (after label inst) 
    // but in arbitrary position if Op_Phi insts present.
    do {
        inst = inst->getNextInst();
    } while (inst!=NULL && inst->getOpcode() == Op_Phi);
    return inst != NULL && inst->getOpcode() == Op_Catch;
}

/*static bool hasCounter( Node* node ) {
    for (Inst* inst =  node->getFirstInst()->next(); inst!=node->getFirstInst(); inst = inst->next()) {
        if (inst->getOpcode() == Op_IncCounter) {
            return true;
        }
    }
    return false;
    
}*/

static void selectedNodesToIgnore(IRManager& irm, LoopTree* lt, StlSet<Node*>& result) {
    ControlFlowGraph& fg = irm.getFlowGraph();
    bool debug = Log::isEnabled();
    if (debug) {
        Log::out() << "Selecting nodes to ignore:"<<std::endl;
    }
    const Nodes& nodes = fg.getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        //first pattern to ignore: class initializers
        Node* loopHead = lt->getLoopHeader(node, false);
        bool inCatchLoop = loopHead!=NULL && (loopHead->isCatchBlock() || loopHead->isDispatchNode() || hasCatch(loopHead));
        if (node->isBlockNode() 
            && node->getOutDegree() == 2  && node->getExceptionEdge()!=NULL 
            && ((Inst*)node->getLastInst())->getOpcode() == Op_InitType) 
        {
            result.insert(node);
            if (debug) {
                Log::out() << "\tIgnoring Op_InitType node:";FlowGraph::printLabel(Log::out(), node);Log::out() << std::endl;
            }
        } else if (inCatchLoop && node->isEmpty()) {
            //common block for multiple catches in catch loop -> add to ignore
            result.insert(node);
            if (debug) {
                Log::out() << "\tIgnoring catch-loop preheader node:";FlowGraph::printLabel(Log::out(), node);Log::out() << std::endl;
            }
        } else if (node->isCatchBlock()) {  //avoid mon-exit loops (catch loops) instrumentation
            Node* blockWithCatchInst = NULL;
            if (hasCatch(node)) {
                blockWithCatchInst = node;
            } else {
                assert(node->hasOnlyOneSuccEdge());
                Edge* e =  *node->getOutEdges().begin();
                blockWithCatchInst = e->getTargetNode();
                if (blockWithCatchInst->hasTwoOrMorePredEdges()) {
                    result.insert(node);
                    if (debug) {
                        Log::out() << "\tIgnoring '1-catch-inst*N-catches' node:";FlowGraph::printLabel(Log::out(), node);Log::out() << std::endl;
                    }
                } 
            }
        }
    }
    if (debug) {
        Log::out() << "DONE. ignored nodes: " << result.size() << std::endl;
    }
}

static void _selectEdgesToInstrument(Node* srcNode, LoopTree* loopTree, Edges& result,
                                     const StlSet<Node*>& nodesToIgnore,  StlVector<bool>& flags) 
{
    bool profileDispatchEdges = true; //TODO: cmd-line param

    assert(flags[srcNode->getId()] == false);
    flags[srcNode->getId()] = true;
    const Edges& oEdges = srcNode->getOutEdges();
    bool ignored = nodesToIgnore.find(srcNode)!=nodesToIgnore.end();
    if (!ignored && srcNode->isBlockNode()) {
        Edge* skipEdge = NULL; //instrument all edges except one
        if (srcNode->isCatchBlock()) {
            // for a catch block we can have only 1 successor edge if catch inst is in next block
            // if catch inst is in catch block -> instrument every outgoing edge to be able to restore catch
            // node frequency easily.
            assert(hasCatch(srcNode) || srcNode->hasOnlyOneSuccEdge());
        } else {
            for(Edges::const_iterator it = oEdges.begin(); it!= oEdges.end(); ++it ){
                Edge* edge = *it;
                Node* targetNode = edge->getTargetNode();
                bool isBackEdge = loopTree->isBackEdge(edge);
                //we must instrument backedges to be able to restore loop iteration count
                if( isBackEdge || (!targetNode->isBlockNode() && profileDispatchEdges) ){
                    // We run into a dispatch node, so we have to instrument all the other edges.
                    skipEdge = NULL;
                    break;
                }
                // It is profitable to instrument along the loop exit edge.
                if (skipEdge == NULL || loopTree->isLoopExit(skipEdge)) {
                    skipEdge = edge; 
                }  
            }
        }
        bool debug = Log::isEnabled();
        for(Edges::const_iterator it = oEdges.begin(); it!= oEdges.end(); ++it ){
            Edge* edge = *it;
            Node* targetNode = edge->getTargetNode();
            if( targetNode->isBlockNode() && edge!=skipEdge){
                result.push_back(edge);
                if (debug) {
                    Log::out()<<"\tedge id="<<edge->getId()<<" ("; FlowGraph::printLabel(Log::out(), edge->getSourceNode());
                    Log::out()<<"->"; FlowGraph::printLabel(Log::out(), edge->getTargetNode());Log::out()<<")"<<std::endl;
                }
            }
        }
    }
    // process all child nodes. for ignored nodes we do not process dispatches 
    // because new catch block can be created that must also be ignored
    for(Edges::const_iterator it = oEdges.begin(); it!= oEdges.end(); ++it ){
        Edge* edge = *it;
        Node* targetNode = edge->getTargetNode();
        if (flags[targetNode->getId()] == false && !(ignored && targetNode->isDispatchNode())) {
            _selectEdgesToInstrument(targetNode, loopTree, result, nodesToIgnore, flags);
        }
    }
}

static void selectEdgesToInstrument(MemoryManager& mm, IRManager& irm, Edges& result,  
                                    const StlSet<Node*>& nodesToIgnore) 
{
    LoopTree* loopTree = irm.getLoopTree(); 
    ControlFlowGraph& fg = irm.getFlowGraph();
    StlVector<bool> flags(mm, fg.getMaxNodeId(), false);

    bool debug = Log::isEnabled();
    if (debug) {
        Log::out() << "List of edges to instrument: "<<std::endl;
    }
    _selectEdgesToInstrument(fg.getEntryNode(), loopTree, result, nodesToIgnore,flags);
    if (debug) {
        Log::out() << "End of list.."<<std::endl;
    }
}

//
// Return true if the method represented by <cfg> is trivial.
// A method is trivial if it is a leaf method, and it has no if-then-else
// statement.
//
static bool isMethodTrivial( ControlFlowGraph& cfg ) {
    const Nodes& nodes = cfg.getNodes();
    Nodes::const_iterator niter;

    for( niter = nodes.begin(); niter != nodes.end(); niter++ ){
        Node* node = *niter;
        if( !node->isBlockNode() || node->isEmpty() ){
            continue;
        }
        Inst* last = (Inst*)node->getLastInst();
        // This method is not a leaf method.
        if( last->getOpcode() >= Op_DirectCall && last->getOpcode() <= Op_IntrinsicCall ){
            return false;
        }
        Edges::const_iterator eiter;
        const Edges& oEdges = node->getOutEdges();
        int succ = 0;

        for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
            Edge* edge = *eiter;
            if( edge->getTargetNode()->isBlockNode() ){
                succ++;
            }
        }
        // This method has if-then-else statement(s).
        if( succ > 1 ){
            return false;
        }
    }
    return true;
}

static uint32 genKey( uint32 pos, Edge* edge, bool bcLevel, bool debug)  {
    uint32 key = 0;
    if (bcLevel) {
        assert(0); //TODO:
    } else {
        //TODO: this algorithm is not 100% effective: we can't rely on edges order in CFG
        uint32 edgePos = 0;
        const Edges& edges = edge->getSourceNode()->getOutEdges();
        for (Edges::const_iterator it = edges.begin(), end = edges.end(); it!=end; ++it) {
            Edge* outEdge = *it;
            if (outEdge == edge) {
                break;
            }
            if (outEdge->isDispatchEdge()) { //dispatch edge order is the reason of the most of the edge ordering errors 
                continue;
            }
            edgePos++;
        }

        key = pos + (edgePos << 16);
    }
    if (debug) {
        Log::out()<<"\t\t key for edge with id="<<edge->getId()<<" ("; FlowGraph::printLabel(Log::out(), edge->getSourceNode());
        Log::out() << "->"; FlowGraph::printLabel(Log::out(), edge->getTargetNode()); Log::out()<<") is "<< key << std::endl;
    }
    return key;
}

} //namespace
