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
 * @version $Revision: 1.16.12.1.4.4 $
 *
 */

#include "devirtualizer.h"
#include "Log.h"
#include "PropertyTable.h"
#include "Dominator.h"
#include "inliner.h"

namespace Jitrino {

DEFINE_OPTPASS_IMPL(GuardedDevirtualizationPass, devirt, "Guarded Devirtualization of Virtual Calls")

void
GuardedDevirtualizationPass::_run(IRManager& irm) {
    computeDominators(irm);
    DominatorTree* dominatorTree = irm.getDominatorTree();
    Devirtualizer pass(irm);
    pass.guardCallsInRegion(irm, dominatorTree);
}

DEFINE_OPTPASS_IMPL(GuardRemovalPass, unguard, "Removal of Cold Guards")

void
GuardRemovalPass::_run(IRManager& irm) {
    Devirtualizer pass(irm);
    pass.unguardCallsInRegion(irm);
}

Devirtualizer::Devirtualizer(IRManager& irm) 
: _hasProfileInfo(irm.getFlowGraph().hasEdgeProfile()), 
  _typeManager(irm.getTypeManager()), 
  _instFactory(irm.getInstFactory()), _opndManager (irm.getOpndManager()) {
    JitrinoParameterTable& propertyTable = irm.getParameterTable();
    
    bool doProfileOnly = OptPass::inDPGOMode(irm);
    _skipColdTargets = propertyTable.lookupBool("opt::devirt::skip_cold", true);
    _doAggressiveGuardedDevirtualization = !doProfileOnly || propertyTable.lookupBool("opt::devirt::aggressive", true);
    _doProfileOnlyGuardedDevirtualization = !_doAggressiveGuardedDevirtualization;
    _devirtUseCHA = propertyTable.lookupBool("opt::devirt::useCHA", false);
    _devirtSkipExceptionPath = propertyTable.lookupBool("opt::devirt::skip_exception_path", true);
}

bool
Devirtualizer::isGuardableVirtualCall(Inst* inst, MethodInst*& methodInst, Opnd*& base, Opnd*& tauNullChecked, Opnd *&tauTypesChecked, uint32 &argOffset)
{
    //
    // Returns true if this call site may be considered for guarded de-virtualization
    //
    Opcode opcode = inst->getOpcode();
    if(opcode == Op_TauVirtualCall) { 
        // Virtual function call
        assert(inst->getNumSrcOperands() >= 3);
        MethodCallInst *methodCallInst = inst->asMethodCallInst();
        assert(methodCallInst != NULL);

        methodInst = methodCallInst;
        base = methodCallInst->getSrc(2);
        tauNullChecked = methodCallInst->getSrc(0);
        tauTypesChecked = methodCallInst->getSrc(1);
        argOffset = 2;

        assert(tauNullChecked->getType()->tag == Type::Tau);
        assert(tauTypesChecked->getType()->tag == Type::Tau);
    } else if(opcode == Op_IndirectMemoryCall) {
        assert(inst->getNumSrcOperands() >= 4);
        // Indirect call - attempt to determine original virtual call
        CallInst* call = inst->asCallInst();
        assert(call != NULL);
        Opnd* funPtr = call->getFunPtr(); // = Src(0)
        methodInst = funPtr->getInst()->asMethodInst();
        if(methodInst == NULL)
            // Cannot resolve methodDesc.
            return false;

        // LdFunAddr should have already converted to a direct call (Op_DirectCall)
        // by the simplifier.
        base = call->getSrc(3);
        tauNullChecked = call->getSrc(1);
        tauTypesChecked = call->getSrc(2);
        argOffset = 3;

        assert(methodInst != NULL && methodInst->getOpcode() == Op_TauLdVirtFunAddrSlot);
        assert(tauNullChecked->getType()->tag == Type::Tau);
        assert(tauTypesChecked->getType()->tag == Type::Tau);
 
        // Assert that this base matches the one from ldFunInst.
        Opcode methodOpcode = methodInst->getOpcode();
        Opnd *methodSrc0 = methodInst->getSrc(0);
        if ((methodOpcode == Op_TauLdVirtFunAddrSlot) ||
            (methodOpcode == Op_TauLdVirtFunAddr)) {
            Inst *vtableInst = methodSrc0->getInst();
            Opcode vtableInstOpcode = vtableInst->getOpcode();
            if ((vtableInstOpcode == Op_TauLdVTableAddr) ||
                (vtableInstOpcode == Op_TauLdIntfcVTableAddr)) {
            } else {
                // must be a copy or something, too hard to check
            }
        } else {
            assert(base == methodInst->getSrc(0));
        }
    } else {
        return false;
    }

    return true;
}

void
Devirtualizer::guardCallsInRegion(IRManager& regionIRM, DominatorTree* dtree) {
    // 
    // Perform guarded de-virtualization on calls in region
    //
    
    assert(dtree->isValid());
    StlDeque<DominatorNode *> dom_stack(regionIRM.getMemoryManager());

    dom_stack.push_back(dtree->getDominatorRoot());
    CFGNode* node = NULL;
    DominatorNode *domNode = NULL;
    while (!dom_stack.empty()) {

        domNode = dom_stack.back();
        
        if (domNode) {
            
            node = domNode->getNode();
            guardCallsInBlock(regionIRM, node);
            
            // move to next sibling;
            dom_stack.back() = domNode->getSiblings();
            
            // but first deal with this node's children
            //
            // Skip exception control flow unless directed
            //
            // (because of this if we can't use walkers from walkers.h)
            if(node->isBlockNode() || !_devirtSkipExceptionPath)
                dom_stack.push_back(domNode->getChild()); 
        } else {
            dom_stack.pop_back(); // no children left, exit scope
        }
    }
}


void
Devirtualizer::genGuardedDirectCall(IRManager &regionIRM, CFGNode* node, Inst* call, MethodDesc* methodDesc, Opnd *tauNullChecked, Opnd *tauTypesChecked, uint32 argOffset) {
    FlowGraph &regionFG = regionIRM.getFlowGraph();
    assert(!methodDesc->isStatic());
    assert(call == node->getLastInst());

    Log::cat_opt_inline()->ir << "Generating guarded direct call to " << methodDesc->getParentType()->getName() 
        << "." << methodDesc->getName() << ::std::endl;
    Log::cat_opt_inline()->debug << "Guarded call bytecode size=" << (int) methodDesc->getByteCodeSize() << ::std::endl;

    //
    // Compute successors of call site
    //
    CFGNode* next = (CFGNode*) node->getUnconditionalEdge()->getTargetNode();
    CFGNode* dispatch = (CFGNode*) node->getExceptionEdge()->getTargetNode();

    //
    // Disconnect edges from call.
    //
    regionFG.removeEdge(node, next);
    regionFG.removeEdge(node, dispatch);
    call->unlink();

    //
    // Create nodes for guard, direct call, virtual call, and merge.
    //
    CFGNode* guard = node;
    CFGNode* directCallBlock = regionFG.createBlockNode();
    CFGNode* virtualCallBlock = regionFG.createBlockNode();
    CFGNode* merge = next;

    //
    // Reconnect graph with new nodes.  Connect to merge point later.
    //
    regionFG.addEdge(guard, directCallBlock)->setEdgeProb(1.0);
    regionFG.addEdge(guard, virtualCallBlock);
    regionFG.addEdge(directCallBlock, dispatch);
    regionFG.addEdge(virtualCallBlock, dispatch);
    directCallBlock->setFreq(guard->getFreq());

    //
    // Create direct call instruction
    //
    Opnd* dst = call->getDst(); 
    uint32 numArgs = call->getNumSrcOperands()-argOffset;
    uint32 i = 0;
    uint32 j = argOffset; // skip taus
    Opnd** args = new (regionIRM.getMemoryManager()) Opnd*[numArgs];
    for(; i < numArgs; ++i, ++j)
        args[i] = call->getSrc(j); // skip taus
    Inst* directCall = _instFactory.makeDirectCall(dst, 
                                                   tauNullChecked,
                                                   tauTypesChecked,
                                                   numArgs, args, methodDesc);
    //
    // copy InlineInfo
    //
    InlineInfo* call_ii = call->getCallInstInlineInfoPtr();
    InlineInfo* new_ii = directCall->getCallInstInlineInfoPtr();
    assert(call_ii && new_ii);
    new_ii->getInlineChainFrom(*call_ii);
    
    directCallBlock->append(directCall);

    //
    // Create virtual call
    //
    Inst* virtualCall = call;
    CallInst* icall = virtualCall->asCallInst();
    if(icall != NULL) {
        // Duplicate function pointer calculation in virtual call block.
        // Either the original call will be eliminated as dead code or 
        // this call will be eliminated by CSE.
        Opnd* funPtr = icall->getFunPtr();
        MethodInst* ldSlot = funPtr->getInst()->asMethodInst();
        assert(ldSlot != NULL && ldSlot->getOpcode() == Op_TauLdVirtFunAddrSlot);
        assert(ldSlot->getNumSrcOperands() == 2);
        Opnd* vtable = ldSlot->getSrc(0);
        Opnd* tauVtableHasDesc = ldSlot->getSrc(1);
        Opnd* funPtr2 = _opndManager.createSsaTmpOpnd(funPtr->getType());
        Inst* ldSlot2 = _instFactory.makeTauLdVirtFunAddrSlot(funPtr2, vtable,
                                                             tauVtableHasDesc,
                                                             ldSlot->getMethodDesc());
        virtualCallBlock->append(ldSlot2);
        icall->setSrc(0, funPtr2);
    }
    virtualCallBlock->append(virtualCall);

    //
    // Promote a return operand (if one exists) of the call from a ssa temp to a var.
    //
    if (!dst->isNull()) {
        Opnd* ssaTmp1 = _opndManager.createSsaTmpOpnd(dst->getType());  // returns value of direct call
        directCall->setDst(ssaTmp1);
        Opnd* ssaTmp2 = _opndManager.createSsaTmpOpnd(dst->getType());  // returns value of direct call
        virtualCall->setDst(ssaTmp2);
        
        VarOpnd* returnVar = _opndManager.createVarOpnd(dst->getType(), false);
        
        CFGNode* directStVarBlock = regionFG.createBlockNode();
        directStVarBlock->setFreq(directCallBlock->getFreq());
        regionFG.addEdge(directCallBlock, directStVarBlock)->setEdgeProb(1.0);
        Inst* stVar1 = _instFactory.makeStVar(returnVar, ssaTmp1);
        directStVarBlock->append(stVar1);
        
        CFGNode* virtualStVarBlock = regionFG.createBlockNode();
        regionFG.addEdge(virtualCallBlock, virtualStVarBlock);
        Inst* stVar2 = _instFactory.makeStVar(returnVar, ssaTmp2);
        virtualStVarBlock->append(stVar2);
        
        Inst* ldVar = _instFactory.makeLdVar(dst, returnVar);
        Inst* phi = NULL;

        if(regionIRM.getInSsa()) {
            Opnd* returnSsaVars[2];
            returnSsaVars[0] = _opndManager.createSsaVarOpnd(returnVar);  // returns value of direct call
            returnSsaVars[1] = _opndManager.createSsaVarOpnd(returnVar);  // returns value of virtual call
            Opnd* phiSsaVar = _opndManager.createSsaVarOpnd(returnVar);  // phi merge of the two above
            stVar1->setDst(returnSsaVars[0]);
            stVar2->setDst(returnSsaVars[1]);
            ldVar->setSrc(0, phiSsaVar);
            phi = _instFactory.makePhi(phiSsaVar, 2, returnSsaVars);
        }
        
        //
        // If successor has more than 1 in-edge, then we need a block for phi
        //
        bool needPhiBlock = (merge->getInDegree() > 0);
        if (needPhiBlock) {
            CFGNode* phiBlock = regionFG.createBlockNode();
            regionFG.addEdge(phiBlock, merge);
            regionFG.addEdge(directStVarBlock, phiBlock)->setEdgeProb(1.0);
            regionFG.addEdge(virtualStVarBlock, phiBlock);
            merge = phiBlock;
        } else {
            regionFG.addEdge(directStVarBlock, merge)->setEdgeProb(1.0);
            regionFG.addEdge(virtualStVarBlock, merge);
        }

        merge->prepend(ldVar);
        if(phi != NULL) 
            merge->prepend(phi);
    } else{
        // Connect calls directly to merge.
        regionFG.addEdge(directCallBlock, merge)->setEdgeProb(1.0);
        regionFG.addEdge(virtualCallBlock, merge);
    }

    //
    // Add the vtable compare (i.e., the type test) and branch in guard node
    //
    Opnd* base = directCall->getSrc(2); // skip over taus
    assert(base->getType()->isObject());
    ObjectType* baseType = (ObjectType*) base->getType();
    Opnd* dynamicVTableAddr = _opndManager.createSsaTmpOpnd(_typeManager.getVTablePtrType(baseType));
    Opnd* staticVTableAddr = _opndManager.createSsaTmpOpnd(_typeManager.getVTablePtrType(baseType));
    guard->append(_instFactory.makeTauLdVTableAddr(dynamicVTableAddr, base,
                                                  tauNullChecked));
    guard->append(_instFactory.makeGetVTableAddr(staticVTableAddr, baseType));
    guard->append(_instFactory.makeBranch(Cmp_EQ, Type::VTablePtr, dynamicVTableAddr, staticVTableAddr, directCallBlock->getLabel()));
}

bool
Devirtualizer::doGuard(IRManager& irm, CFGNode* node, MethodDesc& methodDesc) {
    double methodCount = Inliner::getProfileMethodCount(irm.getCompilationInterface(), methodDesc);
    double blockCount = node->getFreq();

    if(_skipColdTargets && _hasProfileInfo && methodCount == 0)
        return false;

    //
    // Determine if a call site should be guarded
    //

    if (_doAggressiveGuardedDevirtualization) {
        // 
        // In this mode, always guard in the first pass
        //
        return true;
    }



    //
    // Only de-virtualize if there's profile information for this
    // node and the apparent target.
    //
    if(!_hasProfileInfo)
        return false;


    return (blockCount <= methodCount);
}

void
Devirtualizer::guardCallsInBlock(IRManager& regionIRM, CFGNode* node) {

    //
    // Search node for guardian call
    //
    if(node->isBlockNode()) {
        Inst* last = node->getLastInst();
        MethodInst* methodInst = 0;
        Opnd* base = 0;
        Opnd* tauNullChecked = 0;
        Opnd* tauTypesChecked = 0;
        uint32 argOffset = 0;
        if(isGuardableVirtualCall(last, methodInst, base, tauNullChecked, tauTypesChecked,
                                  argOffset)) {
            assert(methodInst && base && tauNullChecked && tauTypesChecked && argOffset);

            bool done = false;
            assert(base->getType()->isObject());
            ObjectType* baseType = (ObjectType*) base->getType();
            MethodDesc* methodDesc = methodInst->getMethodDesc();
            // Class hierarchy analysis -- can we de-virtualize without a guard?
            if(_devirtUseCHA && isPreexisting(base)) {
                ClassHierarchyMethodIterator* iterator = regionIRM.getCompilationInterface().getClassHierarchyMethodIterator(baseType, methodDesc);
                if(iterator) {
                    if(!baseType->isNullObject() && (!baseType->isAbstract() || baseType->isArray()) && !baseType->isInterface()) {
                        methodDesc = regionIRM.getCompilationInterface().getOverriddenMethod(baseType, methodDesc);
                        jitrino_assert(regionIRM.getCompilationInterface(),methodDesc);
                    }

                    if(!iterator->hasNext()) {
                        // No candidate
						Log::cat_opt_inline()->fail << "CHA no candidate " << baseType->getName() << "::" << methodDesc->getName() << methodDesc->getSignatureString() << ::std::endl;
                        jitrino_assert(regionIRM.getCompilationInterface(),0);
                    }
                
                    MethodDesc* newMethodDesc = iterator->getNext();
                    if(!iterator->hasNext()) {
                        // Only one candidate
                        Log::cat_opt_inline()->debug << "CHA devirtualize " << baseType->getName() << "::" << methodDesc->getName() << methodDesc->getSignatureString() << ::std::endl;
                        if(!baseType->isNullObject() && (!baseType->isAbstract() || baseType->isArray()) && !baseType->isInterface()) {
                            assert(newMethodDesc == methodDesc);
                        }

                        Opnd* dst = last->getDst(); 
                        uint32 numArgs = last->getNumSrcOperands()-argOffset;
                        uint32 i = 0;
                        uint32 j = argOffset;
                        Opnd** args = new (regionIRM.getMemoryManager()) Opnd*[numArgs];
                        for(; i < numArgs; ++i, ++j)
                            args[i] = last->getSrc(j-argOffset);
                        Inst* directCall = 
                            _instFactory.makeDirectCall(dst, 
                                                        tauNullChecked,
                                                        tauTypesChecked,
                                                        numArgs, args, 
                                                        newMethodDesc);
                        //
                        // copy InlineInfo
                        //
                        InlineInfo* call_ii = last->getCallInstInlineInfoPtr();
                        InlineInfo* new_ii = directCall->getCallInstInlineInfoPtr();
                        assert(call_ii && new_ii);
                        new_ii->getInlineChainFrom(*call_ii);

                        last->unlink();
                        node->append(directCall);
                        done = true;
                    }
                }
            }
            
            // If base type is concrete, consider an explicit guarded test against it
            if(!done && !baseType->isNullObject() && (!baseType->isAbstract() || baseType->isArray()) && !baseType->isInterface()) {

                NamedType* methodType = methodDesc->getParentType();
                if (_typeManager.isSubClassOf(baseType, methodType)) {
                    // only bother if the baseType has the given method
                    // for an interface call, this may not be the case
                       
                    methodDesc =
                        regionIRM.getCompilationInterface().getOverriddenMethod(baseType, methodDesc);
                    if (methodDesc) {
                        jitrino_assert(regionIRM.getCompilationInterface(), methodDesc->getParentType()->isClass());
                        methodInst->setMethodDesc(methodDesc);
                        
                        //
                        // Try to guard this call
                        //
                        if(doGuard(regionIRM, node, *methodDesc)) {
                            Log::cat_opt_inline()->debug << "Guard call to " << baseType->getName() << "::" << methodDesc->getName() << ::std::endl;
                            genGuardedDirectCall(regionIRM, node, last, methodDesc, tauNullChecked,
                                                 tauTypesChecked, argOffset);
                            Log::cat_opt_inline()->debug << "Done guarding call to " << baseType->getName() << "::" << methodDesc->getName() << ::std::endl;
                        } else {
                            Log::cat_opt_inline()->debug << "Don't guard call to " << baseType->getName() << "::" << methodDesc->getName() << ::std::endl;
                        }
                    }
                }
            }
        }
    }
}

bool
Devirtualizer::isPreexisting(Opnd* obj) {
    assert(obj->getType()->isObject());

    SsaOpnd* ssa = obj->asSsaOpnd();
    if(ssa == NULL)
        return false;

    Inst* inst = ssa->getInst();
    switch(inst->getOpcode()) {
    case Op_DefArg:
        return true;
    case Op_Copy:
    case Op_TauAsType:
    case Op_TauCast:
    case Op_TauStaticCast:
        return isPreexisting(inst->getSrc(0));
    default:
        return false;
    }
}

void
Devirtualizer::unguardCallsInRegion(IRManager& regionIRM, uint32 safetyLevel) {
    FlowGraph &regionFG = regionIRM.getFlowGraph();
    if(!regionFG.hasEdgeProfile())
        return;

    //
    // Search for previously guarded virtual calls
    //
    MemoryManager mm(regionFG.getMaxNodeId()*sizeof(CFGNode*), "Devirtualizer::unguardCallsInRegion.mm");
    StlVector<CFGNode*> nodes(mm);
    regionFG.getNodesPostOrder(nodes);
    StlVector<CFGNode*>::reverse_iterator i;
    for(i = nodes.rbegin(); i != nodes.rend(); ++i) {
        CFGNode* node = *i;
        Inst* last = node->getLastInst();
        if(last->isBranch()) {
            //
            // Check if branch is a guard
            //
            assert(last->getOpcode() == Op_Branch);
            BranchInst* branch = last->asBranchInst();
            CFGNode* dCallNode = (CFGNode*) node->getTrueEdge()->getTargetNode(); 
            CFGNode* vCallNode = (CFGNode*) node->getFalseEdge()->getTargetNode(); 

            if(branch->getComparisonModifier() != Cmp_EQ)
                continue;
            if(branch->getNumSrcOperands() != 2)
                continue;
            Opnd* src0 = branch->getSrc(0);
            Opnd* src1 = branch->getSrc(1);
            if(!src0->getType()->isVTablePtr() || !src1->getType()->isVTablePtr())
                continue;
            if(src0->getInst()->getOpcode() != Op_TauLdVTableAddr || src1->getInst()->getOpcode() != Op_GetVTableAddr)
                continue;

            Inst* ldvfnslot = vCallNode->getFirstInst()->next();
            Inst* callimem = ldvfnslot->next();
            if(ldvfnslot->getOpcode() != Op_TauLdVirtFunAddrSlot || callimem->getOpcode() != Op_IndirectMemoryCall)
                continue;

            //
            // A guard - fold based on profile results.
            //
            bool fold = false;
            bool foldDirect = false;
            if(node->getFreq() == 0) {
                fold = true;
                foldDirect = false;
            } else if(vCallNode->getFreq() == 0) {
                if(safetyLevel == 0) {
                    fold = false;
                } else if(safetyLevel == 1) {
                    Inst* inst0 = src0->getInst();
                    fold = (inst0->getOpcode() == Op_TauLdVTableAddr && isPreexisting(inst0->getSrc(0)));
                } else {
                    assert(safetyLevel == 2);
                    fold = true;
                }
                foldDirect = true;
            } else if(dCallNode->getFreq() == 0) {
                fold = true;
                foldDirect = false;
            }
            
            if(fold) {
                //
                // A compile time is compared.  Later simplification will fold branch appropriately.
                //
                branch->setSrc(1, src0);
                if(!foldDirect)
                    branch->setComparisonModifier(Cmp_NE_Un);
            }
        }
    }
}

} //namespace Jitrino 
