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
/**
 * @author Vyacheslav P. Shakin
 * @version $Revision: 1.8.12.1.4.3 $
 */

#include "Ia32IRManager.h"
#include "Ia32Printer.h"
#include "Interval.h"

namespace Jitrino
{
namespace Ia32{


//========================================================================================

/**
class SimpleStackOpndCoalescer

Removes redundant stack operands and translates redundant 
stack-stack CopyPseudoInsts to copies of the same operand.
The latter will not be expanded by the copy expansion pass 
and thus is not emitted.

SimpleStackOpndCoalescer "removes" copies if:
- both operands are StackAutoLayout 
- both are of the same size
- live-ranges of the operands do not overlap

Live-ranges are approximated by sets of spans like in 
the bin-packing register allocation. SimpleStackOpndCoalescer
uses the same structures (Interval, Span) as the Jitrino bin-packing 
reg allocator. 

It is important that for non-static synchronized methods the method
argument containing "this" is not changed as it is used in 
getAddressOfThis. This is taken into account by making live-ranges
of "this" for such methods "eternal" (living till the end of the method)
*/

class SimpleStackOpndCoalescer
{
public:
    void run();

    SimpleStackOpndCoalescer(IRManager& irm)
        :irManager(irm), memoryManager("SimpleStackOpndCoalescer"), 
        candidateInsts(memoryManager), intervals(memoryManager), opndReplacements(memoryManager), 
        replacementsAdded(0), emptyBlocks(false)
    {}

protected:
    typedef StlVector<Interval*> Intervals;

    struct CandidateInst
    {   
        Inst * inst;
        uint32 execCount;
        CandidateInst(Inst * i = NULL, uint32 ec = 0)
            :inst(i), execCount(ec){}

        static bool less (const CandidateInst& x, const CandidateInst& y)   
        { 
            return x.execCount > y.execCount; // order by execCount descending
        }
    };

    typedef StlVector<CandidateInst> CandidateInsts;

    uint32 initIntervals();
    bool isCandidate(const Inst * inst) const;
    void collectCandidates();
    void collectIntervals();
    void addReplacement(Opnd * dst, Opnd * src);
    void removeInsts();
    void replaceOpnds();
    void printCandidates(::std::ostream& os, uint32 detailLevel = 0)const;

    IRManager &     irManager;
    MemoryManager   memoryManager;
    CandidateInsts  candidateInsts;
    Intervals       intervals;
    OpndVector      opndReplacements;
    uint32          replacementsAdded;
    bool            emptyBlocks;
};

//_________________________________________________________________________________________________
void SimpleStackOpndCoalescer::run()
{
    // initialize intervals vector and check if we have at least 2 stack operands
    if (initIntervals() > 1){
        // scan through all instructions and find suitable CopyPseudoInsts
        collectCandidates();
        // Are there CopyPseudoInsts to coalesce?
        if (candidateInsts.size() > 0){ 
            // collect live-ranges of stack operands
            collectIntervals();
            // collect remove redundant CopyPseudoInsts and collect vector of operand replacements
            removeInsts();
            // Is anything removed?
            if (replacementsAdded > 0){
                replaceOpnds();
                irManager.invalidateLivenessInfo();
            }
        }
    }
}

//_________________________________________________________________________________________________
uint32 SimpleStackOpndCoalescer::initIntervals()
{
    uint32 candidateOpndCount = 0;
    uint32 opndCount = irManager.getOpndCount();
    intervals.resize(opndCount);
    for (uint32 i = 0; i < opndCount; i++){
        Opnd * opnd = irManager.getOpnd(i);
        if (opnd->isPlacedIn(OpndKind_Mem) && opnd->getMemOpndKind() == MemOpndKind_StackAutoLayout){
            intervals[i] = new (memoryManager) Interval(memoryManager);
            candidateOpndCount++;
        }else
            intervals[i] = NULL;
    }
    return candidateOpndCount;
}

//_________________________________________________________________________________________________
void SimpleStackOpndCoalescer::collectCandidates()
{
    candidateInsts.resize(0);

    const Nodes& nodes = irManager.getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode()){
            for (Inst*  inst  = (Inst*)node->getFirstInst(); inst!=NULL; inst=inst->getNextInst()){
                if (isCandidate(inst)){
                    uint32 execCount = (uint32)node->getExecCount();
                    if (execCount < 1) 
                        execCount = 1;
                    candidateInsts.push_back(CandidateInst(inst, execCount));
                }
            }
        }
    }
    if (candidateInsts.size() > 1)
        ::std::sort(candidateInsts.begin(), candidateInsts.end(), CandidateInst::less);
}

//_________________________________________________________________________________________________
void SimpleStackOpndCoalescer::printCandidates(::std::ostream& os, uint32 detailLevel)const
{
    os << irManager.getMethodDesc().getParentType()->getName() << "." << irManager.getMethodDesc().getName()
        << ": " << candidateInsts.size() << ::std::endl;
    if (detailLevel > 0){
        for (uint32 i = 0; i < candidateInsts.size(); i++){
            if (detailLevel > 1)
                IRPrinter::printInst(os, candidateInsts[i].inst);
            Inst * inst = candidateInsts[i].inst;
            Opnd * dstOpnd = inst->getOpnd(0), * srcOpnd = inst->getOpnd(1);
            int adj;
            bool removable = !intervals[dstOpnd->getId()]->conflict(intervals[srcOpnd->getId()], adj);
            os << inst->getId() << " - " << (removable?"removable":"not removable") << " - " << candidateInsts[i].execCount << ::std::endl;
            if (detailLevel > 1){
                os << *intervals[dstOpnd->getId()] << ::std::endl; 
                os << *intervals[srcOpnd->getId()] << ::std::endl; 
            }
        }
        os << ::std::endl;
    }
}

static bool isTypeConversionAllowed(Opnd* fromOpnd, Opnd* toOpnd) {
    Type * fromType = fromOpnd->getType();
    Type * toType = toOpnd->getType();
    bool fromIsGCType = fromType->isObject() || fromType->isManagedPtr();
    bool toIsGCType = toType->isObject() || toType->isManagedPtr();
    return fromIsGCType == toIsGCType;
}

//_________________________________________________________________________________________________
bool SimpleStackOpndCoalescer::isCandidate(const Inst * inst)const
{
    if (inst->hasKind(Inst::Kind_CopyPseudoInst) && inst->getMnemonic() == Mnemonic_MOV){
        Opnd * dstOpnd = inst->getOpnd(0), * srcOpnd = inst->getOpnd(1);
        if (dstOpnd != srcOpnd && 
            intervals[srcOpnd->getId()] != NULL && intervals[dstOpnd->getId()] != NULL
            && dstOpnd->getSize() == srcOpnd->getSize() && isTypeConversionAllowed(srcOpnd, dstOpnd))
            return true;
    }
    return false;
}

//_________________________________________________________________________________________________
void SimpleStackOpndCoalescer::collectIntervals()
{
    irManager.indexInsts();
    uint32 opndCount = irManager.getOpndCount();

    Interval * interval;

    const Nodes& nodes = irManager.getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode()){

            Inst*  inst  = (Inst*)node->getLastInst();
            if (inst == 0)
                continue;

            uint32 instIndex=inst->getIndex();

            BitSet lives(memoryManager, opndCount);

            irManager.getLiveAtExit(node, lives);
            BitSet::IterB ib(lives);
            for (int x = ib.getNext(); x != -1; x = ib.getNext()){
                if ( (interval = intervals[x]) != NULL )
                    interval->startOrExtend(instIndex + 1);
            }

            for (; inst!=NULL; inst=inst->getPrevInst()){
                instIndex = inst->getIndex();
                Inst::Opnds defs(inst, Inst::OpndRole_All);
                for (Inst::Opnds::iterator it = defs.begin(); it != defs.end(); it = defs.next(it)){
                    Opnd * opnd = inst->getOpnd(it);
                    uint32 opndId = opnd->getId();
                    if ( (interval = intervals[opndId]) != NULL ){
                        if (inst->isLiveRangeEnd(it))
                            intervals[opndId]->stop(instIndex + 1);
                        else
                            intervals[opndId]->startOrExtend(instIndex);
                    }
                }
            }

            BitSet* tmp = irManager.getLiveAtEntry(node);

            ib.init(*tmp);
            for (int x = ib.getNext(); x != -1; x = ib.getNext()){
                if ( (interval = intervals[x]) != NULL )
                    interval->stop(instIndex);
            }
        }
    }
    
    for (uint32 i = 0; i < opndCount; i++){
        if ( (interval = intervals[i]) != NULL )
            interval->finish();
    }
}
//_________________________________________________________________________________________________
void SimpleStackOpndCoalescer::replaceOpnds()
{
    const Nodes& nodes = irManager.getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode()){
            for (Inst * inst=(Inst*)node->getLastInst(); inst!=NULL; inst=inst->getPrevInst())
                inst->replaceOpnds(&opndReplacements.front());
        }
    }
}

//_________________________________________________________________________________________________
void SimpleStackOpndCoalescer::addReplacement(Opnd * dstOpnd, Opnd * srcOpnd)
{
    if (dstOpnd != srcOpnd){
        intervals[srcOpnd->getId()]->unionWith(intervals[dstOpnd->getId()]);
        for (uint32 i = 0, n = irManager.getOpndCount(); i < n; i++){
            if (opndReplacements[i] == dstOpnd){
                if (srcOpnd->getId() != i)
                    opndReplacements[i] = srcOpnd;
                else
                    opndReplacements[i] = NULL;
            }
        }
        opndReplacements[dstOpnd->getId()] = srcOpnd;
        replacementsAdded++;
    }
}

//_________________________________________________________________________________________________
void SimpleStackOpndCoalescer::removeInsts()
{
    uint32 opndCount = irManager.getOpndCount();
    replacementsAdded = 0;
    opndReplacements.resize(opndCount);
    for (uint32 i = 0; i < opndCount; i++)
        opndReplacements[i] = NULL;
    for (uint32 i = 0; i < candidateInsts.size(); i++){
        int adj;
        Inst * inst = candidateInsts[i].inst;
        Opnd * dstOpnd = inst->getOpnd(0), * srcOpnd = inst->getOpnd(1);
        if (opndReplacements[dstOpnd->getId()] != NULL){
            dstOpnd = opndReplacements[dstOpnd->getId()];
            assert(opndReplacements[dstOpnd->getId()] == NULL);
        }
        if (opndReplacements[srcOpnd->getId()] != NULL){
            srcOpnd = opndReplacements[srcOpnd->getId()];
            assert(opndReplacements[srcOpnd->getId()] == NULL);
        }
        if (!intervals[dstOpnd->getId()]->conflict(intervals[srcOpnd->getId()], adj))
            addReplacement(dstOpnd, srcOpnd);
    }       
}

    
//========================================================================================
/**
    class CopyExpansion translated CopyPseudoInsts to corresponding copying sequences
*/
class CopyExpansion : public SessionAction {
    void runImpl();
    void restoreRegUsage(Node * bb, Inst * toInst, uint32& gpRegUsageMask, uint32& appRegUsageMask);
    uint32 getNeedInfo()const{ return NeedInfo_LivenessInfo; }
    uint32 getSideEffects()const{ return SideEffect_InvalidatesLivenessInfo; }
};


static ActionFactory<CopyExpansion> _copy("copy");

//_________________________________________________________________________________________________
void CopyExpansion::runImpl()
{
    CompilationInterface& compIntfc = irManager->getCompilationInterface();
    void* bc2LIRmapHandler = NULL;

    if (compIntfc.isBCMapInfoRequired()) {
        bc2LIRmapHandler = getContainerHandler(bcOffset2LIRHandlerName, compIntfc.getMethodToCompile());
    }

    // call SimpleStackOpndCoalescer before all other things including finalizeCallSites
    // as they add new local operands and fixLivenessInfo would be necessary 
    
    bool coalesceStack = true;
    getArg("coalesceStack", coalesceStack);
    if (coalesceStack) {
        SimpleStackOpndCoalescer(*irManager).run();
        irManager->updateLivenessInfo();
    }

    irManager->finalizeCallSites(); 

    const Nodes& nodes = irManager->getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode()){
            uint32 flagsRegUsageMask = 0;
            uint32 gpRegUsageMask = 0;
            bool calculatingRegUsage = false;
            for (Inst * inst=(Inst*)node->getLastInst(), * nextInst=NULL; inst!=NULL; inst=nextInst){
                nextInst=inst->getPrevInst();
                if (inst->hasKind(Inst::Kind_CopyPseudoInst)){
                    Mnemonic mn=inst->getMnemonic();
                    Inst *copySequence = NULL;
                    if (mn==Mnemonic_MOV){
                        Opnd * toOpnd=inst->getOpnd(0);
                        Opnd * fromOpnd=inst->getOpnd(1);

                        if (toOpnd == fromOpnd){
                            continue;
                        } else if (toOpnd->isPlacedIn(OpndKind_Reg) && fromOpnd->isPlacedIn(OpndKind_Reg)){
                            if (toOpnd->getRegName()==fromOpnd->getRegName())
                                continue;
                        }else{
#ifdef _EM64T_
                            if (!calculatingRegUsage && ((toOpnd->isPlacedIn(OpndKind_Mem) && fromOpnd->isPlacedIn(OpndKind_Mem))||fromOpnd->isPlacedIn(OpndKind_Imm))){
#else
                            if (!calculatingRegUsage && ((toOpnd->isPlacedIn(OpndKind_Mem) && fromOpnd->isPlacedIn(OpndKind_Mem))||(toOpnd->isPlacedIn(OpndKind_Reg) && fromOpnd->isPlacedIn(OpndKind_Imm)))){
#endif
                                restoreRegUsage(node, inst, gpRegUsageMask, flagsRegUsageMask);
                                calculatingRegUsage=true;
                            }
                        }
                        copySequence = irManager->newCopySequence(Mnemonic_MOV, toOpnd, fromOpnd, gpRegUsageMask, flagsRegUsageMask);
                    }else if (mn==Mnemonic_PUSH||mn==Mnemonic_POP){
#ifdef _EM64T_
                        if (!calculatingRegUsage && (inst->getOpnd(0)->isPlacedIn(OpndKind_Mem)||inst->getOpnd(0)->isPlacedIn(OpndKind_Imm))){
#else
                        if (!calculatingRegUsage && inst->getOpnd(0)->isPlacedIn(OpndKind_Mem)){
#endif
                            restoreRegUsage(node, inst, gpRegUsageMask, flagsRegUsageMask);
                            calculatingRegUsage=true;
                        }
                        copySequence = irManager->newCopySequence(mn, inst->getOpnd(0), NULL, gpRegUsageMask, flagsRegUsageMask);
                    }
                    // CopyPseudoInst map entries should be changed by new copy sequence instructions in byte code map
                    if (compIntfc.isBCMapInfoRequired() && copySequence != NULL) {
                        uint32 instID = inst->getId();
                        uint16 bcOffs = getBCMappingEntry(bc2LIRmapHandler, instID);
                        if (bcOffs != ILLEGAL_BC_MAPPING_VALUE) {
                            Inst * cpInst=NULL, * nextCpInst=copySequence, * lastCpInst=copySequence->getPrev(); 
                            do { 
                                cpInst=nextCpInst;
                                nextCpInst=cpInst->getNext();
                                uint32 cpInstID = cpInst->getId();
                                setBCMappingEntry(bc2LIRmapHandler, cpInstID, bcOffs);
                            } while ((cpInst != lastCpInst) && (cpInst != NULL));
                        }
                    }
                    // End of code map change

                    copySequence->insertAfter(inst);
                    inst->unlink();
                };
                if (calculatingRegUsage) {
                    irManager->updateRegUsage(inst, OpndKind_GPReg, gpRegUsageMask);
                    irManager->updateRegUsage(inst, OpndKind_StatusReg, flagsRegUsageMask);
                }
            }
        }
    }
}

//_________________________________________________________________________________________________
void CopyExpansion::restoreRegUsage(Node* bb, Inst * toInst, uint32& gpRegUsageMask, uint32& appRegUsageMask)
{
    assert(bb->isBlockNode());
    if (bb->isEmpty()) {
        return;
    }
    irManager->getRegUsageAtExit(bb, OpndKind_GPReg, gpRegUsageMask);
    irManager->getRegUsageAtExit(bb, OpndKind_StatusReg, appRegUsageMask);
    for (Inst* inst = (Inst*)bb->getLastInst(); inst != toInst; inst = inst->getPrevInst()){
        irManager->updateRegUsage(inst, OpndKind_GPReg, gpRegUsageMask);
        irManager->updateRegUsage(inst, OpndKind_StatusReg, appRegUsageMask);
    }
}

}}; //namespace Ia32

