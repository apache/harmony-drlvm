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
 * @author Alexander V. Astapchuk
 * @version $Revision: $
 */
 
/**
 * @file
 * @brief Main decoding (disassembling) routines implementation.
 */

#include "dec_base.h"

bool DecoderBase::is_prefix(const unsigned char * bytes)
{
    unsigned char b0 = *bytes;
    unsigned char b1 = *(bytes+1);
    if (b0 == 0xF0) { // LOCK
        return true;
    }
    if (b0==0xF2 || b0==0xF3) { // REPNZ/REPZ prefixes
        if (b1 == 0x0F) {   // .... but may be a part of SIMD opcode
            return false;
        }
        return true;
    }
    if (b0 == 0x2E || b0 == 0x36 || b0==0x3E || b0==0x26 || b0==0x64 || b0==0x3E) {
        // branch hints, segment prefixes
        return true;
    }
    if (b0==0x66) { // operand-size prefix
        if (b1 == 0x0F) {   // .... but may be a part of SIMD opcode
            return false;
        }
        return false; //XXX - currently considered as part of opcode//true;
    }
    if (b0==0x67) { // address size prefix
        return true;
    }
    return false;
}


unsigned DecoderBase::decode(const void * addr, Inst * pinst)
{
    Inst tmp;

    //assert( *(unsigned char*)addr != 0x66);
    
    const unsigned char * bytes = (unsigned char*)addr;
    // Check prefix first
    for (unsigned i=0; i<4; i++) {
        if (!is_prefix(bytes)) {
            break;
        }
        ++bytes;
    }

    if (is_prefix(bytes)) {
        // More than 4 prefixes together ?
        assert(false);
        return 0;
    }
    
    // Load up to 4 prefixes
    // for each Mnemonic
    //  for each opcodedesc
    //      if (raw_len == 0) memcmp(, raw_len)
    //  else check the mixed state which is one of the following:
    //      /digit /i /rw /rd /rb
    
    bool found = false;
    const unsigned char * saveBytes = bytes;
    for (unsigned mn=1; mn<Mnemonic_Count; mn++) {
        bytes = saveBytes;
        found=try_mn((Mnemonic)mn, &bytes, &tmp);
        if (found) {
            tmp.mn = (Mnemonic)mn;
            break;
        }
    }
    if (!found) {
        assert(false);
        return 0;
    }
    tmp.size = (unsigned)(bytes-(const unsigned char*)addr);
    if (pinst) {
        *pinst = tmp;
    }
    return tmp.size;
}

bool DecoderBase::decode_aux(const EncoderBase::OpcodeDesc& odesc, unsigned aux,
             const unsigned char ** pbuf, Inst * pinst) 
{
    OpcodeByteKind kind = (OpcodeByteKind)(aux & OpcodeByteKind_KindMask);
    unsigned byte = (aux & OpcodeByteKind_OpcodeMask);
    unsigned data_byte = **pbuf;
    EncoderBase::Operand& opnd = pinst->operands[pinst->argc];
    const EncoderBase::OpndDesc& opndDesc = odesc.opnds[pinst->argc];
    
    switch (kind) {
    case OpcodeByteKind_SlashR:
        //decodeModRM(odesc, pbuf, pinst);
        {
        const ModRM& modrm = *(ModRM*)*pbuf;
        unsigned regIndex = odesc.opnds[0].kind == OpndKind_GPReg ? 0 : 1;
        RegName reg = getRegName(OpndKind_GPReg, opndDesc.size, modrm.reg);
        EncoderBase::Operand& regOpnd = pinst->operands[regIndex];
        if (regIndex == 0) {
            regOpnd = EncoderBase::Operand(reg);
            ++pinst->argc;
            decodeModRM(odesc, pbuf, pinst);
        }
        else {
            decodeModRM(odesc, pbuf, pinst);
            ++pinst->argc;
            regOpnd = EncoderBase::Operand(reg);
        }
        ++pinst->argc;
        }
        return true;
    case OpcodeByteKind_rb:
    case OpcodeByteKind_rw:
    case OpcodeByteKind_rd:
        {
            unsigned regid = data_byte - byte;
            if (regid>7) {
                return false;
            }
            ++*pbuf;
            return true;
        }
    case OpcodeByteKind_cb:
        {
        char offset = *(char*)*pbuf;
        *pbuf += 1;
        opnd = EncoderBase::Operand(offset);
        ++pinst->argc;
        //pinst->direct_addr = (void*)(pinst->offset + *pbuf);
        }
        return true;
    case OpcodeByteKind_cw:
        assert(false); // not an error, but not expected in current env
        break;
    case OpcodeByteKind_cd:
        {
        int offset = *(int*)*pbuf;
        *pbuf += 4;
        opnd = EncoderBase::Operand(offset);
        ++pinst->argc;
        }
        return true;
    case OpcodeByteKind_SlashNum:
        {
        const ModRM& modrm = *(ModRM*)*pbuf;
        if (modrm.reg != byte) {
            return false;
        }
        decodeModRM(odesc, pbuf, pinst);
        ++pinst->argc;
        }
        return true;
    case OpcodeByteKind_ib:
        {
        char ival = *(char*)*pbuf;
        opnd = EncoderBase::Operand(ival);
        ++pinst->argc;
        *pbuf += 1;
        }        
        return true;
    case OpcodeByteKind_iw:
        {
        short ival = *(short*)*pbuf;
        opnd = EncoderBase::Operand(ival);
        ++pinst->argc;
        *pbuf += 2;
        }        
        return true;
    case OpcodeByteKind_id:
        {
        int ival = *(int*)*pbuf;
        opnd = EncoderBase::Operand(ival);
        ++pinst->argc;
        *pbuf += 4;
        }        
        return true;
#ifdef _EM64T_
    case OpcodeByteKind_io:
        {
        long long int ival = *(long long int*)*pbuf;
        opnd = EncoderBase::Operand(OpndSize_64, ival);
        ++pinst->argc;
        *pbuf += 8;
        }        
        return true;
#endif
    case OpcodeByteKind_plus_i:
        {
            unsigned regid = data_byte - byte;
            if (regid>7) {
                return false;
            }
            ++*pbuf;
            return true;
        }
    case OpcodeByteKind_ZeroOpcodeByte: // cant be here
        assert(false);
        break;
    default:
        // unknown kind ? how comes ?
        assert(false);
        break;
    }
    return false;
}

bool DecoderBase::try_mn(Mnemonic mn, const unsigned char ** pbuf, Inst * pinst) {
    const unsigned char * save_pbuf = *pbuf;
    EncoderBase::OpcodeDesc * opcodes = EncoderBase::opcodes[mn];
    for (unsigned i=0; !opcodes[i].last; i++) {
        const EncoderBase::OpcodeDesc& odesc = opcodes[i];
        *pbuf = save_pbuf;
        if (odesc.opcode_len != 0) {
            if (memcmp(*pbuf, odesc.opcode, odesc.opcode_len)) {
                continue;
            }
            *pbuf += odesc.opcode_len;
        }
        if (odesc.aux0 != 0) {
            
            if (!decode_aux(odesc, odesc.aux0, pbuf, pinst)) {
                continue;
            }
            if (odesc.aux1 != 0) {
                if (!decode_aux(odesc, odesc.aux1, pbuf, pinst)) {
                    continue;
                }
            }
            pinst->odesc = &opcodes[i];
            return true;
        }
        else {
            // Can't have empty opcode
            assert(odesc.opcode_len != 0);
            pinst->odesc = &opcodes[i];
            return true;
        }
    }
    return false;
}

#ifdef _IA32_
#define DISASM_REG_SIZE OpndSize_32
#else
#define DISASM_REG_SIZE OpndSize_64
#endif

bool DecoderBase::decodeModRM(const EncoderBase::OpcodeDesc& odesc,
                              const unsigned char ** pbuf, Inst * pinst)
{
    EncoderBase::Operand& opnd = pinst->operands[pinst->argc];
    const EncoderBase::OpndDesc& opndDesc = odesc.opnds[pinst->argc];
    
    //XXX debug ///assert(0x66 != *(*pbuf-2));
    const ModRM& modrm = *(ModRM*)*pbuf;
    *pbuf += 1;
    
    RegName reg = RegName_Null;
    RegName base = RegName_Null;
    RegName index = RegName_Null;
    int disp = 0;
    unsigned scale = 0; 
    //XXX
    scale=scale; base = base; index = index; disp = disp; reg = reg;

    reg = getRegName(OpndKind_GPReg, DISASM_REG_SIZE, modrm.reg);
    if (modrm.mod == 3) {
        // we have only modrm. no sib, no disp.
        reg = getRegName(OpndKind_GPReg, opndDesc.size, modrm.rm);
        opnd = EncoderBase::Operand(reg);
        return true;
    }
    const SIB& sib = *(SIB*)*pbuf;
    // check whether we have a sib
    if (modrm.rm == 4) {
        // yes, we have SIB
        *pbuf += 1;
        scale = sib.scale == 0 ? 0 : (1<<sib.scale);
        if (sib.index == 4) {
            // no index
        }
        else {
            index = getRegName(OpndKind_GPReg, DISASM_REG_SIZE, sib.index);
        }
        base = getRegName(OpndKind_GPReg, DISASM_REG_SIZE, sib.base);
    }
    else {
        if (modrm.mod != 0 || modrm.rm != 5) {
            base = getRegName(OpndKind_GPReg, DISASM_REG_SIZE, modrm.rm);
        }
        else {
            // mod=0 && rm == 5 => only disp32
        }
    }
    
    if (modrm.mod == 2) {
        // have disp32 
        disp = *(int*)*pbuf;
        *pbuf += 4;
    }
    else if (modrm.mod == 1) {
        // have disp8 
        disp = *(char*)*pbuf;
        *pbuf += 1;
    }
    else {
        assert(modrm.mod == 0);
        if (modrm.rm == 5) {
            // have disp32 w/o sib
            disp = *(int*)*pbuf;
            *pbuf += 4;
        }
        else if (modrm.rm == 4 && sib.base == 5) {
            // have to analyze sib, special case without EBP: have disp32+SI
            *pbuf += 4;
        }
    }
    opnd = EncoderBase::Operand(opndDesc.size, base, index, scale, disp);
    return true;
}

