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
 * @author Vyacheslav P. Shakin
 * @version $Revision: 1.12.14.1.4.3 $
 */

#include "Ia32CodeEmitter.h"
#include "Log.h"
#include "Ia32RuntimeInterface.h"
#include "Ia32Printer.h"
#include "Ia32RegAllocCheck.h"
#include "Ia32GCMap.h"
#include "Ia32BCMap.h"

namespace Jitrino
{
namespace Ia32 
{


//========================================================================================
// class CodeEmitter::ConstantAreaLayout
//========================================================================================
//________________________________________________________________________________________
void CodeEmitter::ConstantAreaLayout::collectItems()
{
    for (uint32 i=0, n=irManager.getOpndCount(); i<n; ++i){
        Opnd * opnd=irManager.getOpnd(i);
        Opnd::RuntimeInfo * ri=NULL;
        if (opnd->isPlacedIn(OpndKind_Mem)&&opnd->getMemOpndKind()==MemOpndKind_ConstantArea){
            Opnd * addrOpnd=opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
            ri=addrOpnd->getRuntimeInfo();
            assert(ri->getKind()==Opnd::RuntimeInfo::Kind_ConstantAreaItem);
        }else if (opnd->isPlacedIn(OpndKind_Imm)){
            ri=opnd->getRuntimeInfo();
            if (ri && ri->getKind()!=Opnd::RuntimeInfo::Kind_ConstantAreaItem)
                ri=NULL;
        }
        if (ri!=NULL){
            ConstantAreaItem * item=(ConstantAreaItem *)ri->getValue(0);
            if (item->getAddress()!=(void*)1){
                items.push_back(item);
                item->setAddress((void*)1);
            }
        }
    }
}

//________________________________________________________________________________________
void CodeEmitter::ConstantAreaLayout::calculateItemOffsets()
{
    uint32 offset=0;
    for (uint32 i=0, n=items.size(); i<n; ++i){
        ConstantAreaItem * item=items[i];
        uint32 size=item->getSize();
        uint32 alignment=size; 
        if (size<=4)
            alignment=4;
        else if (size<=8)
            alignment=8;
        else
            alignment=16;
        offset=(offset+alignment-1)&~(alignment-1);
        item->setAddress((void*)offset);
        offset+=size;
    }
    dataSize=offset;
}

//________________________________________________________________________________________
void CodeEmitter::ConstantAreaLayout::doLayout()
{
    collectItems();
    calculateItemOffsets();

    uint32 dataBlock = (uint32)irManager.getCompilationInterface()
        .allocateDataBlock(dataSize + blockAlignment*2, blockAlignment);
    dataBlock=(dataBlock+blockAlignment-1)&~(blockAlignment-1);
    assert(dataBlock % blockAlignment == 0);

    for (uint32 i=0, n=items.size(); i<n; ++i){
        ConstantAreaItem * item=items[i];
        uint32 offset=(uint32)item->getAddress();
        item->setAddress((void*)(dataBlock+offset));
        if (!item->hasKind(ConstantAreaItem::Kind_SwitchTableConstantAreaItem))
            memcpy(item->getAddress(), item->getValue(), item->getSize());
    }   
}

//________________________________________________________________________________________
void CodeEmitter::ConstantAreaLayout::finalizeSwitchTables()
{
    for (uint32 i=0, n=items.size(); i<n; ++i){
        ConstantAreaItem * item=items[i];
        if (item->hasKind(ConstantAreaItem::Kind_SwitchTableConstantAreaItem)){
            void ** table = (void **)item->getAddress();
            BasicBlock ** bbs=(BasicBlock **)item->getValue();
            uint32 nbb=(uint32)item->getSize()/sizeof(BasicBlock*);
            for (uint32 ibb=0; ibb<nbb; ibb++){
                BasicBlock * bb=bbs[ibb];
                assert(bb!=NULL);
                assert(std::find(irManager.getNodes().begin(), irManager.getNodes().end(), bb)!=irManager.getNodes().end());
                assert(bb->getCodeStartAddr()!=NULL);
                table[ibb]=bb->getCodeStartAddr();
            }
        }
    }   
}


//========================================================================================
// class CodeEmitter
//========================================================================================

//________________________________________________________________________________________
void CodeEmitter::runImpl()
{
    irManager.setInfo("inlineInfo", new(irManager.getMemoryManager()) InlineInfoMap(irManager.getMemoryManager()));
    constantAreaLayout.doLayout();
    irManager.resolveRuntimeInfo();
    emitCode();
    postPass();
    constantAreaLayout.finalizeSwitchTables();
    registerExceptionHandlers();
    registerInlineInfoOffsets();

}

void CodeEmitter::registerInlineInfoOffsets() 
{
    InlineInfoMap * inlineMap = (InlineInfoMap *)irManager.getInfo("inlineInfo");
    for (CFG::NodeIterator it(irManager); it!=NULL; ++it){
        if (((Node*)it)->hasKind(Node::Kind_BasicBlock)){
            BasicBlock * bb=(BasicBlock * )(Node*)it;
            const Insts& insts=bb->getInsts();
            for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
                if(inst->getMnemonic() == Mnemonic_CALL) {
                    CallInst* callinst = (CallInst*)inst;
                    if ( callinst->getInlineInfo() ) {
                        Log::cat_opt_inline()->ir << "callinstr, at offset=" << (uint32)inst->getCodeStartAddr()+inst->getCodeSize() << ::std::endl;
                        Log::cat_opt_inline()->ir << "has inline info:" << ::std::endl;
                        callinst->getInlineInfo()->printLevels(Log::cat_opt_inline()->ir);
                        // report offset 1 bundle forward
                        inlineMap->registerOffset((uint32)inst->getCodeStartAddr() - (uint32)irManager.getCodeStartAddr()+inst->getCodeSize(), callinst->getInlineInfo());
                    }
                    Log::cat_opt_inline()->ir << ::std::endl;
                }
            }
        }
    }
}

//________________________________________________________________________________________
bool CodeEmitter::verify (bool force)
{   
    bool failed=false;
    if (force || irManager.getVerificationLevel() >= 1)
    {
        irManager.updateLivenessInfo();
        for (uint32 i=0, n=irManager.getOpndCount(); i<n; i++){
            Opnd * opnd=irManager.getOpnd(i);
            if (!opnd->hasAssignedPhysicalLocation()){
                VERIFY_OUT("Unassigned operand: " << opnd << ::std::endl);
                failed=true;
            }
        }
    }
    return !failed;
}   

//________________________________________________________________________________________
void CodeEmitter::emitCode( void ) {

    // irManager.getMaxInstId() - is max possible number of instructions
    // irManager.getMaxNodeId() - to handle number of basic blocks, when jmp targets alignment used
    // as the current scheme only process basic blocks as jmp targets
    const unsigned maxMethodSize = (irManager.getMaxInstId() + irManager.getMaxNodeId())*MAX_NATIVE_INST_SIZE;

    //
    uint8 * codeStreamStart = (uint8*)malloc( maxMethodSize );
    //+---- free() below
    //|
    //v
    //
#ifdef _DEBUG
    memset(codeStreamStart, 0xCC, maxMethodSize);
#endif


    uint8 * ip = codeStreamStart;
    for( BasicBlock * bb = irManager.getPrologNode(); bb != NULL; bb=bb->getLayoutSucc()) {

        uint8 * blockStartIp = ip;
        bb->setCodeOffset( blockStartIp-codeStreamStart );
        const Insts& insts=bb->getInsts();
        for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
            if( inst->hasKind(Inst::Kind_PseudoInst)) continue;
            uint8 * instStartIp = ip;
            inst->setCodeOffset( instStartIp-blockStartIp );
            ip = inst->emit(ip);
            inst->setCodeSize( ip-instStartIp );
        }
        bb->setCodeSize( ip-blockStartIp );
    }
    unsigned codeSize = (unsigned)(ip-codeStreamStart);
    assert( codeSize < maxMethodSize );

    uint8 * codeBlock = (uint8*)irManager.getCompilationInterface().allocateCodeBlock(
            codeSize , JMP_TARGET_ALIGMENT, getCodeSectionHeat(0), 0, false );
    memcpy(codeBlock, codeStreamStart, codeSize); 
    irManager.setCodeStartAddr(codeBlock);

    //^
    //|
    //+---- malloc() above
    free( codeStreamStart );
    //
}

//________________________________________________________________________________________
void CodeEmitter::postPass()
{  
    MemoryManager& irmm = irManager.getMemoryManager();
    bool isBcRequired = irManager.getCompilationInterface().isBCMapInfoRequired();
    BcMap* bcMap = new(irmm) BcMap(irmm);
    irManager.setInfo("bcMap", bcMap);
    for( BasicBlock * bb = irManager.getPrologNode(); bb != NULL; bb=bb->getLayoutSucc()) {
        uint64 bcOffset;
        const Insts& insts=bb->getInsts();
        for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)) {
                if (inst->hasKind(Inst::Kind_ControlTransferInst) && 
                    ((ControlTransferInst*)inst)->isDirect()
                ){
                    uint8 * instCodeStartAddr=(uint8*)inst->getCodeStartAddr();
                    uint8 * instCodeEndAddr=(uint8*)instCodeStartAddr+inst->getCodeSize();
                    uint8 * targetCodeStartAddr=0;
                    uint32 targetOpndIndex = ((ControlTransferInst*)inst)->getTargetOpndIndex();
                    if (inst->hasKind(Inst::Kind_BranchInst)){
                        BasicBlock * bbTarget=((BranchInst*)inst)->getDirectBranchTarget();
                        targetCodeStartAddr=(uint8*)bbTarget->getCodeStartAddr();
                    }else if (inst->hasKind(Inst::Kind_CallInst)){
                        targetCodeStartAddr=(uint8*)(POINTER_SIZE_INT)inst->getOpnd(targetOpndIndex)->getImmValue();
                    }else 
                        continue;
                    uint32 offset=targetCodeStartAddr-instCodeEndAddr;
                    inst->getOpnd(targetOpndIndex)->assignImmValue((int)offset);
                    // re-emit the instruction: 
                    inst->emit(instCodeStartAddr);

                    if (inst->hasKind(Inst::Kind_CallInst)){
                        registerDirectCall(inst);
                    }
                }
                    if (isBcRequired) {
                            uint64 instID = inst->getId();
                            bcOffset = bc2LIRMapHandler->getVectorEntry(instID);

                            if (bcOffset != ILLEGAL_VALUE) {
                                POINTER_SIZE_INT instStartAddr = (POINTER_SIZE_INT) inst->getCodeStartAddr();
                                bcMap->setEntry((uint64) instStartAddr, bcOffset);
                            }
                    }
        }
        }  
}

//________________________________________________________________________________________
void CodeEmitter::registerDirectCall(Inst * inst)
{
    assert(inst->hasKind(Inst::Kind_CallInst) && ((CallInst*)inst)->isDirect());
    CallInst * callInst=(CallInst*)inst;
    Opnd * targetOpnd=callInst->getOpnd(callInst->getTargetOpndIndex());
    assert(targetOpnd->isPlacedIn(OpndKind_Imm));
    Opnd::RuntimeInfo * ri=targetOpnd->getRuntimeInfo();

    if( !ri || ri->getKind() != Opnd::RuntimeInfo::Kind_MethodDirectAddr) { return; };

    MethodDesc * md = (MethodDesc*)ri->getValue(0);
    irManager.getCompilationInterface().setNotifyWhenMethodIsRecompiled(md,inst->getCodeStartAddr());
    if (Log::cat_cg()->isDebugEnabled()) {
        Log::out() << "Registered call to " << md->getParentType()->getName() << "." << md->getName() << " at ";
        Log::out() << inst->getCodeStartAddr() << " for recompiled method event" << ::std::endl;
    }
}

//________________________________________________________________________________________
bool RuntimeInterface::recompiledMethodEvent(BinaryRewritingInterface& binaryRewritingInterface,
                           MethodDesc *              recompiledMethodDesc, 
                           void *                    data) 
{
    Byte ** indirectAddr = (Byte **)recompiledMethodDesc->getIndirectAddress();
    Byte * targetAddr = *indirectAddr;
    Byte * callAddr = (Byte*)data;
    uint32 offset = targetAddr - callAddr-5;

    if (Log::cat_rt()->isDebugEnabled()) {
        Log::cat_rt()->out() << "patching call to "<<recompiledMethodDesc->getName()<<" at "<<(void*)callAddr<<"; new target address is "<<(void*)targetAddr<< ::std::endl;
    }

    *(uint32*)(callAddr+1)=offset;

    return true;
}

//________________________________________________________________________________________
CompilationInterface::CodeBlockHeat CodeEmitter::getCodeSectionHeat(uint32 sectionID)const
{
    CompilationInterface::CodeBlockHeat heat;
    CompilationInterface& ci=irManager.getCompilationInterface();
    uint32 level = ci.getOptimizationLevel();
    if (level == 0)
        heat = CompilationInterface::CodeBlockHeatMin;
    else if (level == 1 && ci.isDynamicProfiling())
        heat = CompilationInterface::CodeBlockHeatDefault;
    else if (sectionID==0)
        heat = CompilationInterface::CodeBlockHeatMax;
    else
        heat = CompilationInterface::CodeBlockHeatMin;
    return heat;
}

//________________________________________________________________________________________
void CodeEmitter::registerExceptionHandlers()
{
    uint32 regionStart=0, regionEnd=0; DispatchNode * regionDispatchNode=NULL;
    for( BasicBlock * bb = irManager.getPrologNode(); bb != NULL; bb=bb->getLayoutSucc()) {
            DispatchNode * dispatchNode=(DispatchNode *)bb->getNode(Direction_Out, Node::Kind_DispatchNode);
            if (regionDispatchNode!=dispatchNode){
                if (regionDispatchNode!=NULL && regionStart<regionEnd)
                    registerExceptionRegion((void*)regionStart, (void*)regionEnd, regionDispatchNode);
                regionDispatchNode=dispatchNode;
                regionStart=regionEnd=(uint32)bb->getCodeStartAddr();
            }
            if (dispatchNode!=NULL){
                regionEnd+=bb->getCodeSize();
        }
    }
    if (regionDispatchNode!=NULL && regionStart<regionEnd)
        registerExceptionRegion((void*)regionStart, (void*)regionEnd, regionDispatchNode);


    uint32 handlerInfoCount=exceptionHandlerInfos.size();
    irManager.getCompilationInterface().setNumExceptionHandler(handlerInfoCount);
    for (uint32 i=0; i<handlerInfoCount; i++){
        const ExceptionHandlerInfo & info=exceptionHandlerInfos[i];
        if (Log::cat_cg()->isDebugEnabled()) {
            Log::out() << "Exception Handler Info [ " << i << "]: " << ::std::endl;
            Log::out() << "    " << (void*)(info.regionStart) << " - "<<(void*)(info.regionEnd) << 
                " => " << (void*)info.handlerAddr << ::std::endl << "    ";
            IRPrinter::printType(Log::out(), info.exceptionType);
            Log::out() << ::std::endl;
        }

        irManager.getCompilationInterface().
            setExceptionHandlerInfo(i,
                (Byte*)info.regionStart, (Byte*)info.regionEnd,
                (Byte*)info.handlerAddr,
                info.exceptionType,
                info.exceptionObjectIsDead);
    }

}

//________________________________________________________________________________________
void CodeEmitter::registerExceptionRegion(void * regionStart, void * regionEnd, DispatchNode * regionDispatchNode)
{
    assert(regionStart!=NULL && regionStart<regionEnd && regionDispatchNode!=NULL);
    regionDispatchNode->sortCatchEdges();
    const Edges& edges=regionDispatchNode->getEdges(Direction_Out);
    for (Edge * edge=edges.getFirst(); edge!=NULL; edge=edges.getNext(edge)){
        Node * head=edge->getNode(Direction_Head);
        if (edge->hasKind(Edge::Kind_CatchEdge)){
            BasicBlock * handler=(BasicBlock*)head;

            const Insts& insts = handler->getInsts();
            Inst * catchInst =insts.getFirst();
            for(; catchInst != NULL && catchInst->getKind() != Inst::Kind_CatchPseudoInst;catchInst = insts.getNext(catchInst));
            assert(catchInst);
            MemoryManager mm(0x400, "CatchOpnds");
            LiveSet * ls=new(mm) LiveSet(mm, irManager.getOpndCount());
            irManager.getLiveAtExit(handler, *ls);
            for (Inst * inst=insts.getLast(); inst!=catchInst; inst=insts.getPrev(inst)){
                irManager.updateLiveness(inst, *ls);
            }
            bool isExceptObjDead = !ls->isLive(catchInst->getOpnd(0));
            
            void * handlerAddr=handler->getCodeStartAddr();
            ObjectType * exceptionType = (ObjectType*)((CatchEdge*)edge)->getType();
            exceptionHandlerInfos.push_back(
                    ExceptionHandlerInfo(regionStart, regionEnd, handlerAddr, exceptionType, isExceptObjDead)
                );
        }else{
            if (head->hasKind(Node::Kind_DispatchNode))
                registerExceptionRegion(regionStart, regionEnd, (DispatchNode*)head);
        }
    }
}


}}; // ~Jitrino::Ia32
