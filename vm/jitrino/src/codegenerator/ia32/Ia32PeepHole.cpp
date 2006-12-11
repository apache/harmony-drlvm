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
 * @author Alexander Astapchuk
 */

#include "Ia32CgUtils.h"

namespace Jitrino {
namespace Ia32 {


class PeepHoleOpt;
static const char* help = 
"Performs simple local (per-BB) or per-Inst optimizations.\n"
"Some of them include:\n"
"\t Inlined F2I conversion\n"
"A better instructions selection:\n"
"\t Change 32bit immediate values to 8bit in ALU instructions\n"
"\t MOVSS/MOVSD replaced with MOVQ\n"
"\t MOVSS/MOVSD xmm, [memconst=0.] => PXOR xmm, xmm\n"
"It's recommended to have 2 passes of peephole: the first one before\n"
"a register allocator - to inline the conversions and provide more\n"
"opportunities for further optimization. And the second one - after\n"
"the register allocator to improve the instructions selection."
;

static ActionFactory<PeepHoleOpt> _staticAutoRegister("peephole", help);

class PeepHoleOpt : 
    public SessionAction, 
    protected OpndUtils, 
    protected InstUtils, 
    protected SubCfgBuilderUtils 
{
private:
    // Virtuals
    uint32  getSideEffects(void) const
    {
        return m_bHadAnyChange ? (uint32)-1 : 0;
    }
private:
    enum Changed { 
    /// Nothing was changed
    Changed_Nothing, 
    /**
     * One or more Opnds were changed/added/removed - might need to 
     * update liveness info.
     */
    Changed_Opnd, 
    /**
     * One or more Insts were changed/added/removed - might need to 
     * update Insts list.
     */
    Changed_Inst, 
    /**
     * One or more Nodes were changed/added/removed - might need to 
     * update Nodes list.
     */
    Changed_Node
    };
    //
    // General machinery. 
    // TODO: It's better to separate the general CFG-walking machinery into
    // a separate class.
    //
    void runImpl(void);
    Changed handleBasicBlock(Node* node);
    Changed handleInst(Inst* inst);
    //
    // 
    //
    Changed handleInst_Call(Inst* inst);
    Changed handleInst_HelperCall(Inst* inst, const  Opnd::RuntimeInfo* ri);
    Changed handleInst_Convert_F2I_D2I(Inst* inst);
    Changed handleInst_ALU(Inst* inst);
    Changed handleInst_SSEMov(Inst* inst);
    Changed handleInst_SSEXor(Inst* inst);
    //
    // Helpers
    //
    //
    bool m_bHadAnyChange;
}; // ~PeepHoleOpt

void PeepHoleOpt::runImpl(void)
{
    setIRManager(irManager);
    m_bHadAnyChange = false;
    // organize an infinity loop and keep spinning till we have any change.
    // thought have a safety counter to prevent a really infinity in case 
    // anything goes wrong in runtime
    bool keepGoing = true;
    unsigned safetyCounter = 0;
    do {
        keepGoing = false;
        const Nodes& nodes = irManager->getFlowGraph()->getNodes();
        for (Nodes::const_iterator citer = nodes.begin(); 
            citer != nodes.end(); ++citer) {
            Node* node = *citer;
            if (!node->isBlockNode()) {
                continue;
            }
            Changed whatChanged = handleBasicBlock(node);
            if (whatChanged != Changed_Nothing) {
                m_bHadAnyChange = true;
                keepGoing = true;
            }
            if (whatChanged == Changed_Node) {
                break;
            }
        }
        ++safetyCounter;
        if(safetyCounter > 100000) {
            // I hardly believe in a method that has more than 100K 
            // opportunities to fix in peephole. 
            // Most probably self bug - assert() in debug mode, stop trying
            // in release.
            assert(false);
            keepGoing = false;
        }
    } while(keepGoing);
}


PeepHoleOpt::Changed PeepHoleOpt::handleBasicBlock(Node* node)
{
    Inst* inst = (Inst*)node->getFirstInst();
    Changed changedInBB = Changed_Nothing;
    while (inst != NULL) {
        Inst* savePrev = inst->getPrevInst();
        Changed whatChanged = handleInst(inst);
        if (whatChanged == Changed_Node) {
            // Need to scan the CFG again.
            return Changed_Node;
        }
        Inst* next = NULL;
        if (whatChanged == Changed_Inst) {
            changedInBB = Changed_Inst;
            // Inst was replaced, or deleted, or new Inst was added - 
            // proceed with this new or updated instruction(s) again
            if (savePrev == NULL) {
                next = (Inst*)node->getFirstInst();
            }
            else {
                next = savePrev->getNextInst();
            }
        }
        else {
            assert(whatChanged == Changed_Nothing || whatChanged == Changed_Opnd);
            if (changedInBB != Changed_Nothing) {
                changedInBB = whatChanged;
            }
            next = inst->getNextInst();
        }
        inst = next;
    }
    return changedInBB;
}


PeepHoleOpt::Changed PeepHoleOpt::handleInst(Inst* inst)
{
    if (isPseudoInst(inst)) {
        return Changed_Nothing;
    }

    Mnemonic mnemonic = inst->getMnemonic();
    switch(mnemonic) {
    case Mnemonic_CALL:
        return handleInst_Call(inst);
    case Mnemonic_ADD:
    case Mnemonic_SUB:
    case Mnemonic_NOT:
    case Mnemonic_AND:
    case Mnemonic_OR:
    case Mnemonic_XOR:
    case Mnemonic_CMP:
    case Mnemonic_TEST:
        return handleInst_ALU(inst);
    case Mnemonic_MOVSS:
    case Mnemonic_MOVSD:
        return handleInst_SSEMov(inst);
    case Mnemonic_XORPS:
    case Mnemonic_XORPD:
        return handleInst_SSEXor(inst);
    default:
        break;
    }
    return Changed_Nothing;
}

PeepHoleOpt::Changed PeepHoleOpt::handleInst_Call(Inst* inst)
{
    assert(inst->getMnemonic() == Mnemonic_CALL);
    CallInst* callInst = (CallInst*)inst;
    unsigned targetOpndIndex = callInst->getTargetOpndIndex();
    Opnd* targetOpnd = callInst->getOpnd(targetOpndIndex);
    Opnd::RuntimeInfo* ri = targetOpnd->getRuntimeInfo();
    Opnd::RuntimeInfo::Kind rt_kind = Opnd::RuntimeInfo::Kind_Null;
    if (ri != NULL) {
        rt_kind = ri->getKind();
    }

    if (Opnd::RuntimeInfo::Kind_HelperAddress == rt_kind) {
        return handleInst_HelperCall(inst, ri);
    }
    return Changed_Nothing;
}

PeepHoleOpt::Changed PeepHoleOpt::handleInst_HelperCall(
    Inst* inst, 
    const Opnd::RuntimeInfo* ri)
{
    assert(Opnd::RuntimeInfo::Kind_HelperAddress == ri->getKind());
    void* rt_data = ri->getValue(0);
    POINTER_SIZE_INT helperId = (POINTER_SIZE_INT)rt_data;
    switch(helperId) {
    case CompilationInterface::Helper_ConvStoI32:
    case CompilationInterface::Helper_ConvDtoI32:
        return handleInst_Convert_F2I_D2I(inst);
    default:
        break;
    }
    return Changed_Nothing;
}

PeepHoleOpt::Changed PeepHoleOpt::handleInst_Convert_F2I_D2I(Inst* inst)
{
    //
    // Inline 'int_value = (int)(float_value or double_value)'
    //
    Opnd* dst = inst->getOpnd(0);
    Opnd* src = inst->getOpnd(2);
    Type* srcType = src->getType();
    assert(srcType->isSingle() || srcType->isDouble());
    assert(dst->getType()->isInt4());
    const bool is_dbl = srcType->isDouble();
    // Here, we might have to deal with 3 cases with src (_value):
    // 1. Unassigned operand - act as if were operating with XMM
    // 2. Assigned to FPU - convert to FPU operations, to 
    //    avoid long FPU->mem->XMM chain
    // 3. Assigned to XMM - see #1
    const bool xmm_way = 
        !(src->hasAssignedPhysicalLocation() && src->isPlacedIn(OpndKind_FPReg));

    if (!xmm_way) {
        //TODO: will add FPU later if measurements show it worths trying
        return Changed_Nothing;
    }
    //
    //
    /*
        movss xmm0, val
        // presuming the corner cases (NaN, overflow) 
        // normally happen rare, do conversion first, 
        // and check for falls later
    -- convertNode
        cvttss2si eax, xmm0
    -- ovfTestNode
        // did overflow happen ?
        cmp eax, 0x80000000 
        jne _done               // no - go return result
    -- testAgainstZeroNode
        // test SRC against zero
        comiss xmm0, [fp_zero]
        // isNaN ? 
        jp _nan     // yes - go load 0
    -- testIfBelowNode
        // xmm < 0 ?
        jb _done    // yes - go load MIN_INT. EAX already has it - simply return.
    -- loadMaxIntNode 
        // ok. at this point, XMM is positive and > MAX_INT
        // must load MAX_INT which is 0x7fffffff.
        // As EAX has 0x80000000, then simply substract 1
        sub eax, 1
        jmp _done
    -- loadZeroNode
    _nan:
        xor eax, eax
    -- nodeNode
    _done:
        mov result, eax
    }
    */
    Opnd* fpZeroOpnd = getZeroConst(srcType);
    Type* int32type = irManager->getTypeManager().getInt32Type();
    Opnd* oneOpnd = irManager->newImmOpnd(int32type, 1);
    Opnd* intZeroOpnd = getIntZeroConst();

    // 0x8..0 here is not the INT_MIN, but comes from the COMISS 
    // opcode description instead.
    Opnd* minIntOpnd = irManager->newImmOpnd(int32type, 0x80000000);

    newSubGFG();
    Node* entryNode = getSubCfgEntryNode();

    Node* convertNode = newBB();
    Node* ovfTestNode = newBB();
    Node* testAgainstZeroNode = newBB();
    Node* testIfBelowNode = newBB();
    Node* loadMaxIntNode = newBB();
    Node* loadZeroNode = newBB();
    Node* doneNode = newBB();
    //
    // presuming the corner cases (NaN, overflow) 
    // normally happen rare, do conversion first, 
    // and check for falls later
    //
    connectNodes(entryNode, convertNode);
    //
    // convert
    //
    setCurrentNode(convertNode)    ;
    Mnemonic mn_cvt = is_dbl ? Mnemonic_CVTTSD2SI : Mnemonic_CVTTSS2SI;
    /*cvttss2si r32, xmm*/ newInst(mn_cvt, 1, dst, src);
    connectNodeTo(ovfTestNode);
    setCurrentNode(NULL);

    //
    // check whether overflow happened
    //
    setCurrentNode(ovfTestNode);
    /*cmp r32, MIN_INT*/ newInst(Mnemonic_CMP, dst, minIntOpnd);
    /*jne _done       */ newBranch(Mnemonic_JNE, doneNode, testAgainstZeroNode, 0.9, 0.1);
    //
    setCurrentNode(NULL);

    // test SRC against zero
    //
    setCurrentNode(testAgainstZeroNode);
    Mnemonic mn_cmp = is_dbl ? Mnemonic_COMISD : Mnemonic_COMISS;
    /*comiss src, 0.  */ newInst(mn_cmp, src, fpZeroOpnd);
    /*jp _nan:result=0*/ newBranch(Mnemonic_JP, loadZeroNode, testIfBelowNode);
    setCurrentNode(NULL);

    //
    // 
    //
    setCurrentNode(loadZeroNode);
    /*mov r32, 0*/      newInst(Mnemonic_MOV, dst, intZeroOpnd);
    /*jmp _done*/       connectNodeTo(doneNode);
    setCurrentNode(NULL);

    //
    // test if we have a huge negative in SRC
    //
    setCurrentNode(testIfBelowNode);
    /*jb _done:*/       newBranch(Mnemonic_JB, doneNode, loadMaxIntNode);
    setCurrentNode(NULL);
    //
    // 
    //
    setCurrentNode(loadMaxIntNode);
    /* sub dst, 1*/     newInst(Mnemonic_SUB, dst, oneOpnd);
    connectNodeTo(doneNode);
    setCurrentNode(NULL);
    //
    connectNodes(doneNode, getSubCfgReturnNode());
    //
    propagateSubCFG(inst);
    return Changed_Node;
}


PeepHoleOpt::Changed PeepHoleOpt::handleInst_ALU(Inst* inst)
{
    // The normal form is 'OPERATION left opnd, right operand'
    // except for NOT operation.
    const Mnemonic mnemonic = inst->getMnemonic();
    if (mnemonic == Mnemonic_NOT) {
        // No optimizations this time
        return Changed_Nothing;
    }
    
    // Only these mnemonics have the majestic name of ALUs.
    assert(mnemonic == Mnemonic_ADD || mnemonic == Mnemonic_SUB ||
           mnemonic == Mnemonic_OR || mnemonic == Mnemonic_XOR ||
           mnemonic == Mnemonic_AND || 
           mnemonic == Mnemonic_CMP || mnemonic == Mnemonic_TEST);

    // Only process simple variants: ALU opcodes that either define flags 
    //and use 2 operands, or simply use 2 operands
    unsigned leftIndex = 0;
    if (isReg(inst->getOpnd(leftIndex), RegName_EFLAGS)) {
        ++leftIndex;
    }
    
    const unsigned rightIndex = leftIndex + 1;
    
    Opnd* left = inst->getOpnd(leftIndex);
    Opnd* right = inst->getOpnd(rightIndex);
    
    if (mnemonic != Mnemonic_TEST && 
        isReg(left) && isImm32(right) && fitsImm8(right)) {
        /* what: OPERATION reg, imm32 => OPERATION reg, imm8
           why: shorter instruction
           nb: applicable for all ALUs, but TEST
        */
        right = convertImmToImm8(right);
        replaceOpnd(inst, rightIndex, right);
        return Changed_Opnd;
    }

    return Changed_Nothing;
}

PeepHoleOpt::Changed PeepHoleOpt::handleInst_SSEMov(Inst* inst)
{
    assert(inst->getMnemonic() == Mnemonic_MOVSS || 
           inst->getMnemonic() == Mnemonic_MOVSD);
           
    const bool isDouble = inst->getMnemonic() == Mnemonic_MOVSD;

    if (inst->getOpndCount() != 2) {
        // Expected only MOVSS/SD a, b
        assert(false);
        return Changed_Nothing;
    }
    Opnd* dst = inst->getOpnd(0);
    Opnd* src = inst->getOpnd(1);
    //
    //
    if (isReg(dst) && equals(src, dst)) {
        // what: same register moved around
        // why:  useless thing
        removeInst(inst);
        return Changed_Inst;
    }
    
    //
    //
    if (isReg(dst) && isMem(src)) {
        /* what: MOVSS/MOVSD xmmreg, [zero constant from memory] => PXOR xmmreg, xmmreg
           why:  shorter instruction; no memory access => faster
           nb:   only works with 64 XMMs
        */
        bool isZeroConstant = false;
        if (isDouble) {
            isZeroConstant = isFPConst(src, (double)0);
        }
        else {
            isZeroConstant = isFPConst(src, (float)0);
        }
        
        if (isZeroConstant) {
            // PXOR only accepts double registers, convert dst
            dst = convertToXmmReg64(dst);
            Inst* ii = irManager->newInst(Mnemonic_PXOR, dst, dst);
            replaceInst(inst, ii);
            return Changed_Inst;
        }
        //
        // fall through to process more
        // ||
        // vv
        
    }   // ~ movss xmm, 0 => pxor xmm,xmm
    
    if (isReg(dst) && isReg(src)) {
        /*what: MOVSS/MOVSD reg, reg => MOVQ reg, reg
          why: MOVSD has latency=6, MOVSS has latency=4, MOVQ's latency=2
          nb: MOVQ only works with 64 xmms
        */
        dst = convertToXmmReg64(dst);
        src = convertToXmmReg64(src);
        Inst* ii = irManager->newInst(Mnemonic_MOVQ, dst, src);
        replaceInst(inst, ii);
        return Changed_Inst;
    }
    
    // We just handled 'both regs' case above, the only possible variant:
    assert((isReg(dst)&&isMem(src)) || (isReg(src)&&isMem(dst)));
    if (false && isDouble) {
        //FIXME: MOVQ with memory gets encoded badly - need to fix in encoder
        /*
        what: MOVSD => MOVQ
        why:  faster (? actually, I hope so. Need to double check)
        nb:   only for xmm64
        */
        Inst* ii = irManager->newInst(Mnemonic_MOVQ, dst, src);
        replaceInst(inst, ii);
        return Changed_Inst;
    }
    
    return Changed_Nothing;
}

PeepHoleOpt::Changed PeepHoleOpt::handleInst_SSEXor(Inst* inst)
{
    assert(inst->getMnemonic() == Mnemonic_XORPS || 
           inst->getMnemonic() == Mnemonic_XORPD);
           
    if (inst->getOpndCount() != 2) {
        // Expected only XORPS/PD a, b
        assert(false);
        return Changed_Nothing;
    }
    
    Opnd* dst = inst->getOpnd(0);
    Opnd* src = inst->getOpnd(1);
    
    if (isReg(dst) && isReg(src, dst->getRegName())) {
        /*what: XORPS/XORPD regN, regN => PXOR regN, regN
          why: XORPS/PD used for zero-ing register, but PXOR is faster 
               (2 ticks on PXOR vs 4 ticks for XORPS/XORPD)
        */
        dst = convertToXmmReg64(dst);
        src = convertToXmmReg64(src);
        Inst* ii = irManager->newInst(Mnemonic_PXOR, dst, src);
        replaceInst(inst, ii);
        return Changed_Inst;
    }
    return Changed_Nothing;
}

}}; // ~namespace Jitrino::Ia32
