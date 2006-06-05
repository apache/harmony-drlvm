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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.14.12.2.4.4 $
 */

#include "Ia32Inst.h"
#include "Ia32GCSafePoints.h"
#include "Timer.h"
#include "BitSet.h"


namespace Jitrino
{

#define LIVENESS_FILTER_INST_INTERVAL 128
#define LIVENESS_FILTER_OBJ_INST_INTERVAL 64

static Timer *phase0Timer=NULL, *phase1Timer=NULL, *phase1Checker = NULL, *phase2Timer=NULL, *phase2Checker = NULL, *saveResultsTimer=NULL;

namespace Ia32 {

// does nothing except allows to debugger to not loose local stack if method called is last in code
static void dbg_point() {}

// Helper function, removes element idx from vector in O(1) time
// Moves last element in vector to idx and reduce size by 1
// WARN: elements order could be changed!
static void removeElementAt(GCSafePointPairs& pairs, uint32 idx) {
    assert(!pairs.empty());
    uint32 newSize = pairs.size() - 1;
    assert(idx<=newSize);
    if (idx < newSize) {
        pairs[idx] = pairs[newSize];
    }
    pairs.resize(newSize);
}

void GCSafePointsInfo::removePairByMPtrOpnd(GCSafePointPairs& pairs, const Opnd* mptr) const {
#ifdef _DEBUG_
    uint32 nPairs = 0;
#endif 
    for (int i = pairs.size(); --i>=0;) {
        MPtrPair& p = pairs[i];
        if (p.mptr == mptr) {
            removeElementAt(pairs, i);
#ifdef _DEBUG_
            nPairs++;
#else
            break; //only one pair in one point for mptr is allowed !
#endif
        }
    }
#ifdef _DEBUG_
    assert(nPairs <= 1);
#endif
}

MPtrPair* GCSafePointsInfo::findPairByMPtrOpnd(GCSafePointPairs& pairs, const Opnd* mptr) {
    for (GCSafePointPairs::iterator it = pairs.begin(), end = pairs.end(); it!=end; ++it) {
        MPtrPair& p = *it;
        if(p.getMptr() == mptr) {
            return &p;
        }
    }
    return NULL;
}

GCSafePointsInfo::GCSafePointsInfo(MemoryManager& _mm, IRManager& _irm, Mode _mode) :
mm(_mm), irm(_irm), pairsByNodeId(_mm), pairsByGCSafePointInstId(mm), 
livenessFilter(mm), ambiguityFilters(mm), staticMptrs(mm), allowMerging(false), opndsAdded(0), instsAdded(0), mode(_mode)
{
    _calculate();
}


bool GCSafePointsInfo::graphHasSafePoints(const IRManager& irm){
    const Nodes& nodes = irm.getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)) {
            if (blockHasSafePoints((BasicBlock*)node)) {
                return true;
            }
        }
    }
    return false;
}

bool GCSafePointsInfo::blockHasSafePoints(const BasicBlock* b) {
    const Insts& insts = b->getInsts();
    for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
        if (isGCSafePoint(inst)) {
            return true;
        }
    }
    return false;
}


uint32 GCSafePointsInfo::getNumSafePointsInBlock(const BasicBlock* b, const GCSafePointPairsMap* pairsMap ) {
    uint32 n = 0;
    const Insts& insts = b->getInsts();
    for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
        if (isGCSafePoint(inst)) {
            if (pairsMap!=NULL) { // if pairs!=NULL counts only gcpoints with non-empty pairs
                GCSafePointPairsMap::const_iterator it = pairsMap->find(inst->getId());
                assert(it!=pairsMap->end());
                GCSafePointPairs* pairs = it->second;
                if (pairs->empty()) {
                    continue;
                }
            }
            n++;
        }
    }
    return n;
}


void GCSafePointsInfo::_calculate() {
    assert(pairsByNodeId.empty());
    assert(pairsByGCSafePointInstId.empty());

    //check if there are no safepoints in cfg at all
    if (!graphHasSafePoints(irm)) {
        return;
    } 
    irm.updateLivenessInfo();
    irm.updateLoopInfo(); //to use _hasLoops property

    insertLivenessFilters();
    allowMerging = true;
    calculateMPtrs();
    if (mode == MODE_2_CALC_OFFSETS) { 
        assert(opndsAdded == 0);
        return;
    }
    if ( opndsAdded > 0 ) {
        irm.invalidateLivenessInfo();
        irm.updateLivenessInfo();
    }
    allowMerging  = false;
    filterLiveMPtrsOnGCSafePoints();
}


void GCSafePointsInfo::insertLivenessFilters() {
    PhaseTimer tm(phase0Timer, "ia32::gcpointsinfo::phase0");
    const Nodes& nodes = irm.getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)) {
            const Insts& insts = ((BasicBlock*)node)->getInsts();
            if (insts.getCount() < LIVENESS_FILTER_INST_INTERVAL) {
                continue;
            }
            uint32 objOps  = 0;
            for (Inst * inst=insts.getLast(); inst != NULL ; inst=insts.getPrev(inst)) {
                Opnd* opnd = inst->getOpnd(0); // VSH: 0 - ???? 
                if (opnd->getType()->isObject() || opnd->getType()->isManagedPtr()) {
                    objOps++;
                    if (objOps == LIVENESS_FILTER_OBJ_INST_INTERVAL) {
                        break;
                    }
                }
            }
            if (objOps < LIVENESS_FILTER_OBJ_INST_INTERVAL) {
                continue;
            }
             // insert liveness filter for every n-th inst in block
            int nFilters = insts.getCount() / LIVENESS_FILTER_INST_INTERVAL;
            LiveSet* ls = new (mm) LiveSet(mm, irm.getOpndCount());
            irm.getLiveAtExit(node, *ls);
            int nFiltersLeft = nFilters;
            int instsInInterval = 0;
            for (Inst * inst=insts.getLast(); nFiltersLeft > 0; inst=insts.getPrev(inst)) {
                assert(inst!=NULL);
                irm.updateLiveness(inst, *ls);
                instsInInterval++;
                if (instsInInterval == LIVENESS_FILTER_INST_INTERVAL) {
                    nFiltersLeft--;
                    instsInInterval = 0;
                    setLivenessFilter(inst, ls);
                    if (nFiltersLeft != 0) {
                        ls = new (mm) LiveSet(*ls);
                    }
                }
            }
        }
    }
}

void GCSafePointsInfo::setLivenessFilter(const Inst* inst, const LiveSet* ls) {
    livenessFilter[inst] = ls;
}

const LiveSet* GCSafePointsInfo::findLivenessFilter(const Inst* inst) const {
    StlMap<const Inst*, const LiveSet*>::const_iterator it = livenessFilter.find(inst);
    if (it == livenessFilter.end()) {
        return NULL;
    }
    return it->second;
}

void GCSafePointsInfo::calculateMPtrs() {
    PhaseTimer tm(phase1Timer, "ia32::gcpointsinfo::phase1");
    assert(pairsByNodeId.empty());
    pairsByNodeId.resize(irm.getMaxNodeId());
    Nodes nodes(mm);
    irm.getNodes(nodes, CFG::OrderType_Topological);
    assert(nodes.back() == irm.getExitNode());
    nodes.pop_back();
    for (Nodes::iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        const Edges& outEdges = node->getEdges(Direction_Out);
        if (outEdges.getCount() == 1 && outEdges.getFirst()->getNode(Direction_Head) == irm.getExitNode()) {
            if (!node->hasKind(Node::Kind_BasicBlock) || !blockHasSafePoints((BasicBlock*)node)) {
                *it = NULL;
                continue;
            } 
        }
        pairsByNodeId[node->getId()] = new (mm) GCSafePointPairs(mm);
    }
    nodes.erase(std::remove(nodes.begin(), nodes.end(), (Node*)NULL), nodes.end());

    GCSafePointPairs tmpPairs(mm);
    uint32 nIterations = irm.getMaxLoopDepth() + 1;
    bool changed = true;
    bool restart = false;

#ifdef _DEBUG 
    nIterations++; //one more extra iteration to check on debug that nothing changed
#endif 
    for (uint32 iteration = 0; iteration < nIterations ; ) {
        changed = false;
        for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
            Node* node = *it;
            tmpPairs.clear();
            uint32 instsBefore = instsAdded;
            derivePairsOnEntry(node, tmpPairs);
            if (instsBefore!=instsAdded) {
                restart = true;
                break;
            }
            GCSafePointPairs& nodePairs = *pairsByNodeId[node->getId()];
            if (node->hasKind(Node::Kind_BasicBlock)) {
                const Insts& insts=((BasicBlock*)node)->getInsts();
                for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
                    updatePairsOnInst(inst, tmpPairs);
                    if (isGCSafePoint(inst)) { //saving results on safepoint
                        GCSafePointPairs* instPairs = NULL;
                        instPairs = pairsByGCSafePointInstId[inst->getId()];
                        if (instPairs == NULL) {
                            instPairs = new (mm) GCSafePointPairs(mm);
                            pairsByGCSafePointInstId[inst->getId()] = instPairs;
                        }
                        instPairs->clear();
                        instPairs->insert(instPairs->end(), tmpPairs.begin(), tmpPairs.end());
                    }
                }
                if (iteration == 0 || !hasEqualElements(nodePairs, tmpPairs)) {
                    changed = true;
                    nodePairs.swap(tmpPairs);
                }
            } else {
                nodePairs.swap(tmpPairs);
            }
        }
        if (restart) {  //clear all cached pairs and restart all iterations
            for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
                Node* node = *it;
                GCSafePointPairs& pairs = *pairsByNodeId[node->getId()];
                pairs.clear();
            }
            iteration = 0;
            restart = false;
            continue;
        }
        if (!changed) {
            break;
        }
        iteration++;
    }
#ifdef _DEBUG
    assert(!changed);
    checkPairsOnNodeExits(nodes);
#endif
}

static inline int adjustOffsets(int32 offsetBefore, int32 dOffset) {
    if (offsetBefore == MPTR_OFFSET_UNKNOWN || dOffset == MPTR_OFFSET_UNKNOWN) {
        return MPTR_OFFSET_UNKNOWN;
    }
    int res = offsetBefore + dOffset;
    assert(res>=0);
    return res;
}


int32 GCSafePointsInfo::getOffsetFromImmediate(Opnd* offsetOpnd) const {
    if (offsetOpnd->isPlacedIn(OpndKind_Immediate)) {
        if (offsetOpnd->getImmValue() == 0 && offsetOpnd->getRuntimeInfo()!=NULL) {
            irm.resolveRuntimeInfo(offsetOpnd);
        }
        return (int32)offsetOpnd->getImmValue();
    }
    return MPTR_OFFSET_UNKNOWN;
}


void GCSafePointsInfo::runLivenessFilter(Inst* inst, GCSafePointPairs& pairs) const {
    if (!pairs.empty()) {
        const LiveSet* livenessFilter = findLivenessFilter(inst);
        if (livenessFilter!=NULL) {
            for (int i = pairs.size(); --i>=0;) {
                MPtrPair& p = pairs[i];
                if (!livenessFilter->isLive(p.mptr)) {
                    removeElementAt(pairs, i);
                }
            }
        }
    }
}

Opnd* GCSafePointsInfo::getBaseAccordingMode(Opnd* opnd) const {
    //MODE_2_CALC_OFFSETS does not have valid base info, because it does not resolves ambiguity!
    return mode == MODE_1_FIX_BASES ? opnd : NULL;
}    

void GCSafePointsInfo::processStaticFieldMptr(Opnd* opnd, Opnd* fromOpnd, bool forceStatic) {
    StlSet<Opnd*>::iterator sit = staticMptrs.find(fromOpnd);
    if (sit==staticMptrs.end()) {
        Opnd::RuntimeInfo* info = fromOpnd->isPlacedIn(OpndKind_Imm) ? fromOpnd->getRuntimeInfo() : NULL;
        bool isStaticFieldMptr = info!=NULL && info->getKind() == Opnd::RuntimeInfo::Kind_StaticFieldAddress;
        if (!isStaticFieldMptr && !forceStatic) {
            return;
        } 
        assert(isStaticFieldMptr);
        staticMptrs.insert(fromOpnd);
    }
    if (fromOpnd!=opnd) {
        staticMptrs.insert(opnd);
    }
}


void GCSafePointsInfo::updateMptrInfoInPairs(GCSafePointPairs& res, Opnd* newMptr, Opnd* fromOpnd, int offset, bool fromOpndIsBase) {
    if (fromOpndIsBase) { //from is base
        removePairByMPtrOpnd(res, newMptr);
        assert(staticMptrs.find(newMptr) == staticMptrs.end());
        MPtrPair p(newMptr, getBaseAccordingMode(fromOpnd), offset);
        res.push_back(p);
    } else { //fromOpnd is mptr
        MPtrPair* fromPair = findPairByMPtrOpnd(res, fromOpnd);
        if (fromPair != NULL) {
            int newOffset = adjustOffsets(fromPair->offset, offset);
            //remove old pair, add new
            MPtrPair* pair = newMptr == fromOpnd ? fromPair : findPairByMPtrOpnd(res, newMptr);
            if (pair != NULL) { //reuse old pair
                pair->offset = newOffset;
                pair->base = fromPair->base;
                pair->mptr = newMptr;
            } else { // new mptr, was not used before
                MPtrPair toPair(newMptr, getBaseAccordingMode(fromPair->base), newOffset);
                res.push_back(toPair);
            }
        } else {
            processStaticFieldMptr(newMptr, fromOpnd, true);
            dbg_point();
        }
    }
}


void GCSafePointsInfo::updatePairsOnInst(Inst* inst, GCSafePointPairs& res) {
    runLivenessFilter(inst, res);//filter pairs with dead mptrs from list

    Inst::Opnds opnds(inst, Inst::OpndRole_Explicit | Inst::OpndRole_Auxilary |Inst::OpndRole_UseDef);
    uint32 defIndex = opnds.begin();
    uint32 useIndex1 = opnds.next(defIndex);

    if (defIndex >= opnds.end() || useIndex1 >= opnds.end() 
        || (inst->getOpndRoles(defIndex) & Inst::OpndRole_Def) == 0 
        || (inst->getOpndRoles(useIndex1) & Inst::OpndRole_Use) == 0 )
    {
        return;
    }
    Opnd* opnd = inst->getOpnd(defIndex);
    Type* opndType = opnd->getType();
    if (!opndType->isObject() && !opndType->isManagedPtr()) {
        return;
    }
    if (mode == MODE_1_FIX_BASES) { //3 addr form
        if (opndType->isObject()) { // MODE_1_FIX_BASES -> obj & mptr types are not coalesced
            StlMap<Inst*, Opnd*>::const_iterator ait;
            if (!ambiguityFilters.empty() && ((ait = ambiguityFilters.find(inst))!=ambiguityFilters.end())) {
                //mptr ambiguity filter -> replace old base with new(opnd) in pairs
                const Opnd* mptr = ait->second;
                MPtrPair* pair =  findPairByMPtrOpnd(res, mptr);
#ifdef _DEBUG
                Opnd* oldBase = inst->getOpnd(useIndex1); //inst is: newBase = oldBase
                assert(pair!=NULL);
                assert(pair->getBase() == oldBase || pair->getBase() == opnd); // ==opnd-> already resolved in dominant block
#endif

                pair->base = opnd;
            }
        } else { //def of mptr
            Opnd* fromOpnd = inst->getOpnd(useIndex1);
            int32 offset = MPTR_OFFSET_UNKNOWN;
            assert(fromOpnd->getType()->isObject() || fromOpnd->getType()->isManagedPtr());
            uint32 useIndex2 = opnds.next(useIndex1);
            if (useIndex2 < opnds.end()) {
                Opnd* offsetOpnd = inst->getOpnd(useIndex2); 
                offset = getOffsetFromImmediate(offsetOpnd);
            }
            updateMptrInfoInPairs(res, opnd, fromOpnd, offset, fromOpnd->getType()->isObject());
            dbg_point();
        }
    } else { // mode == MODE_2_CALC_OFFSETS   - 2 addr form
        //we can't rely on base/mptr type info here.
        //algorithm:
        // if fromOpnd is Mptr -> result is Mptr
        // else -> analyze inst
        Opnd* fromOpnd = inst->getOpnd(useIndex1);
        bool fromIsMptrOrObj = fromOpnd->getType()->isObject() || fromOpnd->getType()->isManagedPtr();
        MPtrPair* fromPair = fromIsMptrOrObj ? findPairByMPtrOpnd(res, fromOpnd) : NULL;
        if (fromPair!=NULL) {  // ok result is mptr
            int32 offset = 0;
            if (!fromIsMptrOrObj) {
                assert(inst->getMnemonic() == Mnemonic_ADD);
                if (fromOpnd->isPlacedIn(OpndKind_Immediate)) {
                    Opnd* offsetOpnd = fromOpnd; 
                    offset = getOffsetFromImmediate(offsetOpnd);
                } else {
                    assert(fromOpnd->isPlacedIn(OpndKind_Memory) || fromOpnd->isPlacedIn(OpndKind_Reg));
                    offset = MPTR_OFFSET_UNKNOWN;
                }
            }
            updateMptrInfoInPairs(res, opnd, fromOpnd, offset, fromPair == NULL);
            dbg_point();
        } else { // try to detect if opnd is mptr or obj
            if (!inst->hasKind(Inst::Kind_ControlTransferInst)) {
                int32 offset = 0;
                bool createNewPair = false;
                if (fromIsMptrOrObj) {
                    MPtrPair* fromPair = findPairByMPtrOpnd(res, fromOpnd);
                    if (fromPair != NULL) {
                        offset = fromPair->getOffset();
                        createNewPair = true;
                    } else {
                        processStaticFieldMptr(opnd, fromOpnd, false);
                    }
                } else  if (staticMptrs.find(opnd) == staticMptrs.end()) {
                    if (inst->getMnemonic() == Mnemonic_ADD) {
						if (fromOpnd->isPlacedIn(OpndKind_Immediate)) {
							offset = getOffsetFromImmediate(fromOpnd); 
							assert(offset!=MPTR_OFFSET_UNKNOWN);
						} else {
							offset=MPTR_OFFSET_UNKNOWN;
						}
                        //when offset is added to base in 2 address form -> mptr is created
                        createNewPair = offset!=0;
                    } else if (inst->getMnemonic() == Mnemonic_LEA) {
                        assert(fromOpnd->isPlacedIn(OpndKind_Memory));
                        Opnd* scaleOpnd = fromOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Scale);
                        if (scaleOpnd == NULL) {
                            Opnd* displOpnd = fromOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
                            assert(displOpnd!=NULL);
                            offset = (int32)displOpnd->getImmValue();
                        } else {
                            offset = MPTR_OFFSET_UNKNOWN;
                        }
                        createNewPair = true;
                    } else {
                        assert(0);
                    }
                } 
                if (createNewPair) {
                    assert(staticMptrs.find(opnd) == staticMptrs.end());
                    MPtrPair p(opnd, NULL, offset);
                    res.push_back(p);
                }
            } 
        }
    } // else mode2
}



void GCSafePointsInfo::derivePairsOnEntry(const Node* node, GCSafePointPairs& res) {
    assert(res.empty());
    // optimization: filter out those mptrs that are not live at entry.
    // this situation is possible because of updatePairsOnInst() function does not track liveness
    // and allows dead operands in pairs for block end;
    const LiveSet* ls = irm.getLiveAtEntry(node); 
    const Edges& edges=node->getEdges(Direction_In);
    
    //step1: add all live pairs from pred edges into res
    for (Edge* edge= edges.getFirst(); edge!=NULL; edge=edges.getNext(edge)){
        Node * predNode =edge->getNode(Direction_Tail);
        const GCSafePointPairs& predPairs = *pairsByNodeId[predNode->getId()];
        res.reserve(res.size() + predPairs.size());
        for (GCSafePointPairs::const_iterator it = predPairs.begin(), end = predPairs.end(); it!=end; ++it) {
            const MPtrPair& predPair = *it;
            if (ls->isLive(predPair.mptr)) {
                res.push_back(predPair);//by value
            }
        }
    }

    //step 2: merge pairs with the same (mptr, base) and process ambiguos pairs
    bool needToFilterDeadPairs = false;
    uint32 instsAddedBefore = instsAdded;
    std::sort(res.begin(), res.end());
    for (uint32 i=0, n = res.size(); i < n; i++) {
        MPtrPair& p1 = res[i];
        Opnd* newBase = NULL; //new base opnd to merge ambiguos mptrs
        while (i+1 < n) {
            const MPtrPair& p2 = res[i+1];
            if (p1.mptr == p2.mptr) {
                if (p1.base == p2.base) { //same base & mptr -> merge offset
                    if (p1.offset != p2.offset) { 
                        p1.offset = MPTR_OFFSET_UNKNOWN;
                    }
                } else { //equal mptrs with a different bases -> add var=baseX to prevBlocks and replace pairs with new single pair
                    assert(mode == MODE_1_FIX_BASES);
                    assert(allowMerging);
                    if (newBase == NULL) { //first pass for this ambiguous mptrs fix them all
                        newBase = irm.newOpnd(p1.base->getType());
                        opndsAdded++;
                        uint32 dInsts = 0;
                        for (Edge* edge= edges.getFirst(); edge!=NULL; edge=edges.getNext(edge)) {
                            Node * predNode =edge->getNode(Direction_Tail);
                            GCSafePointPairs& predPairs = *pairsByNodeId[predNode->getId()];
                            for (GCSafePointPairs::iterator it = predPairs.begin(), end = predPairs.end(); it!=end; ++it) {
                                MPtrPair& predPair = *it;
                                if (predPair.mptr == p1.mptr) { //ok remove this pair, add vardef, add new pair
                                    Opnd* oldBase = predPair.base;
                                    assert(predNode->hasKind(Node::Kind_BasicBlock));
                                    BasicBlock* predBlock = (BasicBlock*)predNode;
                                    //add var def inst to predBlock
                                    Inst* baseDefInst = irm.newCopyPseudoInst(Mnemonic_MOV, newBase, oldBase);
                                    Inst* lastInst = predBlock->getInsts().getLast();
                                    if (lastInst!=NULL && lastInst->hasKind(Inst::Kind_ControlTransferInst)) {
                                        predBlock->prependInsts(baseDefInst, lastInst);
                                    } else {
                                        predBlock->appendInsts(baseDefInst, lastInst);
                                    }
                                    if (predPair.offset!=p1.offset) {
                                        p1.offset = MPTR_OFFSET_UNKNOWN;
                                    }
                                    dInsts++;
                                    ambiguityFilters[baseDefInst] = p1.mptr;
                                    //and now we should remove oldBase from next nodes in topological ordering
                                    //oldBase could be cached and merging process could be started for oldBase and newBase in inner loops heads
                                    //we will do it one time after the all predPairs is processed for the current node
                                    break;//go to next node processing
                                }
                            }
                        }
                        p1.base = newBase;
                        assert(dInsts > 1);
                        instsAdded+=dInsts;
                    }
                }
                needToFilterDeadPairs = true;
                i++;
            } else {
                break;
            }
        } //while
    }

    //step3:
    if (instsAdded==instsAddedBefore && needToFilterDeadPairs) { //if nstsAdded!=instsAddedBefore -> recalculation restarts
        //step 3: leave only p1(see step2) pairs
        //unique -> remove all pairs with equal mptr but the first
        GCSafePointPairs::iterator newEnd = std::unique(res.begin(), res.end(), &MPtrPair::equalMptrs); 
        res.resize(newEnd - res.begin());
    }


}

void GCSafePointsInfo::filterLiveMPtrsOnGCSafePoints() {
    PhaseTimer tm(phase2Timer, "ia32::gcpointsinfo::phase2");
    assert(!pairsByNodeId.empty());
    //unoptimized impl -> analyzing all blocks -> could be moved to GCMap
    const Nodes& nodes = irm.getNodes();
    LiveSet ls(mm, irm.getOpndCount());
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (!node->hasKind(Node::Kind_BasicBlock)) {
            continue;
        }
        uint32 nSafePoints = getNumSafePointsInBlock((BasicBlock*)node, &pairsByGCSafePointInstId);
        if (nSafePoints == 0) {
            continue;
        }
        // ok this basic block has safepoints        
        const Insts& insts=((BasicBlock*)node)->getInsts();
        uint32 nSafePointsTmp = nSafePoints;
        // remove pairs with dead mptr, use liveness to do it;
        ls.clear();
        irm.getLiveAtExit(node, ls);
        nSafePointsTmp = nSafePoints;
        for (Inst * inst=insts.getLast();;inst=insts.getPrev(inst)) {
            assert(inst!=NULL);
            if (isGCSafePoint(inst)) {
                GCSafePointPairs& pairs  = *pairsByGCSafePointInstId[inst->getId()];
                if (!pairs.empty()) {
                    nSafePointsTmp--;
                    for (int i =pairs.size(); --i>=0;) {
                        MPtrPair& p = pairs[i];
                        if (!ls.isLive(p.mptr)) {
                            removeElementAt(pairs, i);
                        }
                    }
                }
                if (nSafePointsTmp == 0) {
                    break;
                }
            }
            irm.updateLiveness(inst, ls); 
        }
    }
#ifdef _DEBUG
    checkPairsOnGCSafePoints();
#endif
}

/// checks that sets of mptrs that come to any node from any edge are equal
void GCSafePointsInfo::checkPairsOnNodeExits(const Nodes& nodes) const {
    PhaseTimer tm(phase1Checker, "ia32::gcpointsinfo::phase1Checker");
    assert(!pairsByNodeId.empty());
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it ) {
        Node* node = *it;
        const LiveSet* ls = irm.getLiveAtEntry(node);
        const Edges& edges=node->getEdges(Direction_In);
        if(edges.getCount() > 1) {
            Edge* edge1 = edges.getFirst();
            Node * predNode1 =edge1->getNode(Direction_Tail);
            const GCSafePointPairs& pairs1 = *pairsByNodeId[predNode1->getId()];
            //ensure that first edge mptr set is equal to edge2..N set
            for (Edge* edge2 = edges.getNext(edge1); edge2!=NULL; edge2=edges.getNext(edge2)){
                Node * predNode2 =edge2->getNode(Direction_Tail);
                const GCSafePointPairs& pairs2 = *pairsByNodeId[predNode2->getId()];
                //now check that for every mptr in pairs1 there is a pair in pairs2 with the same mptr
                for (uint32 i1 = 0, n1 = pairs1.size();i1<n1; i1++) {
                    const MPtrPair& p1 = pairs1[i1];
                    if (ls->isLive(p1.mptr)) {
                        for (uint32 i2 = 0; ; i2++) {
                            assert(i2 < pairs2.size());
                            const MPtrPair& p2 = pairs2[i2];
                            if (p1.mptr == p2.mptr) {
                                break;
                            }
                        }
                    }
                }
                // and vice versa
                for (uint32 i2 = 0, n2 = pairs2.size();i2<n2; i2++) {
                    const MPtrPair& p2 = pairs2[i2];
                    if (ls->isLive(p2.mptr)) {
                        for (uint32 i1 = 0; ; i1++) {
                            assert(i1 < pairs1.size());
                            const MPtrPair& p1 = pairs1[i1];
                            if (p1.mptr == p2.mptr) {
                                break;
                            }
                        }
                    }
                }
            } //for edge2...edge[N]
        }
    }
}

// checks that we've collected pairs for every live mptr on safe point
void GCSafePointsInfo::checkPairsOnGCSafePoints() const {
    if (mode == MODE_2_CALC_OFFSETS) { // check is done using type info. Type info is not available for mode2
        return;
    }
    PhaseTimer tm(phase2Checker, "ia32::gcpointsinfo::phase2Checker");
    const Nodes& nodes = irm.getNodes();
    uint32 nOpnds = irm.getOpndCount();
    LiveSet ls(mm, nOpnds);
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (!node->hasKind(Node::Kind_BasicBlock)) {
            continue;
        }
        uint32 nSafePoints = getNumSafePointsInBlock((BasicBlock*)node);
        if (nSafePoints== 0) {
            continue;
        }
        uint32 nSafePointsTmp = nSafePoints;
        const Insts& insts=((BasicBlock*)node)->getInsts();
        irm.getLiveAtExit(node, ls);
        for (Inst * inst=insts.getLast(); nSafePointsTmp > 0; inst=insts.getPrev(inst)) {
            assert(inst!=NULL);
            if (isGCSafePoint(inst)) {
                nSafePointsTmp--;
                GCSafePointPairsMap::const_iterator mit = pairsByGCSafePointInstId.find(inst->getId());
                assert(mit!=pairsByGCSafePointInstId.end());
                const GCSafePointPairs& pairs = *mit->second; 
                LiveSet::IterB liveOpnds(ls);
                for (int i = liveOpnds.getNext(); i != -1; i = liveOpnds.getNext()) {
                    Opnd* opnd = irm.getOpnd(i);
                    if (opnd->getType()->isManagedPtr()) {
                        if (staticMptrs.find(opnd)!= staticMptrs.end()) {
                            continue;
                        }
                        for (uint32 j=0; ; j++) {
                            assert(j<pairs.size());
                            const MPtrPair&  pair = pairs[j];
                            if (pair.mptr == opnd) {
                                break;
                            }
                        }
                    }
                } //for opnds
            }
            irm.updateLiveness(inst, ls);
        }
    }
}

static uint32 select_1st(const std::pair<uint32, GCSafePointPairs*>& p) {return p.first;}


void GCSafePointsInfo::dump(const char* stage) const {
    Log::cat_cg()->out()<<"========================================================================"<<std::endl;
    Log::cat_cg()->out()<<"__IR_DUMP_BEGIN__: pairs dump"<<std::endl;
    Log::cat_cg()->out()<<"========================================================================"<<std::endl;
    //sort by inst id
    const GCSafePointPairsMap& map = pairsByGCSafePointInstId;
    StlVector<uint32> insts(mm, map.size());
    std::transform(map.begin(), map.end(), insts.begin(), select_1st);
    std::sort(insts.begin(), insts.end());
    
    //for every inst sort by mptr id and dump
    for (size_t i = 0; i<insts.size(); i++) {
        uint32 id = insts[i];
        const GCSafePointPairs* pairs = map.find(id)->second;
        Log::cat_cg_gc()->out()<<"inst="<<id<<" num_pairs="<<pairs->size()<<std::endl;
        GCSafePointPairs cloned = *pairs;
        std::sort(cloned.begin(), cloned.end());
        for(size_t j=0; j<cloned.size(); j++) {
            MPtrPair& p = cloned[j];
            Log::cat_cg()->out()<<"    mptr="<< p.mptr->getFirstId()
                <<" base="<<(p.base!=NULL?(int)p.base->getFirstId():-1)
                <<" offset="<<p.offset<<std::endl;
        }
    }
    Log::cat_cg()->out()<<"========================================================================"<<std::endl;
    Log::cat_cg()->out()<<"__IR_DUMP_END__: pairs dump"<<std::endl;
    Log::cat_cg()->out()<<"========================================================================"<<std::endl;

}

#define MAX(a,b) (((a) > (b)) ? (a) : (b))

void GCPointsBaseLiveRangeFixer::runImpl() {
    const char* params = getParameters();
    bool disableStaticOffsets = params!=NULL && !strcmp(params, "disable_static_offsets");
    MemoryManager mm(MAX(512, irManager.getOpndCount()), "GCSafePointsMarker");
    GCSafePointsInfo info(mm, irManager, GCSafePointsInfo::MODE_1_FIX_BASES);
    
    if (Log::cat_cg()->isIREnabled()) {
        info.dump(getTagName());
    }

    if(!info.hasPairs()) {
        return;
    }
    PhaseTimer tm(saveResultsTimer, "ia32::gcpoints::saveResults");
    const Nodes& nodes = irManager.getNodes();
    StlVector<Opnd*> basesAndMptrs(mm); 
    StlVector<int32> offsets(mm);
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node *node = *it;
        if (!node->hasKind(Node::Kind_BasicBlock)) {
            continue;
        }
        BasicBlock* block = (BasicBlock*)node;
        const Insts& insts=block->getInsts();
        for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)) {
            if (IRManager::isGCSafePoint(inst)) {
                const GCSafePointPairs& pairs = info.getGCSafePointPairs(inst);
                if (pairs.empty()) {
                    continue;
                }
                sideEffect = SideEffect_InvalidatesLivenessInfo;
                basesAndMptrs.clear(); 
                offsets.clear();
                if (!disableStaticOffsets) {
                    //bases to adjust liveness info. No bases from pairs with valid static offsets get into this set
                    for (GCSafePointPairs::const_iterator it = pairs.begin(), end = pairs.end(); it!=end; ++it) {
                        const MPtrPair& p = *it;
                        //basesAndMptrs.push_back(p.getMptr()); 
                        //offsets.push_back(p.getOffset());
                        if (p.getOffset() == MPTR_OFFSET_UNKNOWN) { // adjust base live range
                            Opnd* base = p.getBase();
                            if (std::find(basesAndMptrs.begin(), basesAndMptrs.end(), base) == basesAndMptrs.end()) {
                                basesAndMptrs.push_back(p.getBase());
                                offsets.push_back(0);
                            }
                        }
                        
                    }
                } else { //ignore static offsets info, adjust live range for all bases
                    std::transform(pairs.begin(), pairs.end(), basesAndMptrs.begin(), std::mem_fun_ref(&MPtrPair::getBase));
                    std::sort(basesAndMptrs.begin(), basesAndMptrs.end());
                    StlVector<Opnd*>::iterator newEnd = std::unique(basesAndMptrs.begin(), basesAndMptrs.end());
                    basesAndMptrs.resize(newEnd - basesAndMptrs.begin());
                    std::fill(offsets.begin(), offsets.end(), 0);
                }                
                if (!basesAndMptrs.empty()) {
                    GCInfoPseudoInst* gcInst = irManager.newGCInfoPseudoInst(basesAndMptrs);
                    gcInst->desc = getTagName();
                    gcInst->offsets.resize(offsets.size());
                    std::copy(offsets.begin(), offsets.end(), gcInst->offsets.begin());
                    block->appendInsts(gcInst, inst);
                }
            } //if inst is gc safe point
        } //for insts
    }
}

}} //namespace

