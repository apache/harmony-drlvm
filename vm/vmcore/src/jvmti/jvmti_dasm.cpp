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
 * @author Alexander V. Astapchuk
 * @version $Revision: $
 */
/**
 * @file
 * @brief Disassembler for JVMTI implementation.
 */
#include "jvmti_dasm.h"
#include "dec_base.h"

void InstructionDisassembler::disasm(const NativeCodePtr addr, 
                                     InstructionDisassembler * pidi)
{
    assert(addr);
    assert(pidi != NULL);
    Inst inst;
    pidi->len = DecoderBase::decode(addr, &inst);
    if (pidi->len == 0) {
        // Something wrong happened
        pidi->type = OPCODEERROR;
        return;
    }
    
    pidi->target = (NativeCodePtr)inst.direct_addr;
    pidi->type = UNKNOWN;
    
    if (inst.mn == Mnemonic_CALL && 
        inst.odesc->opnds[0].kind == OpndKind_Imm) {
        pidi->type = RELATIVE_CALL;
    }
    else if (inst.mn == Mnemonic_JMP &&
             inst.odesc->opnds[0].kind == OpndKind_Imm) {
        pidi->type = RELATIVE_JUMP;
    }
    else if (is_jcc(inst.mn)) {
        // relative Jcc is the only possible variant
        assert(inst.odesc->opnds[0].kind == OpndKind_Imm);
        pidi->cond_jump_type = (CondJumpType)(inst.mn-Mnemonic_Jcc);
        assert(pidi->cond_jump_type < CondJumpType_Count);
        pidi->type = RELATIVE_COND_JUMP;
    }
}
