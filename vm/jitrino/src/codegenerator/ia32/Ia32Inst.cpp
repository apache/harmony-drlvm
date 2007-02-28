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
 * @version $Revision: 1.14.12.1.4.4 $
 */

#include "Ia32Inst.h"
#include "Ia32IRManager.h"
#include "InlineInfo.h"
#include "Ia32CFG.h"

namespace Jitrino
{
namespace Ia32{


//=================================================================================================
// class Opnd
//=================================================================================================

//_________________________________________________________________________________________________
void Opnd::assignRegName(RegName r)
{
    r=constraints[ConstraintKind_Calculated].getAliasRegName(r);
    assert(r!=RegName_Null);
    memOpndKind=MemOpndKind_Null;
    regName=r;
    constraints[ConstraintKind_Location]=Constraint(r);
    checkConstraints();
}

//_________________________________________________________________________________________________
void Opnd::assignImmValue(int64 v)
{
    memOpndKind=MemOpndKind_Null;
    if (constraints[ConstraintKind_Location].getKind() != OpndKind_Imm)
        runtimeInfo=NULL;
    immValue=v;
    constraints[ConstraintKind_Location]=Constraint(OpndKind_Imm, constraints[ConstraintKind_Initial].getSize());
    checkConstraints();
}

//_________________________________________________________________________________________________
void Opnd::assignMemLocation(MemOpndKind k, Opnd * _base, Opnd * _index, Opnd * _scale, Opnd * _displacement)
{
    assert(_base||_index||_displacement);
    assert(!_scale||_index);

    constraints[ConstraintKind_Location]=Constraint(OpndKind_Mem, constraints[ConstraintKind_Initial].getSize());
    memOpndKind=k;

    checkConstraints();

    if (_base) 
        setMemOpndSubOpnd(MemOpndSubOpndKind_Base, _base);
    else 
        memOpndSubOpnds[MemOpndSubOpndKind_Base]=NULL;
    if (_index) 
        setMemOpndSubOpnd(MemOpndSubOpndKind_Index, _index);
    else 
        memOpndSubOpnds[MemOpndSubOpndKind_Index]=NULL;
    if (_scale) 
        setMemOpndSubOpnd(MemOpndSubOpndKind_Scale, _scale);
    else 
        memOpndSubOpnds[MemOpndSubOpndKind_Scale]=NULL;
    if (_displacement) 
        setMemOpndSubOpnd(MemOpndSubOpndKind_Displacement, _displacement);
    else 
        memOpndSubOpnds[MemOpndSubOpndKind_Displacement]=NULL;

}

//_________________________________________________________________________________________________
void Opnd::setMemOpndSubOpnd(MemOpndSubOpndKind so, Opnd * opnd)
{
    assert(opnd);
    assert((so != MemOpndSubOpndKind_Displacement)  || (INT_MAX >= opnd->getImmValue() && INT_MIN <= opnd->getImmValue()));
    assert((so != MemOpndSubOpndKind_Scale)         || ((uint64)opnd->getImmValue() <= 8));
    assert(memOpndKind!=MemOpndKind_Null);
    memOpndSubOpnds[so]=opnd;
}

//_________________________________________________________________________________________________
bool Opnd::replaceMemOpndSubOpnd(Opnd * opndOld, Opnd * opndNew)
{
    bool replaced = false;
    if (memOpndKind != MemOpndKind_Null){
        assert(isPlacedIn(OpndKind_Mem));
        for (uint32 i=0; i<MemOpndSubOpndKind_Count; i++)
            if (memOpndSubOpnds[i]!=NULL && memOpndSubOpnds[i]==opndOld) {
                setMemOpndSubOpnd((MemOpndSubOpndKind)i, opndNew);
                replaced = true;
            }
    }
    return replaced;
}

//_________________________________________________________________________________________________
bool Opnd::replaceMemOpndSubOpnds(Opnd * const * opndMap)
{
    bool replaced = false;
    if (memOpndKind != MemOpndKind_Null){
        assert(isPlacedIn(OpndKind_Mem));
        for (uint32 i=0; i<MemOpndSubOpndKind_Count; i++)
            if (memOpndSubOpnds[i]!=NULL && opndMap[memOpndSubOpnds[i]->id]!=NULL){
                setMemOpndSubOpnd((MemOpndSubOpndKind)i, opndMap[memOpndSubOpnds[i]->id]);
                replaced = true;
            }
    }
    return replaced;
}


#ifdef _DEBUG
//_________________________________________________________________________________________________
void Opnd::checkConstraints()
{
    assert(constraints[ConstraintKind_Initial].contains(constraints[ConstraintKind_Calculated]));
    assert(constraints[ConstraintKind_Calculated].contains(constraints[ConstraintKind_Location]));
}
#endif

//_________________________________________________________________________________________________
void Opnd::setCalculatedConstraint(Constraint c)
{
    constraints[ConstraintKind_Calculated]=c & constraints[ConstraintKind_Initial];
    checkConstraints();
    assert(!constraints[ConstraintKind_Calculated].isNull());
}

//_________________________________________________________________________________________________
void Opnd::addRefCount(uint32& index, uint32 blockExecCount)
{
    if (blockExecCount==0)
        blockExecCount=1;
    refCount++;
    if (id==EmptyUint32)
        id=index++;
    if (memOpndKind != MemOpndKind_Null){
        for (uint32 i=0; i<MemOpndSubOpndKind_Count; i++)
            if (memOpndSubOpnds[i]!=NULL){
                memOpndSubOpnds[i]->addRefCount(index, blockExecCount);
            }
    }
}

//_________________________________________________________________________________________________
void Opnd::setDefiningInst(Inst * inst)
{
    if (defScope==DefScope_Temporary||defScope==DefScope_Null){
        if (definingInst==NULL){
            defScope=DefScope_Temporary;
            definingInst=inst;
        }else{
            if (inst->getNode()==definingInst->getNode())
                defScope=DefScope_SemiTemporary;
            else{
                defScope=DefScope_Variable;
                definingInst=NULL;
            }
        }
    }else if (defScope==DefScope_SemiTemporary){
        assert(definingInst!=NULL);     
        if (inst->getNode()!=definingInst->getNode()){
            defScope=DefScope_Variable;
            definingInst=NULL;
        }
    }
}

//=================================================================================================
// class Inst
//=================================================================================================
void* Inst::operator new(size_t sz, MemoryManager& mm, uint32 opndCount)
{   
    Inst * p = (Inst*)mm.alloc(sz + Inst::getOpndChunkSize(opndCount)); 
    p->allocatedOpndCount = opndCount; p->opnds = (Opnd**)((uint8*)p + sz);
    return p; 
}

//_________________________________________________________________________________________________
void Inst::insertOpnd(uint32 idx, Opnd * opnd, uint32 roles)
{
    assert(opndCount < allocatedOpndCount);
    
    if (idx < defOpndCount)
        roles |= OpndRole_Def;

    if (roles & OpndRole_Def){
        if (idx > defOpndCount)
            idx = defOpndCount;
        defOpndCount++;
    }
    
    if (idx > opndCount)
        idx = opndCount;

    uint32 * opndRoles = getOpndRoles();
    Constraint * opndConstraints = getConstraints();

    for (uint32 i = opndCount; i > idx; i--){
        opnds[i] = opnds[i - 1];
        opndRoles[i] = opndRoles[i - 1];
        opndConstraints[i] = opndConstraints[i - 1];
    }

    opnds[idx] = opnd;
    opndRoles[idx] = roles;
    
    opndCount++;
}

//_________________________________________________________________________________________________
uint32 Inst::countOpnds(uint32 roles)const
{
    uint32 count=0;
    if ( (roles & OpndRole_InstLevel) != 0 ){
        const uint32 * opndRoles = getOpndRoles();
        for (uint32 i = 0, n = opndCount; i < n; i++){
            uint32 r = opndRoles [i];
            if ( (r & roles & OpndRole_FromEncoder) != 0 && (r & roles & OpndRole_ForIterator) != 0 )
                count++;
        }
    }
    if ( (roles & OpndRole_OpndLevel) != 0 && (roles & OpndRole_Use) != 0  ){
        for (uint32 i = 0, n = opndCount; i < n; i++){
            const Opnd * opnd = opnds[i];
            if ( opnd->memOpndKind != MemOpndKind_Null ){
                const Opnd * const * subOpnds = opnd->getMemOpndSubOpnds();
                for (uint32 j = 0; j < MemOpndSubOpndKind_Count; j++){
                    const Opnd * subOpnd = subOpnds[j];
                    if (subOpnd != NULL)
                        count++;
                }
            }
        }
    }
    return count;
}

//_________________________________________________________________________________________________
Constraint Inst::getConstraint(uint32 idx, uint32 memOpndMask, OpndSize size)
{ 
    Constraint c(OpndKind_Any); 
    if (idx < opndCount){
        const uint32 * opndRoles = getOpndRoles();
        const Constraint * constraints = getConstraints();
        c = constraints[idx];        
        if ((opndRoles[idx] & OpndRole_Explicit) && (properties & Inst::Properties_MemoryOpndConditional)){
            bool removeMemory = false;
            if (memOpndMask == 0xFFFFFFFF){// Strong constraint requested 
                removeMemory = true;
            } else if (memOpndMask != 0) {
                for (unsigned j = 0; j < opndCount; j++) {
                    if (j != idx && ((1 << j) & memOpndMask) && (opndRoles[j] & OpndRole_Explicit)) {
                        if (getOpnd(j)->getConstraint(Opnd::ConstraintKind_Location).getKind() == OpndKind_Mem){
                            removeMemory = true;
                            break;
                }
            }
            }
        }
            if (removeMemory) 
                c = Constraint((OpndKind)(c.getKind() &~ OpndKind_Mem), c.getSize(), c.getMask());
        }
    }else{
        c = opnds[(idx - opndCount) / 4]->getMemOpndSubOpndConstraint((MemOpndSubOpndKind)((idx - opndCount) & 3));
    }
    return size==OpndSize_Null?c:c.getAliasConstraint(size);
}

//_________________________________________________________________________________________________
void Inst::setOpnd(uint32 index, Opnd * opnd)
{
    Opnd ** opnds = getOpnds();
    if (index < opndCount) {
        Constraint cc = opnds[index]->getConstraint(Opnd::ConstraintKind_Initial);
        opnds[index] = opnd;
        if(!hasKind(Kind_PseudoInst) 
            && (getOpndRoles()[index] & OpndRole_Explicit) 
            && cc != opnd->getConstraint(Opnd::ConstraintKind_Initial)
            && !Encoder::isOpndAllowed(opcodeGroup, getExplicitOpndIndexFromOpndRoles(getOpndRoles()[index]), opnd->getConstraint(Opnd::ConstraintKind_Initial), getForm()==Form_Extended, true)
            ) {

            BasicBlock * bb = getBasicBlock();

            //instruction must be inserted into a basic block to be modifiable.
            //it caused by necessity of reference to irManager for implicit operands
            //assigning (flags, for example)
            assert(bb);
            assignOpcodeGroup(&bb->getIRManager());
            assert(opcodeGroup);
        }

    } else{
        opnds[(index - opndCount) / 4]->setMemOpndSubOpnd((MemOpndSubOpndKind)((index - opndCount) & 3), opnd);
    }
    verify();
}

//_________________________________________________________________________________________________
bool Inst::replaceOpnd(Opnd * oldOpnd, Opnd * newOpnd, uint32 opndRoleMask)
{
    bool replaced = false;
    assert(newOpnd != NULL);
    for (uint32 i=0, n=getOpndCount(); i<n; i++){
        uint32 opndRole=getOpndRoles(i);
        if (
            (opndRole&opndRoleMask&OpndRole_FromEncoder)!=0 && 
            (opndRole&opndRoleMask&OpndRole_ForIterator)!=0
            ){
            Opnd * opnd=getOpnd(i);
            if (opnd==oldOpnd){
                assert((opndRole&OpndRole_Changeable)!=0);
                setOpnd(i, newOpnd);
                opnd=newOpnd;
                replaced = true;
            }
            if ((opndRoleMask&OpndRole_OpndLevel)!=0)
                replaced |= opnd->replaceMemOpndSubOpnd(oldOpnd, newOpnd);
        }
    }

    return replaced;
}

bool Inst::getPureDefProperty() const {
    if (getProperties() & Properties_PureDef) {
        assert(getOpndCount(OpndRole_InstLevel|OpndRole_Use)==2);
        Opnd * use = NULL;
        for (uint32 i=0, n=getOpndCount(); i<n; i++){
            if (getOpndRoles(i)&OpndRole_Use) {
                if(!use) {
                   use = getOpnd(i);
                } else {            
                    return use == getOpnd(i);
                }
            }
        }
    }
    return false;
}
//_________________________________________________________________________________________________
bool Inst::replaceOpnds(Opnd * const * opndMap, uint32 opndRoleMask)
{
    bool replaced = false;
    for (uint32 i=0, n=getOpndCount(); i<n; i++){
        uint32 opndRole=getOpndRoles(i);
        if (
            (opndRole&opndRoleMask&OpndRole_FromEncoder)!=0 && 
            (opndRole&opndRoleMask&OpndRole_ForIterator)!=0
            ){
            Opnd * opnd=getOpnd(i);
            Opnd * newOpnd=opndMap[opnd->getId()];
            if (newOpnd!=NULL){
                assert((opndRole&OpndRole_Changeable)!=0);
                setOpnd(i, newOpnd);
                opnd=newOpnd;
                replaced = true;
            }
            if ((opndRoleMask&OpndRole_OpndLevel)!=0)
                replaced |= opnd->replaceMemOpndSubOpnds(opndMap);
        }
    }
    return replaced;
}

//_________________________________________________________________________________________________
void Inst::swapOperands(uint32 idx0, uint32 idx1)
{
    Opnd * opnd0=getOpnd(idx0), * opnd1=getOpnd(idx1);
    setOpnd(idx1, opnd0);
    setOpnd(idx0, opnd1);
    verify();
}

//_________________________________________________________________________________________________
void Inst::changeInstCondition(ConditionMnemonic cc, IRManager * irManager)
{
    assert(getProperties()&Properties_Conditional);
    Mnemonic baseMnemonic=getBaseConditionMnemonic(mnemonic);
    assert(baseMnemonic!=Mnemonic_Null);
    mnemonic=(Mnemonic)(baseMnemonic+cc);
    assignOpcodeGroup(irManager);
    assert(opcodeGroup!=NULL);
}

//_________________________________________________________________________________________________
void Inst::reverse(IRManager * irManager)
{
    assert(canReverse());
    Mnemonic baseMnemonic=getBaseConditionMnemonic(mnemonic);
    assert(baseMnemonic!=Mnemonic_Null);
    changeInstCondition(reverseConditionMnemonic((ConditionMnemonic)(mnemonic-baseMnemonic)), irManager);
}

//_________________________________________________________________________________________________
uint8* Inst::emit(uint8* stream)
{
    uint8 * instEnd = stream;
    if (!hasKind(Inst::Kind_PseudoInst)){
        assert(mnemonic!=Mnemonic_Null);
        assert(form==Form_Native);
        instEnd = Encoder::emit(stream, this);
    }
    setCodeSize((uint32)(instEnd - stream));
    return instEnd;
}

//_________________________________________________________________________________________________
void Inst::initFindInfo(Encoder::FindInfo& fi, Opnd::ConstraintKind opndConstraintKind)const
{
    Opnd * const * opnds = getOpnds();
    const uint32 * roles = getOpndRoles();
    uint32 count = 0, defCount = 0, defCountFromInst = defOpndCount;
    for (uint32 i=0, n=opndCount; i<n; i++){
        uint32 r = roles[i];
        if (r & Inst::OpndRole_Explicit){
            fi.opndConstraints[count++] = opnds[i]->getConstraint(opndConstraintKind);
            if (i < defCountFromInst)
                defCount++;
        }
    }
    fi.defOpndCount = defCount;
    fi.opndCount = count;
    fi.opcodeGroup= opcodeGroup;
    fi.mnemonic = mnemonic;
    fi.isExtended = (Form)form == Form_Extended;
}

//_________________________________________________________________________________________________
void Inst::fixOpndsForOpcodeGroup(IRManager * irManager)
{
    uint32 handledExplicitOpnds = 0, handledImplicitOpnds = 0;

    uint32 * roles = getOpndRoles();
    Constraint * constraints = getConstraints();

    Form f = getForm();

    for (uint32 i=0, n=opndCount; i<n; i++){
        if (roles[i] & Inst::OpndRole_Explicit){
            uint32 idx, r;  
            if (f == Form_Native){
                idx = handledExplicitOpnds; 
                r = Encoder::getOpndRoles(opcodeGroup->opndRoles, idx);
                if ( (r & OpndRole_Def) != 0 && defOpndCount<=i)
                    defOpndCount = i + 1;
            }else{
                idx = opcodeGroup->extendedToNativeMap[handledExplicitOpnds];
                r = i < defOpndCount ? OpndRole_Def : OpndRole_Use;
            }
            r |= Inst::OpndRole_Explicit | (handledExplicitOpnds << 16);
            roles[i] = r;
            constraints[i] = opcodeGroup->opndConstraints[idx];
            handledExplicitOpnds++;
        }else if (roles[i] & Inst::OpndRole_Implicit){
            assert(handledImplicitOpnds < opcodeGroup->implicitOpndRoles.count);
            assert(opnds[i]->getRegName() == opcodeGroup->implicitOpndRegNames[handledImplicitOpnds]);
            handledImplicitOpnds++;
        }
    }

    for (uint32 i = handledImplicitOpnds; i < opcodeGroup->implicitOpndRoles.count; i++){
        RegName iorn = opcodeGroup->implicitOpndRegNames[i];
        Opnd * implicitOpnd = irManager->getRegOpnd(iorn);
        uint32 implicitOpndRoles = 
            Encoder::getOpndRoles(opcodeGroup->implicitOpndRoles, i) | Inst::OpndRole_Implicit;
        if (implicitOpndRoles & OpndRole_Def){
            insertOpnd(defOpndCount, implicitOpnd, implicitOpndRoles);
            constraints[defOpndCount - 1] = Constraint(iorn);
        }else{
            insertOpnd(opndCount, implicitOpnd, implicitOpndRoles);
            constraints[opndCount - 1] = Constraint(iorn);
        }
    }
}

//_________________________________________________________________________________________________
void Inst::assignOpcodeGroup(IRManager * irManager)
{   
    if (hasKind(Inst::Kind_PseudoInst)){
        assert(getForm() == Form_Extended);
        opcodeGroup= (Encoder::OpcodeGroup *)Encoder::getDummyOpcodeGroup();
        uint32 * roles = getOpndRoles();
        uint32 i = 0;
        for (uint32 n = defOpndCount; i < n; i++)
            roles[i] = OpndRole_Auxilary | OpndRole_Def;
        for (uint32 n = opndCount; i < n; i++)
            roles[i] = OpndRole_Auxilary | OpndRole_Use;
    }else{
        Encoder::FindInfo fi;
        initFindInfo(fi, Opnd::ConstraintKind_Initial);
        opcodeGroup=(Encoder::OpcodeGroup *)Encoder::findOpcodeGroup(fi);
        assert(opcodeGroup);  // Checks that the requested mnemonic is implemented in Ia32EncodingTable
        fixOpndsForOpcodeGroup(irManager);
    }
    properties = opcodeGroup->properties;
}

//_________________________________________________________________________________________________
void Inst::makeNative(IRManager * irManager)
{
    if (getForm()==Form_Native  || hasKind(Kind_PseudoInst) )
        return;
    assert(opcodeGroup);

    Node* bb = getNode();

    uint32 * opndRoles = getOpndRoles();
    Constraint * constraints = getConstraints();

    int32 defs[IRMaxNativeOpnds]={ -1, -1, -1, -1 };
    for (uint32 i=0; i<opndCount; i++){
        uint32 r = opndRoles[i];
        if ((r & OpndRole_Explicit) == 0) continue;
        uint32 extendedIdx = r >> 16;
        uint32 nativeIdx = opcodeGroup->extendedToNativeMap[extendedIdx];
        assert(nativeIdx < IRMaxNativeOpnds);
        int32 defNativeIdx = defs[nativeIdx];
        if (defNativeIdx != -1){
            assert(i >= defOpndCount);
            opndRoles[defNativeIdx] |= OpndRole_Use;
            if (bb && opnds[defNativeIdx] != opnds[i]){
                Inst * moveInst=irManager->newCopySequence(Mnemonic_MOV, opnds[defNativeIdx], opnds[i]); 
                moveInst->insertBefore(this);
            }
            opnds[i] = NULL;
        }else
            defs[nativeIdx] = i;
    }

    uint32 packedOpnds = 0, explicitOpnds = 0;
    for (uint32 i = 0, n = opndCount; i < n; i++){
        if (opnds[i] == NULL) continue;
        uint32 r = opndRoles[i];
        if ((r & OpndRole_Explicit) != 0){
            r = (r & 0xffff) | (explicitOpnds << 16);
            explicitOpnds++;
        }
        if (i > packedOpnds){
            opnds[packedOpnds] = opnds[i];
            constraints[packedOpnds] = constraints[i];
            opndRoles[packedOpnds] = r;
        }
        packedOpnds++;
    }

    opndCount=packedOpnds;
    form = Form_Native;
}

//_________________________________________________________________________________________________
void * Inst::getCodeStartAddr() const 
{
    BasicBlock* bb = getBasicBlock();
    return bb!=NULL?(uint8*)bb->getCodeStartAddr()+codeOffset:0;
}


//_________________________________________________________________________________________________
Edge::Kind Inst::getEdgeKind(const Edge* edge) const 
{
    assert(edge->getSourceNode()->isBlockNode() && edge->getTargetNode()->isBlockNode());
    assert (!hasKind(Inst::Kind_BranchInst) && !hasKind(Inst::Kind_SwitchInst));
    return Edge::Kind_Unconditional;
}

//=========================================================================================================
//   class GCInfoPseudoInst
//=========================================================================================================

GCInfoPseudoInst::GCInfoPseudoInst(IRManager* irm, int id)
: Inst(Mnemonic_NULL, id, Inst::Form_Extended), 
offsets(irm->getMemoryManager()), desc(NULL)
{ 
    kind = Kind_GCInfoPseudoInst;
}

//=================================================================================================
// class BranchInst
//=================================================================================================

void BranchInst::reverse(IRManager * irManager)
{
    assert(canReverse());
    assert(node->isConnectedTo(true, trueTarget) && node->isConnectedTo(true, falseTarget));
    ControlTransferInst::reverse(irManager);
    Node* tmp = trueTarget;
    trueTarget = falseTarget;
    falseTarget = tmp;
}

//_________________________________________________________________________________________________
Edge::Kind BranchInst::getEdgeKind(const Edge* edge) const 
{
    Node* node = edge->getTargetNode();
    if (node == trueTarget) {
        return Edge::Kind_True;
    }
    assert(node == falseTarget);
    return Edge::Kind_False;
}

//_________________________________________________________________________________________________
bool BranchInst::canReverse() const 
{
    return node!=NULL && isDirect() && ControlTransferInst::canReverse();
}

//_________________________________________________________________________________________________
// called from CFG when edge target is replaced
void BranchInst::updateControlTransferInst(Node* oldTarget, Node* newTarget) 
{
    if (!newTarget->isBlockNode()) {
        assert(newTarget->isDispatchNode());
        return;
    }
    if (oldTarget == trueTarget) {
        trueTarget = newTarget;
    } else {
        assert(oldTarget == falseTarget);
        falseTarget = newTarget;
    }
}

//_________________________________________________________________________________________________
// called from CFG when 2 blocks are merging and one of  the branches is redundant.
void BranchInst::removeRedundantBranch()
{
    unlink();
};

//_________________________________________________________________________________________________
void BranchInst::verify() const 
{
    Inst::verify();
    assert(trueTarget!=NULL && falseTarget!=NULL);
    assert(trueTarget->isBlockNode() && falseTarget->isBlockNode());
    assert(node->isConnectedTo(true, trueTarget));
    assert(node->isConnectedTo(true, falseTarget));
}

//=========================================================================================================
//   class SwitchInst
//=========================================================================================================

Node* SwitchInst::getTarget(uint32 i)const
{
    Opnd * tableAddr=getTableAddress();
    Opnd::RuntimeInfo * ri=tableAddr->getRuntimeInfo();
    assert(ri->getKind()==Opnd::RuntimeInfo::Kind_ConstantAreaItem);
    ConstantAreaItem * cai = (ConstantAreaItem *)ri->getValue(0);
    assert(i<cai->getSize()/sizeof(Node*));
    return ((Node**)cai->getValue())[i];
}

//_________________________________________________________________________________________________
Edge::Kind SwitchInst::getEdgeKind(const Edge* edge) const 
{
#ifdef _DEBUG
    Node* target = edge->getTargetNode();
    assert(target->isBlockNode());
    bool found = false;
    for (uint32 i =0 ; i < getNumTargets(); i++) {
        Node* myTarget = getTarget(i);
        if (myTarget == target) {
            found = true;
            break;
        }
   }
    assert(found);
#endif
    return Edge::Kind_Unconditional;
}

//_________________________________________________________________________________________________
uint32 SwitchInst::getNumTargets() const 
{
    Opnd * tableAddr=getTableAddress();
    Opnd::RuntimeInfo * ri= tableAddr->getRuntimeInfo();
    assert(ri->getKind()==Opnd::RuntimeInfo::Kind_ConstantAreaItem);
    ConstantAreaItem * cai = (ConstantAreaItem *)ri->getValue(0);
    return cai->getSize() / sizeof(Node*);
}

//_________________________________________________________________________________________________
void SwitchInst::setTarget(uint32 i, Node* bb)
{
    assert(bb->isBlockNode());
    Opnd * tableAddr=getTableAddress();
    Opnd::RuntimeInfo * ri=tableAddr->getRuntimeInfo();
    assert(ri->getKind()==Opnd::RuntimeInfo::Kind_ConstantAreaItem);
    ConstantAreaItem * cai = (ConstantAreaItem *)ri->getValue(0);
    assert(i<cai->getSize()/sizeof(Node*));
    ((Node**)cai->getValue())[i]=bb;
}

//_________________________________________________________________________________________________
void SwitchInst::replaceTarget(Node * bbFrom, Node * bbTo)
{
    assert(bbTo->isBlockNode());
    Opnd * tableAddr=getTableAddress();
    Opnd::RuntimeInfo * ri=tableAddr->getRuntimeInfo();
    assert(ri->getKind()==Opnd::RuntimeInfo::Kind_ConstantAreaItem);
    ConstantAreaItem * cai = (ConstantAreaItem *)ri->getValue(0);
    Node** bbs=(Node**)cai->getValue();
#ifdef _DEBUG
    bool found = false;
#endif
    for (uint32 i=0, n=cai->getSize()/sizeof(Node*); i<n; i++) {
        if (bbs[i]==bbFrom) {
#ifdef _DEBUG
            found = true;
#endif
            bbs[i]=bbTo; 
        }
    }
    assert(found);
}

//_________________________________________________________________________________________________
// called from CFG when edge target is replaced
void SwitchInst::updateControlTransferInst(Node* oldTarget, Node* newTarget) 
{
    if (!newTarget->isBlockNode()) {
        assert(newTarget->isDispatchNode());
        return;
    }
    replaceTarget(oldTarget, newTarget);
}

//_________________________________________________________________________________________________
// called from CFG when 2 blocks are merging and one of  the branches is redundant.
void SwitchInst::removeRedundantBranch()
{
    assert(0);
}

//_________________________________________________________________________________________________
Opnd * SwitchInst::getTableAddress() const
{
#ifdef _EM64T_
    return tableAddr;
#else
    return getOpnd(0)->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
#endif
}

//_________________________________________________________________________________________________
void SwitchInst::verify() const 
{
    Inst::verify();
#ifdef _DEBUG
    for (uint32 i=0, n = getNumTargets(); i<n; ++i) {
        Node* target = getTarget(i);
        assert(target!=NULL && target->isBlockNode());
        assert(node->isConnectedTo(true, target));
    }
#endif
}


//=========================================================================================================
//   class CallingConventionClient
//=========================================================================================================
//_________________________________________________________________________________________________
void JumpInst::verify() const {
    Inst::verify();
#ifdef _DEBUG
    if (node!=NULL) {
        assert(node->getOutDegree()==1 || node->getOutDegree()==2);
        assert(node->getUnconditionalEdge()!=NULL);
        assert(node->getOutDegree() == 1 || node->getExceptionEdge()!=NULL);
    }
#endif
}

//=========================================================================================================
//   class CallingConventionClient
//=========================================================================================================
//_________________________________________________________________________________________________
void CallingConventionClient::finalizeInfos(Inst::OpndRole role, CallingConvention::ArgKind argKind)
{
    assert(callingConvention!=NULL);
    StlVector<CallingConvention::OpndInfo> & infos = getInfos(role);
    callingConvention->getOpndInfo(argKind, (uint32)infos.size(), &infos.front());
    bool lastToFirst=callingConvention->pushLastToFirst();
    uint32 slotNumber=0;
    for (
        uint32 i=lastToFirst?0:(uint32)infos.size()-1, 
        end=lastToFirst?(uint32)infos.size():(uint32)-1, 
        inc=lastToFirst?1:-1;
        i!=end;
        i+=inc
        ){
        CallingConvention::OpndInfo & info=infos[i];
        for (uint32 j=0; j<info.slotCount; j++){
            if (!info.isReg)
                info.slots[j]=0xFFFF & slotNumber++;
        }
    }
    (role==Inst::OpndRole_Def?defArgStackDepth:useArgStackDepth)=slotNumber*sizeof(POINTER_SIZE_INT); 
}

//_________________________________________________________________________________________________
void CallingConventionClient::layoutAuxilaryOpnds(Inst::OpndRole role, OpndKind kindForStackArgs)
{
    StlVector<CallingConvention::OpndInfo> & infos = getInfos(role);
    StlVector<StackOpndInfo> & stackOpndInfos = getStackOpndInfos(role);
    uint32 slotSize=sizeof(POINTER_SIZE_INT);
    uint32 regArgCount=0, stackArgCount=0;
    Inst::Opnds opnds(ownerInst, Inst::OpndRole_Auxilary|role);
    Inst::Opnds::iterator handledOpnds=opnds.begin();
    for (uint32 i=0, n=(uint32)infos.size(); i<n; i++){
        const CallingConvention::OpndInfo& info=infos[i];
#ifdef _DEBUG
        bool eachSlotRequiresOpnd=false;
#endif
        uint32 offset=0;
        for (uint32 j=0, cbCurrent=0; j<info.slotCount; j++){
            Opnd * opnd=opnds.getOpnd(handledOpnds);
            OpndSize sz=opnd->getSize();
            uint32 cb=getByteSize(sz);
            RegName r=(RegName)info.slots[j];
            if (info.isReg) {
                r=Constraint::getAliasRegName(r,sz);
                assert(r!=RegName_Null);
#ifdef _DEBUG
                eachSlotRequiresOpnd=true;
#endif
                cbCurrent+=getByteSize(getRegSize(r));
            }else{
                if (cbCurrent==0)
                    offset=(info.slots[j] & 0xffff)*slotSize;
                cbCurrent+=slotSize;  
            }

            if (cbCurrent>=cb){
                if (info.isReg){
                    ownerInst->setConstraint(handledOpnds, r);
                    regArgCount++;
                }else{
                    ownerInst->setConstraint(handledOpnds, Constraint(kindForStackArgs, sz));
                    stackArgCount++;
                    StackOpndInfo sainfo={ handledOpnds, offset };
                    stackOpndInfos.push_back(sainfo);
                }
                handledOpnds = opnds.next(handledOpnds);
#ifdef _DEBUG
                eachSlotRequiresOpnd=false;
#endif
                cbCurrent=0;
            }
#ifdef _DEBUG
            assert(!eachSlotRequiresOpnd); 
#endif
        }
    }
    if (stackArgCount>0)
        sort(stackOpndInfos.begin(), stackOpndInfos.end());
    assert(handledOpnds == opnds.end());
    assert(stackArgCount==stackOpndInfos.size());
}


//=========================================================================================================
//   class EntryPointPseudoInst
//=========================================================================================================
EntryPointPseudoInst::EntryPointPseudoInst(IRManager * irm, int id, const CallingConvention * cc)
#ifdef _EM64T_
    : Inst(Mnemonic_NULL, id, Inst::Form_Extended), thisOpnd(0), callingConventionClient(irm->getMemoryManager(), cc)
#else
    : Inst(Mnemonic_NULL, id, Inst::Form_Extended), callingConventionClient(irm->getMemoryManager(), cc)
#endif
{   kind=Kind_EntryPointPseudoInst;  callingConventionClient.setOwnerInst(this); }
//_________________________________________________________________________________________________________
Opnd * EntryPointPseudoInst::getDefArg(uint32 i)const
{ 
    return NULL;
}


//=================================================================================================
// class CallInst
//=================================================================================================
//_________________________________________________________________________________________________
CallInst::CallInst(IRManager * irm, int id, const CallingConvention * cc, InlineInfo* ii)  
        : ControlTransferInst(Mnemonic_CALL, id), callingConventionClient(irm->getMemoryManager(), cc), inlineInfo(NULL)
{ 
    if (ii && (!ii->isEmpty()) ) {
        inlineInfo = ii;
    }
    kind=Kind_CallInst; 
    callingConventionClient.setOwnerInst(this); 
}

//=================================================================================================
// class RetInst
//=================================================================================================
//_________________________________________________________________________________________________
RetInst::RetInst(IRManager * irm, int id)  
        : ControlTransferInst(Mnemonic_RET, id), 
        callingConventionClient(irm->getMemoryManager(), irm->getCallingConvention())
{ 
    kind=Kind_RetInst; 
    callingConventionClient.setOwnerInst(this); 
}


}};  // namespace Ia32

