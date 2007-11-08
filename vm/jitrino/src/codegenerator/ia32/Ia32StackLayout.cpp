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
 * @author Intel, Nikolay A. Sidelnikov
 * @version $Revision: 1.10.14.1.4.4 $
 */

#include "Ia32IRManager.h"
#include "Ia32StackInfo.h"

namespace Jitrino
{
namespace Ia32 {

//========================================================================================
// class StackLayouter
//========================================================================================
/**
 *  Class StackLayouter performs forming of stack memory for a method, e.g.
 *  allocates memory for stack variables and input arguments, inserts 
 *  saving/restoring callee-save registers. Also it fills StackInfo object
 *  for runtime access to information about stack layout
 *
 *  This transformer ensures that
 *
 *  1)  All input argument operands and stack memory operands have appropriate
 *      displacements from stack pointer
 *  
 *  2)  There are save/restore instructions for all callee-save registers
 *
 *  3)  There are save/restore instructions for all caller-save registers for
 *      all calls in method
 *
 *  4)  ESP has appropriate value throughout whole method
 *
 *  Stack layout illustration:
 *
 *  +-------------------------------+   inargEnd
 *  |                               |
 *  |                               |
 *  |                               |
 *  +-------------------------------+   inargBase, eipEnd
 *  |           eip                 |
 *  +-------------------------------+   eipBase,icalleeEnd      <--- "virtual" ESP
 *  |           EBX                 |
 *  |           EBP                 |
 *  |           ESI                 |
 *  |           EDI                 |
 *  +-------------------------------+   icalleeBase, fcalleeEnd
 *  |                               |
 *  |                               |
 *  |                               |
 *  +-------------------------------+   fcalleeBase, acalleeEnd
 *  |                               |
 *  |                               |
 *  |                               |
 *  +-------------------------------+   acalleeBase, localEnd
 *  |                               |
 *  |                               |
 *  |                               |
 *  +-------------------------------+   localBase    
 *  |      alignment padding        |
 *  |-------------------------------+   <--- "real" ESP
 *  |           EAX                 |
 *  |           ECX                 |
 *  |           EDX                 |
 *  +-------------------------------+   base of caller-save regs
 *  
*/

class StackLayouter : public SessionAction {
public:
    StackLayouter ();
    StackInfo * getStackInfo() { return stackInfo; }
    void runImpl();

    uint32 getNeedInfo()const{ return 0; }
    uint32 getSideEffects()const{ return 0; }

protected:
    void checkUnassignedOpnds();

    /** Computes all offsets for stack areas and stack operands,
        inserts pushs for callee-save registers
    */
    void createProlog();

    /** Restores stack pointer if needed,
        inserts pops for callee-save registers
    */
    void createEpilog();

    int32 getLocalBase(){ return  localBase; }
    int32 getLocalEnd(){ return  localEnd; }
    int32 getApplCalleeBase(){ return  acalleeBase; }
    int32 getApplCalleeEnd(){ return  acalleeEnd; }
    int32 getFloatCalleeBase(){ return  fcalleeBase; }
    int32 getFloatCalleeEnd(){ return  fcalleeEnd; }
    int32 getIntCalleeBase(){ return  icalleeBase; }
    int32 getIntCalleeEnd(){ return  icalleeEnd; }
    int32 getRetEIPBase(){ return  retEIPBase; } 
    int32 getRetEIPEnd(){ return  retEIPEnd; }
    int32 getInArgBase(){ return  inargBase; } 
    int32 getInArgEnd(){ return  inargEnd; }
    int32 getFrameSize(){ return  frameSize; }
    uint32 getOutArgSize(){ return  outArgSize; }

    int32 localBase;
    int32 localEnd;
    int32 fcalleeBase;
    int32 fcalleeEnd;
    int32 acalleeBase;
    int32 acalleeEnd;
    int32 icalleeBase;
    int32 icalleeEnd;

    int32 retEIPBase;
    int32 retEIPEnd;
    int32 inargBase;
    int32 inargEnd;
    int32 frameSize;
    uint32 outArgSize;
    StackInfo * stackInfo;

    MemoryManager memoryManager;

};

static ActionFactory<StackLayouter> _stack("stack");


StackLayouter::StackLayouter ()
   :localBase(0),
    localEnd(0),
    fcalleeBase(0),
    fcalleeEnd(0),
    acalleeBase(0),
    acalleeEnd(0),
    icalleeBase(0),
    icalleeEnd(0),
    retEIPBase(0),
    retEIPEnd(0),
    inargBase(0),
    inargEnd(0),
    frameSize(0),
    outArgSize(0),
    memoryManager("StackLayouter")
{
};


static bool isSOEHandler(ObjectType* type) {
    static const char* soeHandlers[] = {"java/lang/Object", "java/lang/Throwable", "java/lang/Error", "java/lang/StackOverflowError", NULL};
    const char* typeName = type->getName();
    for (size_t i=0;soeHandlers[i]!=NULL; i++) {
        if (!strcmp(typeName, soeHandlers[i])) {
            return true;
        }
    }
    return false;
}

//checks if SOE can be caught in this method
static bool hasSOEHandlers(IRManager& irm) {
    //A contract with VM: check extra page for synchronized methods or methods with SOE handlers.
    if (irm.getMethodDesc().isSynchronized()) {
        return true;
    }

    const Nodes& nodes= irm.getFlowGraph()->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
       Node* node = *it;
        if (node->isCatchBlock()) {
            const Edges& edges = node->getInEdges();
            for (Edges::const_iterator ite = edges.begin(), ende = edges.end(); ite!=ende; ++ite) {
                Edge* e = *ite;
                CatchEdge* catchEdge = (CatchEdge*)e;
                ObjectType* catchType = catchEdge->getType()->asObjectType();
                assert(catchType!=NULL);
                if (isSOEHandler(catchType)) {
                    return true;
                }
            }
        }
    }
    return false;
}

#define MAX_STACK_FOR_SOE_HANDLERS 0x2000

static void insertSOECheck(IRManager& irm, uint32 maxStackUsedByMethod) {
#ifdef _EM64T_
    //SOE checking is not finished on EM64T
    //TODO: work on stack alignment & SOE checkers
    if (true) return; 
#endif
    uint32 stackToCheck = maxStackUsedByMethod + (hasSOEHandlers(irm) ? MAX_STACK_FOR_SOE_HANDLERS : 0);
    if (stackToCheck == 0) {
        return;
    }
    static const uint32 PAGE_SIZE=0x1000;
    
    uint32 nPagesToCheck = stackToCheck / PAGE_SIZE;
    Inst* prevInst = irm.getEntryPointInst();
    for(uint32 i=0;i<=nPagesToCheck; i++) {
        uint32 offset = i < nPagesToCheck ? PAGE_SIZE * (i+1) : stackToCheck;
        Opnd* guardedMemOpnd = irm.newMemOpnd(irm.getTypeFromTag(Type::IntPtr), MemOpndKind_Heap, irm.getRegOpnd(STACK_REG), -(int)offset);
        Inst* guardInst = irm.newInst(Mnemonic_TEST, guardedMemOpnd, irm.getRegOpnd(STACK_REG));
        guardInst->insertAfter(prevInst);
        guardInst->setBCOffset(0);
        prevInst = guardInst;
    }
}

void StackLayouter::runImpl()
{
    IRManager & irm=getIRManager();

    stackInfo = new(irm.getMemoryManager()) StackInfo(irm.getMemoryManager());
    irm.setInfo(STACK_INFO_KEY, stackInfo);

    irm.calculateOpndStatistics();
#ifdef _DEBUG
    checkUnassignedOpnds();
#endif
    irm.calculateTotalRegUsage(OpndKind_GPReg);
    createProlog();
    createEpilog();
    uint32 maxStackDepth = irm.calculateStackDepth();
    insertSOECheck(irm, maxStackDepth);
    irm.layoutAliasOpnds();

    //fill StackInfo object
    stackInfo->frameSize = getFrameSize();

    stackInfo->icalleeMask = irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_GPReg).getMask() & irm.getTotalRegUsage(OpndKind_GPReg);
    stackInfo->icalleeOffset = getIntCalleeBase();
    stackInfo->fcallee = irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_FPReg).getMask();
    stackInfo->foffset = getFloatCalleeBase();
    stackInfo->acallee = 0; //VSH: TODO - get rid off appl regs irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_ApplicationReg);
    stackInfo->aoffset = getApplCalleeBase();
    stackInfo->localOffset = getLocalBase();
    stackInfo->eipOffset = getRetEIPBase();
}

void StackLayouter::checkUnassignedOpnds()
{
    for (uint32 i=0, n=irManager->getOpndCount(); i<n; i++) {
#ifdef _DEBUG
        Opnd * opnd = irManager->getOpnd(i);
        assert(!opnd->getRefCount() || opnd->hasAssignedPhysicalLocation());
#endif
    }
}

void StackLayouter::createProlog()
{
    const uint32 slotSize = sizeof(POINTER_SIZE_INT); 
    const uint32 stackSizeAlignment = (STACK_ALIGNMENT == STACK_ALIGN_HALF16) ? STACK_ALIGN16 : STACK_ALIGNMENT; 
    IRManager & irm = getIRManager();
    EntryPointPseudoInst * entryPointInst = NULL;
    int offset = 0;
    
    entryPointInst = irManager->getEntryPointInst();
    assert(entryPointInst->getNode() == irManager->getFlowGraph()->getEntryNode());

    // Create or reset displacements for stack memory operands.
    for (uint32 i = 0; i < irm.getOpndCount(); i++) {
        Opnd * opnd = irm.getOpnd(i);
        if (opnd->getRefCount() && opnd->getMemOpndKind() == MemOpndKind_StackAutoLayout) {
            Opnd * dispOpnd=opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
            if (dispOpnd == NULL){
                dispOpnd = irm.newImmOpnd(irm.getTypeManager().getInt32Type(), 0);
                opnd->setMemOpndSubOpnd(MemOpndSubOpndKind_Displacement, dispOpnd);
            }
            dispOpnd->assignImmValue(0);
        }
    }

    // Return EIP area.
    retEIPBase = offset;
    offset += sizeof(POINTER_SIZE_INT);
    retEIPEnd = inargBase = offset;


    // Assign displacements for input operands.
    if (entryPointInst) {
        const StlVector<CallingConventionClient::StackOpndInfo>& stackOpndInfos = 
            ((const EntryPointPseudoInst*)entryPointInst)->getCallingConventionClient().getStackOpndInfos(Inst::OpndRole_Def);

        for (uint32 i = 0, n = (uint32)stackOpndInfos.size(); i < n; i++) {
            uint64 argOffset = stackOpndInfos[i].offset;
            Opnd * opnd = entryPointInst->getOpnd(stackOpndInfos[i].opndIndex);
            Opnd * disp = opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
            disp->assignImmValue(offset + argOffset);
        }
    }
    inargEnd = offset;
    icalleeEnd = offset = 0;

    uint32 calleeSavedRegs=irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_GPReg).getMask();
    uint32 usageRegMask = irManager->getTotalRegUsage(OpndKind_GPReg);
    Inst * lastPush = NULL;
    
    // Push callee-save registers onto stack.
#ifdef _EM64T_
    for (uint32 reg = RegName_R15; reg >= RegName_RAX; reg--) {
#else
    for (uint32 reg = RegName_EDI; reg >= RegName_EAX; reg--) {
#endif
        uint32 mask = getRegMask((RegName)reg);
        if ((mask & calleeSavedRegs) && (usageRegMask & mask)) {
            Inst * inst = irm.newInst(Mnemonic_PUSH, irm.getRegOpnd((RegName)reg));
            if (!lastPush) {
                lastPush = inst;
            }
            inst->insertAfter(entryPointInst);
            offset -= slotSize;
        }
    }
    icalleeBase = fcalleeEnd = fcalleeBase = acalleeEnd = acalleeBase = localEnd = offset;

    // Retrieve relations not earlier than all memory locations are assigned.
    IRManager::AliasRelation * relations = new(irm.getMemoryManager()) IRManager::AliasRelation[irm.getOpndCount()];
    irm.getAliasRelations(relations);

    // Assign displacements for local variable operands.
    for (uint32 i = 0; i < irm.getOpndCount(); i++) {
        Opnd * opnd = irm.getOpnd(i);
        if (opnd->getRefCount() == 0)
            continue;
        if(opnd->getMemOpndKind() == MemOpndKind_StackAutoLayout) {
            Opnd * dispOpnd = opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
            if (dispOpnd->getImmValue() == 0) {
                if (relations[opnd->getId()].outerOpnd == NULL) {
                    uint32 cb = getByteSize(opnd->getSize());
                    cb=(cb + slotSize - 1) & ~(slotSize - 1);
                    offset -= cb;
                    dispOpnd->assignImmValue(offset);
                }
            }
        }
    }

    // Align stack pointer. Local area should preserve alignment available on function enter.
    offset = offset & ~(stackSizeAlignment - 1);

    // Assert local area is properly aligned.
    assert((offset & (STACK_ALIGNMENT - 1)) == 0);    
    
    localBase = offset;

    if (localEnd>localBase) {
        Inst* newIns = irm.newInst(Mnemonic_SUB, irm.getRegOpnd(STACK_REG), irm.newImmOpnd(irm.getTypeManager().getInt32Type(), localEnd - localBase));
        newIns->insertAfter(lastPush ? lastPush : entryPointInst);
    }

    frameSize = icalleeEnd -localBase;
}       

void StackLayouter::createEpilog()
{ // Predeccessors of en and irm.isEpilog(en->pred)
    IRManager & irm = getIRManager();
    uint32 calleeSavedRegs = irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_GPReg).getMask();
    const Edges& inEdges = irm.getFlowGraph()->getExitNode()->getInEdges();
    uint32 usageRegMask = irManager->getTotalRegUsage(OpndKind_GPReg);
    for (Edges::const_iterator ite = inEdges.begin(), ende = inEdges.end(); ite!=ende; ++ite) {
        Edge* edge = *ite;
        if (irm.isEpilog(edge->getSourceNode())) {
            Node * epilog = edge->getSourceNode();
            Inst * retInst = (Inst*)epilog->getLastInst();
            assert(retInst->hasKind(Inst::Kind_RetInst));
            if (localEnd > localBase) {
                // Restore stack pointer.
                Inst* newIns = irm.newInst(Mnemonic_ADD, irm.getRegOpnd(STACK_REG), irm.newImmOpnd(irm.getTypeManager().getInt32Type(), localEnd - localBase));
                newIns->insertBefore(retInst);
            }
#ifdef _EM64T_
            for (uint32 reg = RegName_R15; reg >= RegName_RAX ; reg--) {//pop callee-save registers
#else
            for (uint32 reg = RegName_EDI; reg >= RegName_EAX ; reg--) {//pop callee-save registers
#endif
                uint32 mask = getRegMask((RegName)reg);
                if ((mask & calleeSavedRegs) &&  (usageRegMask & mask)) {
                    Inst* newIns = irm.newInst(Mnemonic_POP, irm.getRegOpnd((RegName)reg));
                    newIns->insertBefore(retInst);
                }
            }
        }
    }
}

}} //namespace Ia32

