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
 * @version $Revision: 1.10.12.1.4.3 $
 */

#include "Ia32Encoder.h"
#include "Ia32Inst.h"

namespace Jitrino {
namespace Ia32 {

const Encoder::OpcodeGroupDescription Encoder::dummyOpcodeGroupDescription;

const Encoder::MemOpndConstraints Encoder::memOpndConstraints[16]= {
    {{
    Constraint(OpndKind_GPReg, OpndSize_32), 
    Constraint(OpndKind_GPReg, OpndSize_32),
    Constraint(OpndKind_Imm, OpndSize_32),
    Constraint(OpndKind_Imm, OpndSize_32) }}, 
    // others contain null constraints, to be fixed later
};

Constraint Encoder::getAllRegs(OpndKind regKind)
{
    switch(regKind) {
    case OpndKind_GPReg:
        return Constraint(OpndKind_GPReg, 
                          Constraint::getDefaultSize(OpndKind_GPReg), 0xff);
    case OpndKind_XMMReg:
        return Constraint(OpndKind_XMMReg,
                          Constraint::getDefaultSize(OpndKind_XMMReg), 0xff);
    case OpndKind_FPReg:
        return Constraint(OpndKind_FPReg,
                          Constraint::getDefaultSize(OpndKind_FPReg), 0x1);
    case OpndKind_StatusReg:
        return Constraint(OpndKind_StatusReg,
                        Constraint::getDefaultSize(OpndKind_StatusReg), 0x1);
    default: 
        break;
    }
    return Constraint();
}

uint32 Encoder::getMnemonicProperties(Mnemonic mn)
{
    const MnemonicDesc * mdesc = getMnemonicDesc(mn);
    return (mdesc->conditional ? 
            Inst::Properties_Conditional:0) | 
            (mdesc->symmetric ? Inst::Properties_Symmetric : 0);
};

//_________________________________________________________________________________________________
bool Encoder::matches(Constraint co, Constraint ci, uint32 opndRoles,
                      bool allowAliases)
{
    return co.isNull() || !(ci&co).isNull() || 
        (allowAliases && !(opndRoles&Inst::OpndRole_Def) &&
        !(ci.getAliasConstraint(co.getSize())&co).isNull());
}

//_________________________________________________________________________________________________
const Encoder::OpcodeGroupDescription * 
Encoder::findOpcodeGroupDescription(const FindInfo& fi)
{
    Mnemonic m = fi.mnemonic;
    assert(m != Mnemonic_Null && m < Mnemonic_Count);
    const OpcodeGroupsHolder& mi = getOpcodeGroups()[m];
    // first, find better matching for already assigned operands
    for (uint32 i=0; i<mi.count; i++) {
        const OpcodeGroupDescription * ogd=mi.groups+i;
        if (matches(ogd, fi, false)) {
            return ogd;
        }
    }

    // now find any matching suitable for the type constraint (initial)
    for (uint32 i=0; i<mi.count; i++) {
        const OpcodeGroupDescription * ogd=mi.groups+i;
        if (matches(ogd, fi, true)) {
            return ogd;
        }
    }
    return NULL;
}

//_________________________________________________________________________________________________
bool Encoder::matches(const OpcodeGroupDescription * ogd, const FindInfo& fi,
                      bool any)
{
    if (fi.isExtended) {
        if (fi.defOpndCount != ogd->opndRoles.defCount || 
            fi.opndCount != ogd->opndRoles.defCount+ogd->opndRoles.useCount) {
            return false;
        }
    }
    else {
        if (fi.opndCount != ogd->opndRoles.count) {
            return false;
        }
    }
    for (uint32 i = 0, n = fi.opndCount; i < n; i++) {
        uint32 idx = fi.isExtended ? ogd->extendedToNativeMap[i] : i;
        assert(idx<IRMaxNativeOpnds);
        Constraint ci=ogd->weakOpndConstraints[idx];
        Constraint co=fi.opndConstraints[idx];
        if (any) {
            co = Constraint(OpndKind_Any, co.getSize());
        }
        if (!matches(co, ci, Encoder::getOpndRoles(ogd->opndRoles,idx), any)) {
            return false;
        }
    }
    return true;
}

//_________________________________________________________________________________________________
bool Encoder::matches(const OpcodeDescription * od, const FindInfo& fi)
{
    const uint32 * extendedToNativeMap = fi.isExtended ? 
                        fi.opcodeGroupDescription->extendedToNativeMap : NULL;
    for (uint32 i = 0, n = fi.opndCount; i < n; i++) {
        uint32 idx = extendedToNativeMap != NULL ? extendedToNativeMap[i] : i;
        assert(idx<IRMaxNativeOpnds);
        Constraint ci = od->opndConstraints[idx];
        Constraint co = fi.opndConstraints[i];
        if (!matches(co, ci, Encoder::getOpndRoles(od->opndRoles,idx), true)) {
            return false;
        }
    }
    return true;
}

//_________________________________________________________________________________________________
uint32 Encoder::findOpcodeDescription(const FindInfo& fi)
{
    const OpcodeGroupDescription * ogd = fi.opcodeGroupDescription;
    assert(ogd!=NULL);
    for (uint32 i=0, n=ogd->opcodeDescriptionCount; i<n; i++) {
        if (matches(ogd->opcodeDescriptions + i, fi)) {
            return i;
        }
    }
    return EmptyUint32;
}

//_________________________________________________________________________________________________
uint8 * Encoder::emit(uint8* stream, const Inst * inst)
{
    FindInfo fi;
    inst->initFindInfo(fi, Opnd::ConstraintKind_Current);
    uint32 opcodeIdx=Encoder::findOpcodeDescription(fi);
    assert(opcodeIdx!=EmptyUint32);
    return Encoder::emit(stream, inst,
                inst->opcodeGroupDescription->opcodeDescriptions+opcodeIdx);
}

//_________________________________________________________________________________________________
uint8* Encoder::emit(uint8* stream, const Inst * inst,
                     const OpcodeDescription * desc)
{
    Mnemonic mnemonic=inst->getMnemonic();
    EncoderBase::Operands args;
    Opnd * const * opnds = inst->getOpnds();
    const uint32 * roles = inst->getOpndRoles();
    for( int idx=0, n=inst->getOpndCount(); idx<n; idx++ ) { 
        if (!(roles[idx] & Inst::OpndRole_Explicit)) continue;
        const Opnd * p = opnds[idx];
        Constraint c = p->getConstraint(Opnd::ConstraintKind_Location);
        // A workaround: there are many cases when the size specified in
        // the inst's operands does not match the real operands' sizes.
        // I.e. there are number of 'MOVZX reg32, reg32' instructions as well
        // as 'SETcc r/m32'.
        // This causes a huge problem in opcode selection:
        // first, such a mess blocks the fast opcode selection and makes it 
        // fall to slow path. For the particular 'MOVZX reg32, reg32' even 
        // the slow path unable to find the proper opcode without a huge 
        // complication of selection algorithm.
        // Thus, trying to fix the inconsistency right here: use the size
        // specified in the opcodeGroupDescriptions, of instruction and not 
        // the size specified in the operand.

        OpndSize sz = inst->opcodeGroupDescription->opcodeDescriptions[0].
                                    opndConstraints[args.count()].getSize();
        // ^^ args.count()  serves as a sequental number of current operand.

        switch( c.getKind() ) {
        case OpndKind_Imm:
            args.add(EncoderBase::Operand(sz, p->getImmValue()));
            break;
        case OpndKind_Mem:
            {
                const Opnd * pbase = 
                               p->getMemOpndSubOpnd(MemOpndSubOpndKind_Base);
                const Opnd * pindex =
                              p->getMemOpndSubOpnd(MemOpndSubOpndKind_Index);
                const Opnd * pscale =
                              p->getMemOpndSubOpnd(MemOpndSubOpndKind_Scale);
                const Opnd * pdisp =
                       p->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);

                int disp = pdisp ? (int)pdisp->getImmValue() : 0 ;
                RegName baseReg = p->getBaseReg();
                if (baseReg == RegName_Null && NULL != pbase) {
                    baseReg = pbase->getRegName();
                }
                // Special processing of FS:[14] - in CFG, they exist as 
                // operands with base=RegName_FS and a displacement.
                // Here, at this stage, we remove RegName_FS from the base
                // in inserts appropriate prefix into the code stream.
                if (baseReg == RegName_FS) {
                    stream = (uint8*)EncoderBase::prefix((char*)stream,
                                                         InstPrefix_FS);
                    baseReg = RegName_Null;
                }
                else {
                    // Just a check that no other segment register is used 
                    // the same way. If something changes, then this 
                    // new usage scheme must be processed in a special way.
                    assert(baseReg == RegName_Null || 
                           getRegKind(baseReg) != OpndKind_SReg);
                }
                if (p->getMemOpndKind() == MemOpndKind_StackAutoLayout) {
                    disp += inst->getStackDepth();
                    if (Mnemonic_POP == mnemonic) {
                        disp -= 4;
                    }
                }

                EncoderBase::Operand o(sz, 
                    baseReg,
                    NULL == pindex ? RegName_Null : pindex->getRegName(),
                    NULL == pscale ? 0 : (unsigned char)pscale->getImmValue(),
                    disp
                    );
                args.add( o );
            }
            break;
        default:
            {
            assert(RegName_Null != p->getRegName());
            RegName opndReg = p->getRegName();
            // find an aliased register, with the same index and kind, but
            // with different size
            RegName reg = getRegName(getRegKind(opndReg), sz, 
                                     getRegIndex(opndReg));
            args.add(EncoderBase::Operand(reg));
            }
            break;
        }
    }
    return (uint8*)EncoderBase::encode((char*)stream, mnemonic, args);
}


const Encoder::OpcodeGroupsHolder * Encoder::getOpcodeGroups(void)
{
    // NOTE: not multi-thread friendly !
    // Having static vars here is not safe for multi-threaded env.
    // However, when Jitrino is working without Jitrino.JET, then there is 
    // *really* little chance that we'll get several threads here at the 
    // very beginning - when we just compiling java/lang/Thread::<clinit>.
    // When we're working with Jitrino.JET on leading edge, then we're 
    // getting recompilation requests from a single thread anyway.
    //
    // However, to be really safe (and, may be, to be ready for several 
    // recompilation threads) these statics need to be removed. Currently, 
    // we can not use auto-starting init, as we'll race with EncoderBase's
    // auto-starting init of its tables. Need to change EncoderBase 
    // to avoid direct access to EncoderBase::mnemonic, etc. - TODO, later.
    static OpcodeGroupsHolder opcodeGroups[Mnemonic_Count];
    static bool groupsDone = false;
    if (!groupsDone) {
        memset(opcodeGroups, 0, sizeof opcodeGroups);
        initOpcodeGroups(opcodeGroups);
        groupsDone = true;
    }
    return opcodeGroups;
}

void Encoder::initOpcodeGroups(OpcodeGroupsHolder * table)
{
    for (unsigned mn = Mnemonic_Null; mn < Mnemonic_Count; mn++) {
        buildHolder(table + mn, mnemonics[mn], opcodes[mn]);
    }
}

void Encoder::buildHolder(OpcodeGroupsHolder * mitem, 
                          const MnemonicDesc& mdesc,
                          const OpcodeDesc * opcodes)
{
    // must be called once for each item
    assert(mitem->count == 0);

    /*
    An idea is to split a set of opcodeDescriptions into several groups
    basing on the following criteria:
        1) the same operand properties (like DU_U)
        2) for wich min and max operand constraints for all operands can be 
           described by a single Constraint item per operand
        The consequences are:
        - corresponding operands in a group are of the same size
        - corresponding operands in a group can be assigned to the same 
          register kind
    */
    for (unsigned i=0; !opcodes[i].last; i++) {
        //
        // fill out an OpcodeDesc for each given opcode
        //
        OpcodeDescription od;
        initOD(od, opcodes + i);
        //
        // try to find a group for the opcodeDesc in already filled groups
        //
        unsigned j=0;
        for ( ; j<mitem->count; j++) {
            const OpcodeGroupDescription& ogd = mitem->groups[j];
            // check (1) from above
            if( ogd.opndRoles.roles != od.opndRoles.roles ) continue;
            // check (2) from above
            OpcodeDescription& od0 = mitem->groups[j].opcodeDescriptions[0];
            //
            unsigned  k = 0;
            for (; k<ogd.opndRoles.count; k++) {
                if (!od0.opndConstraints[k].canBeMergedWith(
                                                    od.opndConstraints[k])) {
                    break;
                }
            }
            if (k == ogd.opndRoles.count) {
                break; // found appropriate group
            }
        }
        if (j != mitem->count) {
            // a group found.
            OpcodeGroupDescription& ogd = mitem->groups[j];
            assert(ogd.opcodeDescriptionCount < 
                   COUNTOF(ogd.opcodeDescriptions));
            ogd.opcodeDescriptions[ogd.opcodeDescriptionCount] = od;
            ++ogd.opcodeDescriptionCount;
            continue;
        }
        // no appropriate group found, creating a new one.
        assert(mitem->count < COUNTOF(mitem->groups));
        OpcodeGroupDescription& ogd = mitem->groups[mitem->count];
        initOGD(ogd, mdesc);
        ogd.opndRoles = od.opndRoles;
        ogd.opcodeDescriptions[0] = od;
        ogd.opcodeDescriptionCount = 1;
        ++mitem->count;
    }
    for (unsigned i=0; i<mitem->count; i++) {
        finalizeOGD(mitem->groups[i]);
    }

    if (getBaseConditionMnemonic(mdesc.mn) == Mnemonic_SETcc) {
        // A workaround here:
        // begins in the master encoding table in the record for SETcc 
        // instructions - see also (see enc_tabl.cpp) - there is a fake 
        // record fr 'SETcc rm32' instruction.
        // However, we can not simply leave the instruction as is, as it will
        // try to use the lower parts of EBP/ESI/EDI, which will be mapped 
        // to 'AH/CH/DH/DH' which is definitely not a behavior we need. 
        // Thus a second special case, which is implicit and resides in 
        // getAliasConstraint(): when we reques 
        // getAliasConstraint(OpndSize_32) for 'r_m8' it returns a constraint
        // with mask which allows only lower 4 registers to be used (EAX, 
        // ECX, EDX, EBX) so only 'AL, CL, DL and BL' will be used in real.
        // This magic feature of constraint prevents a tries to use 
        // EBP/ESI/EDI as arguments for that fake 'SETcc' instruction.
        //
        Constraint cRM8 = Constraint(OpndKind_GPReg, OpndSize_8)|
                          Constraint(OpndKind_Mem, OpndSize_8);

        for (unsigned i=0; i<mitem->count; i++) {
            assert(mitem->groups[i].opcodeDescriptionCount == 1);
            OpcodeDescription& od = mitem->groups[i].opcodeDescriptions[0];
            assert( od.opndRoles.count == 1 );
            if (od.opndConstraints[0].getSize() == OpndSize_8) {
                continue;
            }
            // Substitute the 'r/m32' constraint with the aliased 'r/m8'
            od.opndConstraints[0] = cRM8.getAliasConstraint(
                                            od.opndConstraints[0].getSize());
            // Clean initialized weak and strong constraints, so they'll be
            // recomputed properly
            mitem->groups[i].weakOpndConstraints[0] = Constraint();
            mitem->groups[i].strongOpndConstraints[0] = Constraint();
            // Recompute weak and strong constraints
            finalizeOGD(mitem->groups[i]);
        }
    }
}

void Encoder::initOD(OpcodeDescription& od, const OpcodeDesc * opcode)
{
    od.opndRoles = opcode->roles;
    // .. fill the operands infos
    for (unsigned k=0; k<od.opndRoles.count; k++) {
        if (opcode->opnds[k].reg != RegName_Null) {
            // exact Register specified
            od.opndConstraints[k] = Constraint(opcode->opnds[k].reg);
        }
        else {
            od.opndConstraints[k] = Constraint(opcode->opnds[k].kind, 
                opcode->opnds[k].size == OpndSize_Null ? 
                                        OpndSize_Any : opcode->opnds[k].size);
        }
    }
}

void Encoder::initOGD(OpcodeGroupDescription& ogd, const MnemonicDesc& mdesc)
{
    ogd.properties = 
            (mdesc.symmetric ? Inst::Properties_Symmetric : 0) | 
            (mdesc.conditional ? Inst::Properties_Conditional : 0);

    ogd.opcodeDescriptionCount = 0;

    const OpndRolesDescription NOOPNDS = {0, 0, 0, 0 };
    ogd.opndRoles = NOOPNDS;
    //ogd.weakOpndConstraints[IRMaxNativeOpnds];
    //ogd.strongOpndConstraints[IRMaxNativeOpnds];

    ogd.implicitOpndRegNames[0] = RegName_Null;
    ogd.implicitOpndRegNames[1] = RegName_Null;
    ogd.implicitOpndRegNames[2] = RegName_Null;

    if( mdesc.affectsFlags && mdesc.usesFlags ) {
        static const OpndRolesDescription DU = {1, 1, 1, 
                                                OpndRole_Def|OpndRole_Use};
        ogd.implicitOpndRoles = DU;
        ogd.implicitOpndRegNames[0] = RegName_EFLAGS;
    }
    else if( mdesc.affectsFlags ) {
        static const OpndRolesDescription D = {1, 1, 0, OpndRole_Def};
        ogd.implicitOpndRoles = D;
        ogd.implicitOpndRegNames[0] = RegName_EFLAGS;
    }
    else if( mdesc.usesFlags ) {
        static const OpndRolesDescription U = {1, 0, 1, OpndRole_Use};
        ogd.implicitOpndRoles = U;
        ogd.implicitOpndRegNames[0] = RegName_EFLAGS;
    }

    // will be filled in finilizeOGD()
    //for(int i=0; i<COUNTOF(ogd.extendedToNativeMap); 
    //    ogd.extendedToNativeMap[i++] = (unsigned)-1 ) 
    //{};
    //ogd.opcodeDescriptions, already zero-ed, 
    ogd.printMnemonic = mdesc.name;
}

void Encoder::finalizeOGD(OpcodeGroupDescription& ogd)
{
    // calculate weakOpndConstraints - just a union of all constraints from 
    // 'ogd' imposed on an operand
    for (uint32 i=0; i<ogd.opcodeDescriptionCount; i++)
        for (uint32 j=0; j<ogd.opndRoles.count; j++)
            ogd.weakOpndConstraints[j].unionWith(
                                ogd.opcodeDescriptions[i].opndConstraints[j]);


    // calculate strongOpndConstraints 
    // operand satisfying its strongOpndConstraints element will suit to at
    // least one opcode in the group independently of kinds of other operands
    // (although satisfying weakOpndConstraints) in the instruction 
    // i.e. we have a group:
    //      add:
    //          r32|m32,    r32
    //          r32|m32,    imm32
    //          r32,        r32|m32
    // then the strongOpndConstraints will be:
    //      r32,    r32|imm32
    //      r32 - because if the first operand is r32 then an opcode will be
    //            found independently of the second operand (if the latter 
    //            satisfies its weakOpndConstraints) 
    //      r32|imm32 - similarly for such a second operand an opcode will be
    //            found independently of the first operand (if the latter 
    //            satisfies its weakOpndConstraints) 
    // procedure:
    //      to be described later

    Constraint currentWeakConstraints[lengthof(ogd.weakOpndConstraints)];
    for (uint32 i=0; i<ogd.opcodeDescriptionCount; i++) {

        Constraint::CompareResult currentWeakConstraintRelations[
                                           lengthof(ogd.weakOpndConstraints)];

        for (uint32 j=0; j<ogd.opndRoles.count; j++) {
            Constraint & ccj=currentWeakConstraints[j];
            Constraint & cij=ogd.opcodeDescriptions[i].opndConstraints[j];
            currentWeakConstraintRelations[j]=ccj.compare(cij);
            ccj.unionWith(cij);
        }

        for (uint32 j=0; j<ogd.opndRoles.count; j++) {
            Constraint::CompareResult relation=Constraint::CompareResult_Equal;
            for (uint32 k=0; k<ogd.opndRoles.count; k++) {
                if (k==j) {
                    continue;
                }
                if (relation==Constraint::CompareResult_Equal) {
                    relation=currentWeakConstraintRelations[k];
                }
                else if (currentWeakConstraintRelations[k]!=
                         Constraint::CompareResult_Equal && 
                         currentWeakConstraintRelations[k]!=relation) {
                    relation=Constraint::CompareResult_NotEqual;
                    break;
                }
            }

            Constraint & cj=ogd.strongOpndConstraints[j];
            Constraint & cij=ogd.opcodeDescriptions[i].opndConstraints[j];

            if (relation==Constraint::CompareResult_Equal) {
                cj.unionWith(cij);
            }
            else if (relation==Constraint::CompareResult_RightContainsLeft) {
                cj=cij;
            }
            else if (relation==Constraint::CompareResult_NotEqual) {
                cj.intersectWith(cij);
            }
        }
    }

    // initialize extendedToNativeMap
    uint32 etmIdx=0;
    for (uint32 i=0; i<IRMaxNativeOpnds; i++) {
        if ((i < ogd.opndRoles.count) && 
            (getOpndRoles(ogd.opndRoles, i)&Inst::OpndRole_Def)) {
            ogd.extendedToNativeMap[etmIdx++]=i;
        }
    }
    assert(etmIdx==ogd.opndRoles.defCount);
    for (uint32 i=0; i<IRMaxNativeOpnds; i++) {
        if ((i < ogd.opndRoles.count) && 
            (getOpndRoles(ogd.opndRoles, i)&Inst::OpndRole_Use)) {
            ogd.extendedToNativeMap[etmIdx++]=i;
        }
    }
    assert(etmIdx==(uint32)(ogd.opndRoles.defCount+ogd.opndRoles.useCount));
}

}}; //namespace Ia32
