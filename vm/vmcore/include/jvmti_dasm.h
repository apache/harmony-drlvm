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
 * @brief Disassembler for JVMTI interface declaration.
 */
#if !defined(__JVMTI_DASM_H_INCLUDED__)
#define __JVMTI_DASM_H_INCLUDED__

#include "open/types.h"
#include "jni_types.h"
#include <assert.h>

class InstructionDisassembler {
public:

    enum Type {
        OPCODEERROR = 0,
        UNKNOWN,
        OTHER=UNKNOWN,
        RELATIVE_JUMP,
        RELATIVE_COND_JUMP,
        ABSOLUTE_CALL,
        RELATIVE_CALL
    };
    /**
     * @brief Enum of possible condtions for conditional jump-s.
     *
     * @note To simplify decoding, the current order of the enum constants 
     *       exactly matches the order of ConditionalMnemonic constants
     *       in #EncoderBase. Reordering of this constants requires some 
     *       changes in InstructionDisassembler::disasm.
     */
    enum CondJumpType
    {
        JUMP_OVERFLOW=0,
        JUMP_NOT_OVERFLOW=1,
        JUMP_BELOW=2, JUMP_NOT_ABOVE_OR_EQUAL=JUMP_BELOW, JUMP_CARRY=JUMP_BELOW,
        JUMP_NOT_BELOW=3, JUMP_ABOVE_OR_EQUAL=JUMP_NOT_BELOW, JUMP_NOT_CARRY=JUMP_NOT_BELOW,
        JUMP_ZERO=4, JUMP_EQUAL=JUMP_ZERO,
        JUMP_NOT_ZERO=5, JUMP_NOT_EQUAL=JUMP_NOT_ZERO,
        JUMP_BELOW_OR_EQUAL=6, JUMP_NOT_ABOVE=JUMP_BELOW_OR_EQUAL,
        JUMP_NOT_BELOW_OR_EQUAL=7, JUMP_ABOVE=JUMP_NOT_BELOW_OR_EQUAL,

        JUMP_SIGN=8,
        JUMP_NOT_SIGN=9,
        JUMP_PARITY=10, JUMP_PARITY_EVEN=JUMP_PARITY,
        JUMP_NOT_PARITY=11, JUMP_PARITY_ODD=JUMP_NOT_PARITY,
        JUMP_LESS=12, JUMP_NOT_GREATER_OR_EQUAL=JUMP_LESS,
        JUMP_NOT_LESS=13, JUMP_GREATER_OR_EQUAL=JUMP_NOT_LESS,
        JUMP_LESS_OR_EQUAL=14, JUMP_NOT_GREATER=JUMP_LESS_OR_EQUAL,
        JUMP_NOT_LESS_OR_EQUAL=15, JUMP_GREATER=JUMP_NOT_LESS_OR_EQUAL,

        CondJumpType_Count = 16
    };

    
    InstructionDisassembler(NativeCodePtr address) :
        type(OPCODEERROR), target(0), len(0), cond_jump_type(JUMP_OVERFLOW)
    {
        disasm(address, this);
    }

    InstructionDisassembler(InstructionDisassembler &d)
    {
        type = d.type;
        target = d.target;
        len = d.len;
        cond_jump_type = d.cond_jump_type;
    }

    /**
     * @brief Returns type of underlying instruction.
     */
    Type get_type(void) const
    {
        return type;
    }
    /**
     * @brief Returns length (in bytes) of underlying instruction.
     * 
     * The size includes instruction's prefixes, if any.
     */
    jint get_length_with_prefix(void) const
    {
        assert(type != OPCODEERROR);
        return len;
    }
    /**
     * @brief Returns absolute address of target, if applicable.
     *
     * For instructions other than relative JMP, CALL and conditional jumps, 
     * the value is undefined.
     */
    NativeCodePtr get_jump_target_address(void) const
    {
        assert(type == RELATIVE_JUMP || type == RELATIVE_COND_JUMP ||
            type == ABSOLUTE_CALL || type == RELATIVE_CALL);
        return target;
    }
    /**
     * @brief Returns type of conditional jump.
     * 
     * @note For instructions other than conditional jump, the value is
     *       undefined.
     */
    CondJumpType get_cond_jump_type(void) const
    {
        assert(type == RELATIVE_COND_JUMP);
        return cond_jump_type;
    }

private:
    /**
     * @brief Performs disassembling, fills out InstructionDisassembler's 
     *        fields.
     *
     * If it's impossible (for any reason) to decode an instruction, then 
     * type is set to OPCODEERROR and other fields' values are undefined.
     */
    static void disasm(const NativeCodePtr addr, 
                       InstructionDisassembler * pidi);
    /**
     * @brief Type of instruction.
     */
    Type    type;
    /**
     * @brief Absolute address of target, if applicable.
     */
    NativeCodePtr   target;
    /**
     * @brief Length of the instruction, in bytes.
     */
    unsigned len;
    /**
     * @brief Type of conditional jump.
     */
    CondJumpType cond_jump_type;
};


#endif  // __JVMTI_DASM_H_INCLUDED__
