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
 * @author Pavel Rebriy
 * @version $Revision: 1.1.2.3.4.4 $
 */

#include <limits.h>
#include "ver_real.h"

/**
 * Debug flag macros
 */
// Macro sets verification only for defined class
#define VF_CLASS           0
// Macro sets verification only for defined method
#define VF_METHOD          0

/**
 * Parses bytecode, determines instruction boundaries and 
 * provides checks of simple verifications.
 */
static vf_Result vf_parse_bytecode(vf_Context *ctx);    // verification context

#if _VF_DEBUG
/**
 * Prints code instruction array in stream.
 */
void vf_dump_bytecode(vf_ContextHandle);        // verification context

/**
 * Array of opcode names. Available in debug mode.
 */
const char *vf_opcode_names[256] = {
    "NOP", "ACONST_NULL", "ICONST_M1", "ICONST_0", "ICONST_1", "ICONST_2",
    "ICONST_3", "ICONST_4", "ICONST_5", "LCONST_0", "LCONST_1", "FCONST_0",
    "FCONST_1", "FCONST_2", "DCONST_0", "DCONST_1", "BIPUSH", "SIPUSH",
    "LDC", "LDC_W", "LDC2_W", "ILOAD", "LLOAD", "FLOAD", "DLOAD", "ALOAD",
    "ILOAD_0", "ILOAD_1", "ILOAD_2", "ILOAD_3", "LLOAD_0", "LLOAD_1",
    "LLOAD_2",
    "LLOAD_3", "FLOAD_0", "FLOAD_1", "FLOAD_2", "FLOAD_3", "DLOAD_0",
    "DLOAD_1",
    "DLOAD_2", "DLOAD_3", "ALOAD_0", "ALOAD_1", "ALOAD_2", "ALOAD_3",
    "IALOAD",
    "LALOAD", "FALOAD", "DALOAD", "AALOAD", "BALOAD", "CALOAD", "SALOAD",
    "ISTORE", "LSTORE", "FSTORE", "DSTORE", "ASTORE", "ISTORE_0", "ISTORE_1",
    "ISTORE_2", "ISTORE_3", "LSTORE_0", "LSTORE_1", "LSTORE_2", "LSTORE_3",
    "FSTORE_0", "FSTORE_1", "FSTORE_2", "FSTORE_3", "DSTORE_0", "DSTORE_1",
    "DSTORE_2", "DSTORE_3", "ASTORE_0", "ASTORE_1", "ASTORE_2", "ASTORE_3",
    "IASTORE", "LASTORE", "FASTORE", "DASTORE", "AASTORE", "BASTORE",
    "CASTORE",
    "SASTORE", "POP", "POP2", "DUP", "DUP_X1", "DUP_X2", "DUP2", "DUP2_X1",
    "DUP2_X2", "SWAP", "IADD", "LADD", "FADD", "DADD", "ISUB", "LSUB", "FSUB",
    "DSUB", "IMUL", "LMUL", "FMUL", "DMUL", "IDIV", "LDIV", "FDIV", "DDIV",
    "IREM", "LREM", "FREM", "DREM", "INEG", "LNEG", "FNEG", "DNEG", "ISHL",
    "LSHL", "ISHR", "LSHR", "IUSHR", "LUSHR", "IAND", "LAND", "IOR", "LOR",
    "IXOR", "LXOR", "IINC", "I2L", "I2F", "I2D", "L2I", "L2F", "L2D", "F2I",
    "F2L", "F2D", "D2I", "D2L", "D2F", "I2B", "I2C", "I2S", "LCMP", "FCMPL",
    "FCMPG", "DCMPL", "DCMPG", "IFEQ", "IFNE", "IFLT", "IFGE", "IFGT", "IFLE",
    "IF_ICMPEQ", "IF_ICMPNE", "IF_ICMPLT", "IF_ICMPGE", "IF_ICMPGT",
    "IF_ICMPLE", "IF_ACMPEQ", "IF_ACMPNE", "GOTO", "JSR", "RET",
    "TABLESWITCH",
    "LOOKUPSWITCH", "IRETURN", "LRETURN", "FRETURN", "DRETURN", "ARETURN",
    "RETURN", "GETSTATIC", "PUTSTATIC", "GETFIELD", "PUTFIELD",
    "INVOKEVIRTUAL",
    "INVOKESPECIAL", "INVOKESTATIC", "INVOKEINTERFACE", "_OPCODE_UNDEFINED",
    "NEW", "NEWARRAY", "ANEWARRAY", "ARRAYLENGTH", "ATHROW", "CHECKCAST",
    "INSTANCEOF", "MONITORENTER", "MONITOREXIT", "WIDE", "MULTIANEWARRAY",
    "IFNULL", "IFNONNULL", "GOTO_W", "JSR_W",
};
#endif // _VF_DEBUG

/** 
 * Provides method bytecode verifications.
 */
static vf_Result vf_verify_method_bytecode(vf_Context *ctx)     // verification context
{
    VF_TRACE("method", VF_REPORT_CTX(ctx) << "verifying method");

    // set method for type pool
    ctx->m_type->SetMethod(ctx);

    // parse method descriptor
    vf_parse_description(ctx->m_descriptor, &ctx->m_method_inlen,
                         &ctx->m_method_outlen);
    vf_set_description_vector(ctx->m_descriptor, ctx->m_method_inlen, 0,
                              ctx->m_method_outlen, &ctx->m_method_invector,
                              &ctx->m_method_outvector, ctx);

    // parse bytecode, fill instruction instr
    vf_Result result = vf_parse_bytecode(ctx);
    if (VF_OK != result) {
        goto labelEnd_verifyClassBytecode;
    }
    // build a control flow graph
    result = vf_create_graph(ctx);
    if (VF_OK != result) {
        goto labelEnd_verifyClassBytecode;
    }

    result = vf_check_graph(ctx);
    if (VF_OK != result) {
        goto labelEnd_verifyClassBytecode;
    }

  labelEnd_verifyClassBytecode:

    VF_TRACE("method", VF_REPORT_CTX(ctx) << "method statistics: "
             << " rets: " << ctx->m_retnum
             << ", length: " << ctx->m_len
             << ", handlers: " << ctx->m_handlers
             << ", max stack: " << ctx->m_maxstack
             << ", max locals: " << ctx->m_maxlocal);
    vf_free_graph(ctx);
    return result;
}                               // vf_verify_method_bytecode

/************************************************************
 ******************** Offset functions **********************
 ************************************************************/

/** 
 * Creates array of instruction branch offsets.
 */
static inline void vf_create_instr_offset(vf_Instr *instr,      // given instruction
                                          unsigned offcount,    // number of offets
                                          vf_Pool *pool)        // memory pool
{
    assert(!instr->m_off);
    instr->m_off = (unsigned *) vf_palloc(pool, offcount * sizeof(unsigned));
    instr->m_offcount = offcount;
}                               // vf_create_instruction_offset

/**
 * Sets instruction branch offset.
 */
static inline void vf_set_instr_offset(vf_InstrHandle instr,    // given instruction
                                       unsigned offnum, // offset index in array
                                       unsigned value)  // offset value
{
    assert(instr->m_off && offnum < instr->m_offcount);
    instr->m_off[offnum] = value;
}                               // vf_set_instruction_offset

/**
 * Function creates a single branch.
 */
static inline void vf_set_single_branch_offset(vf_Instr *instr, // given instruction 
                                               unsigned value,  // offset value
                                               vf_Pool *pool)   // memory pool
{
    vf_create_instr_offset(instr, 1, pool);
    vf_set_instr_offset(instr, 0, value);
}                               // vf_set_single_instruction_offset

/************************************************************
 ****************** Modifier functions **********************
 ************************************************************/

/**
 * Sets stack modifier attribute for given code instruction.
 */
static inline void vf_set_stack_modifier(vf_Instr *instr,       // code instruction
                                         int modify)    // stack modifier value
{
    // set stack modifier for instruction
    instr->m_stack = (short) modify;
}                               // vf_set_stack_modifier

/**
 * Sets minimal stack attribute for given code instruction.
 */
static inline void vf_set_min_stack(vf_Instr *instr,    // code instruction
                                    unsigned min_stack) // minimal stack value
{
    // set minimal stack for instruction
    instr->m_minstack = (unsigned short) min_stack;
}                               // vf_set_min_stack

/**
 * The length of the longest instruction except switches.
 */
const unsigned GOTO_W_LEN = 5;
/**
 * Sets basic block attribute for instruction.
 */
static inline void
vf_set_basic_block_flag(unsigned short pc, vf_ContextHandle ctx)
{
    assert(pc < ctx->m_len + GOTO_W_LEN);
    // set begin of basic block for instruction
    ctx->m_bc[pc].m_is_bb_start = true;
}                               // vf_set_basic_block_flag


/**
 * Sets flags for the instruction.
 */
static inline void vf_set_instr_type(vf_Instr *instr,   // instruction
                                     vf_InstrType type) // given flags
{
    // set flag for instruction
    instr->m_type = type;
}                               // vf_set_instr_type

/**
 * Checks local number.
 */
static inline vf_Result vf_check_local_var_number(unsigned local,       // local number
                                                  vf_Context *ctx)      // verification context
{
    // check local variable number
    if (local >= ctx->m_maxlocal) {
        VF_REPORT(ctx, "Incorrect usage of local variable");
        return VF_ErrorLocals;
    }
    return VF_OK;
}                               // vf_check_local_var_number

/**
 * Checks branch offset.
 */
static inline vf_Result vf_check_branch_offset(int offset,      // given instruction offset
                                               vf_Context *ctx) // verification context
{
    if (offset < 0 || (unsigned) offset >= ctx->m_len) {
        VF_REPORT(ctx, "Instruction branch offset is out of range");
        return VF_ErrorBranch;
    }
    ctx->m_bc[offset].m_is_bb_start = true;
    return VF_OK;
}                               // vf_check_branch_offset

/**
 * Function parses local variable number from instruction bytecode.
 */
static inline unsigned short vf_get_local_var_number(vf_InstrHandle instr,      // code instruction
                                                     unsigned char *bytecode,   // method bytecode
                                                     unsigned *index_p, // index in bytecode array
                                                     bool wide_flag)    // if this is a wide instruction
{
    unsigned short local;

    if ((wide_flag)) {
        // get number of local variable
        local =
            (unsigned short) ((bytecode[(*index_p)] << 8) |
                              (bytecode[(*index_p) + 1]));
        // skip parameter (u2)
        (*index_p) += 2;
    } else {
        // get number of local variable
        local = (unsigned short) bytecode[(*index_p)];
        // skip parameter (u1)
        (*index_p)++;
    }
    return local;
}                               // vf_get_local_var_number

/**
 * Receives half word (2 bytes) instruction branch offset
 * value from bytecode array.
 */
static inline int vf_get_hword_offset(unsigned code_pc, // instruction offset in bytecode array
                                      unsigned char *bytecode,  // bytecode array
                                      unsigned *index_p)        // offset index in bytecode array
{
    // get first branch offset
    int offset = (int) code_pc
        + (short) ((bytecode[(*index_p)] << 8) | (bytecode[(*index_p) + 1]));
    // skip parameter (s2)
    (*index_p) += 2;
    return offset;
}                               // vf_get_hword_offset

/**
 * Receives word (4 bytes) instruction branch offset value from bytecode array.
 */
static inline int vf_get_word_offset(unsigned code_pc,  // instruction offset in bytecode array
                                     unsigned char *bytecode,   // bytecode array
                                     unsigned *index_p) // offset index in bytecode array
{
    // get switch branch offset
    int offset = (int) code_pc
        +
        (int) ((bytecode[(*index_p)] << 24) | (bytecode[(*index_p) + 1] << 16)
               | (bytecode[(*index_p) + 2] << 8) |
               (bytecode[(*index_p) + 3]));
    // skip parameter (s4) 
    (*index_p) += 4;
    return offset;
}                               // vf_get_word_offset

/**
 * Receives half word (2 bytes) branch offset from bytecode array,
 * sets into instruction and returns it.
 */
static inline int vf_get_single_hword_branch_offset(vf_Instr *instr,    // instruction
                                                    unsigned code_pc,   // offset in bytecode array
                                                    unsigned char *bytecode,    // bytecode array
                                                    unsigned *index_p,  // offset index in bytecode array
                                                    vf_Pool *pool)      // memory pool
{
    // get first branch offset
    int offset = vf_get_hword_offset(code_pc, bytecode, index_p);
    // create and set edge branch for instruction
    vf_set_single_branch_offset(instr, offset, pool);
    return offset;
}                               // vf_get_single_hword_branch_offset

/**
 * Receives word (4 bytes) branch offset from bytecode array,
 * sets into instruction and returns it.
 */
static inline int vf_get_single_word_branch_offset(vf_Instr *instr,     // instruction
                                                   unsigned code_pc,    // offset in bytecode array
                                                   unsigned char *bytecode,     // bytecode array
                                                   unsigned *index_p,   // offset index in bytecode array
                                                   vf_Pool *pool)       // memory pool
{
    // get first branch offset
    int offset = vf_get_word_offset(code_pc, bytecode, index_p);
    // create and set edge branch for instruction
    vf_set_single_branch_offset(instr, offset, pool);
    return offset;
}                               // vf_get_single_word_branch_offset

/**
 * Receives half word (2 bytes) branch offset from bytecode array and
 * sets received offset and next instruction offset into instruction.
 * Function returns received offset.
 */
static inline int vf_get_double_hword_branch_offset(vf_Instr *instr,    // instruction
                                                    unsigned code_pc,   // instruction offset in bytcode array
                                                    unsigned char *bytecode,    // bytecode array
                                                    unsigned *index_p,  // offset index in bytecode array
                                                    vf_Pool *pool)      // memory pool
{
    // get first branch offset
    int offset = vf_get_hword_offset(code_pc, bytecode, index_p);
    // create and set edge branches for instruction
    vf_create_instr_offset(instr, 2, pool);
    // set first edge branch for instruction
    vf_set_instr_offset(instr, 0, offset);
    // set second edge branch for instruction
    vf_set_instr_offset(instr, 1, (*index_p));
    return offset;
}                               // vf_get_double_hword_branch_offset

/**
 * Receives word (4 bytes) branch offset from bytecode array and
 * sets received offset and next instruction offset into instruction.
 * Function returns received offset.
 */
static inline int vf_get_double_word_branch_offset(vf_Instr *instr,     // instruction
                                                   unsigned code_pc,    // instruction offset in bytcode array
                                                   unsigned char *bytecode,     // bytecode array
                                                   unsigned *index_p,   // offset index in bytecode array
                                                   vf_Pool *pool)       // memory pool
{
    // get first branch offset
    int offset = vf_get_word_offset(code_pc, bytecode, index_p);
    // create and set edge branches for instruction
    vf_create_instr_offset(instr, 2, pool);
    // set first edge branch for instruction
    vf_set_instr_offset(instr, 0, offset);
    // set second edge branch for instruction
    vf_set_instr_offset(instr, 1, (*index_p));
    return offset;
}                               // vf_get_double_word_branch_offset

/**
 * Receives tableswitch branch from bytecode array and
 * sets received offset into instruction.
 * Function returns received branch.
 */
static inline int vf_get_tableswitch_alternative(vf_InstrHandle instr,  // instruction
                                                 unsigned code_pc,      // offset in bytcode array
                                                 unsigned alternative,  // number of tableswitch branch
                                                 unsigned char *bytecode,       // bytecode array
                                                 unsigned *index_p)     // offset index in bytecode array
{
    // get first branch offset
    int offset = vf_get_word_offset(code_pc, bytecode, index_p);
    // set first edge branch for instruction
    vf_set_instr_offset(instr, alternative, offset);
    return offset;
}                               // vf_get_tableswitch_alternative

/**
 * Receives tableswitch alternatives from bytecode array and
 * sets them into instruction.
 * Function returns number of alternatives.
 */
static inline int vf_set_tableswitch_offsets(vf_Instr *instr,   // instruction
                                             unsigned code_pc,  // instruction offset in bytecode array
                                             unsigned *index_p, // offset index in bytecode array
                                             unsigned char *bytecode,   // bytecode array
                                             vf_Pool *pool)     // memory pool
{
    // skip padding
    unsigned default_off = ((*index_p) + 0x3) & (~0x3U);
    unsigned index = default_off;
    // skip default offset
    index += 4;
    // get low and high tableswitch values
    int low = vf_get_word_offset(code_pc, bytecode, &index);
    int high = vf_get_word_offset(code_pc, bytecode, &index);
    int number = high - low + 2;
    // create tableswitch branches
    vf_create_instr_offset(instr, number, pool);
    // set default offset
    vf_get_tableswitch_alternative(instr, code_pc, 0, bytecode, &default_off);
    // set another instruction offsets
    for (int count = 1; count < number; count++) {
        vf_get_tableswitch_alternative(instr, code_pc, count, bytecode,
                                       &index);
    }
    // set index next instruction
    (*index_p) = index;
    return number;
}                               // vf_set_tableswitch_offsets

/**
 * Receives lookupswitch alternatives from bytecode array and
 * sets them into instruction.
 * Function returns number of alternatives.
 */
static inline vf_Result vf_set_lookupswitch_offsets(vf_Instr *instr,    // instruction
                                                    unsigned code_pc,   // instruction offset in bytecode
                                                    unsigned *index_p,  // offset index in bytecode array
                                                    unsigned char *bytecode,    // array of bytecode
                                                    unsigned *branch_p, // number of alternatives
                                                    vf_Context *ctx)    // verification context
{
    // skip padding
    unsigned default_off = ((*index_p) + 0x3) & (~0x3U);
    unsigned index = default_off;
    // skip default offset
    index += 4;
    // get alternative number of lookupswitch (add default alternative)
    int number = vf_get_word_offset(0, bytecode, &index) + 1;
    *branch_p = number;
    // create and tableswitch branches
    vf_create_instr_offset(instr, number, ctx->m_pool);
    // set default offset
    vf_get_tableswitch_alternative(instr, code_pc, 0, bytecode, &default_off);
    // set another instruction offsets
    int old_key = INT_MIN;
    for (int count = 1; count < number; count++) {
        // get and check branch key value
        int key = vf_get_word_offset(0, bytecode, &index);
        if (old_key < key) {
            old_key = key;
        } else if (key != INT_MIN) {
            VF_REPORT(ctx,
                      "Instruction lookupswitch has unsorted key values");
            return VF_ErrorInstruction;
        }
        // get lookupswitch alternative and set offset to instruction
        vf_get_tableswitch_alternative(instr, code_pc, count, bytecode,
                                       &index);
    }
    // set index next instruction
    (*index_p) = index;
    return VF_OK;
}                               // vf_set_lookupswitch_offsets

/************************************************************
 ********************* Vector functions *********************
 ************************************************************/

/**
 * Sets check type for a given stack map vector entry.
 */
static inline void vf_set_vector_check(vf_MapEntry *vector,     // stack map vector
                                       unsigned num,    // vector entry number
                                       vf_CheckConstraint check)        // constraint check type
{
    // set check for map vector entry
    assert(check < VF_CHECK_NUM);
    vector[num].m_ctype = check;
}                               // vf_set_vector_check

/**
 * Sets constraint pool index for a given stack map vector entry.
 */
static inline void vf_set_vector_check_index(vf_MapEntry *vector,       // stack map vector
                                             unsigned num,      // vector entry number
                                             unsigned short index)      // constraint pool index
{
    // set index for a map vector entry
    vector[num].m_index = index;
}                               // vf_set_vector_check_index

/**
 * Sets a given data type to stack map vector entry.
 */
static inline void vf_set_vector_type(vf_MapEntry *vector,      // stack map vector
                                      unsigned num,     // vector entry number
                                      vf_MapType type)  // stack map entry type
{
    assert(type < SM_NUMBER);
    vector[num].m_type = type;
}                               // vf_set_vector_type

/**
 * Sets null data type for given stack map vector entry.
 */
static inline void vf_set_vector_stack_entry_null(vf_MapEntry *vector,  // stack map vector
                                                  unsigned num) // vector entry number
{
    // set stack map vector entry by null
    vector[num].m_type = SM_NULL;
}                               // vf_set_vector_stack_entry_null

/**
 * Sets int data type for given stack map vector entry.
 */
static inline void vf_set_vector_stack_entry_int(vf_MapEntry *vector,   // stack map vector
                                                 unsigned num)  // vector entry number
{
    // set stack map vector entry by int
    vector[num].m_type = SM_INT;
}                               // vf_set_vector_stack_entry_int

/**
 * Sets float data type for given stack map vector entry.
 */
static inline void vf_set_vector_stack_entry_float(vf_MapEntry *vector, // stack map vector
                                                   unsigned num)        // vector entry number
{
    // set stack map vector entry by float
    vector[num].m_type = SM_FLOAT;
}                               // vf_set_vector_stack_entry_float

/**
 * Sets long data type for given stack map vector entry.
 */
static inline void vf_set_vector_stack_entry_long(vf_MapEntry *vector,  // stack map vector
                                                  unsigned num) // vector entry number
{
    // set stack map vector entry by long
    vector[num].m_type = SM_LONG_HI;
    vector[num + 1].m_type = SM_LONG_LO;
}                               // vf_set_vector_stack_entry_long

/**
 * Sets double data type for given stack map vector entry.
 */
static inline void vf_set_vector_stack_entry_double(vf_MapEntry *vector,        // stack map vector
                                                    unsigned num)       // vector entry number
{
    // set stack map vector entry by double
    vector[num].m_type = SM_DOUBLE_HI;
    vector[num + 1].m_type = SM_DOUBLE_LO;
}                               // vf_set_vector_stack_entry_double

/**
 * Sets return address data type for given stack map vector entry.
 */
static inline void vf_set_vector_stack_entry_addr(vf_MapEntry *vector,  // stack map vector
                                                  unsigned num, // vector entry number
                                                  unsigned count)       // program count
{
    // set stack map vector entry by return address
    vector[num].m_type = SM_RETURN_ADDR;
    vector[num].m_pc = count;
}                               // vf_set_vector_stack_entry_addr

/**
 * Sets single word data type for given stack map vector entry.
 */
static inline void vf_set_vector_stack_entry_word(vf_MapEntry *vector,  // stack map vector
                                                  unsigned num) // vector entry number
{
    // set stack map vector entry by word
    vector[num].m_type = SM_WORD;
}                               // vf_set_vector_stack_entry_word

/**
 * Sets double word data type for given stack map vector entry.
 */
static inline void vf_set_vector_stack_entry_word2(vf_MapEntry *vector, // stack map vector
                                                   unsigned num)        // vector entry number
{
    // set stack map vector entry by double word
    vector[num].m_type = SM_WORD2_HI;
    vector[num + 1].m_type = SM_WORD2_LO;
}                               // vf_set_vector_stack_entry_word2

/**
 * Sets int data type for given local variable vector entry.
 */
static inline void vf_set_vector_local_var_int(vf_MapEntry *vector,     // local variable vector
                                               unsigned num,    // vector entry number
                                               unsigned local)  // number of local variable
{
    // set local variable vector entry by int
    vector[num].m_type = SM_INT;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short) local;
}                               // vf_set_vector_local_var_int

/**
 * Sets float data type for given local variable vector entry.
 */
static inline void vf_set_vector_local_var_float(vf_MapEntry *vector,   // local variable vector
                                                 unsigned num,  // vector entry number
                                                 unsigned local)        // number of local variable
{
    // set local variable vector entry by float
    vector[num].m_type = SM_FLOAT;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short) local;
}                               // vf_set_vector_local_var_float

/**
 * Sets long data type for given local variable vector entry.
 */
static inline void vf_set_vector_local_var_long(vf_MapEntry *vector,    // local variable vector
                                                unsigned num,   // vector entry number
                                                unsigned local) // number of local variable
{
    // set local variable vector entry by long
    vector[num].m_type = SM_LONG_HI;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short) local;
    vector[num + 1].m_type = SM_LONG_LO;
    vector[num + 1].m_is_local = true;
    vector[num + 1].m_local = (unsigned short) (local + 1);
}                               // vf_set_vector_local_var_long

/**
 * Sets double data type for given local variable vector entry.
 */
static inline void vf_set_vector_local_var_double(vf_MapEntry *vector,  // local variable vector
                                                  unsigned num, // vector entry number
                                                  unsigned local)       // number of local variable
{
    // set local variable vector entry by double
    vector[num].m_type = SM_DOUBLE_HI;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short) local;
    vector[num + 1].m_type = SM_DOUBLE_LO;
    vector[num + 1].m_is_local = true;
    vector[num + 1].m_local = (unsigned short) (local + 1);
}                               // vf_set_vector_local_var_double

/**
 * Sets reference data type for given local variable vector entry.
 */
static inline void vf_set_vector_local_var_ref(vf_MapEntry *vector,     // local variable vector
                                               unsigned num,    // vector entry number
                                               vf_ValidType *type,      // reference type
                                               unsigned local)  // number of local variable
{
    // set stack map vector entry by reference
    vector[num].m_type = SM_REF;
    vector[num].m_vtype = type;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short) local;
}                               // vf_set_vector_local_var_ref

/**
 * Sets return address data type for given local variable vector entry.
 */
static inline void vf_set_vector_local_var_addr(vf_MapEntry *vector,    // stack map vector
                                                unsigned num,   // vector entry number
                                                unsigned count, // program count
                                                unsigned local) // number of local variable
{
    // set local variable vector entry to return address
    vector[num].m_type = SM_RETURN_ADDR;
    vector[num].m_pc = count;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short) local;
}                               // vf_set_vector_local_var_addr

/**
 * Sets a given data type for a given local variable vector entry.
 */
static inline void vf_set_vector_local_var_type(vf_MapEntry *vector,    // local variable vector
                                                unsigned num,   // vector entry number
                                                vf_MapType type,        // stack map entry type
                                                unsigned local) // number of local variable
{
    // set stack map vector entry by reference
    vector[num].m_type = type;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short) local;
}                               // vf_set_vector_local_var_type

/************************************************************
 ****************** IN Vector functions *********************
 ************************************************************/

/**
 * Allocates memory for new code instruction in the IN stack map vector.
 */
static inline void vf_new_in_vector(vf_Instr *instr,    // code instruction
                                    unsigned len,       // vector length
                                    vf_Pool *pool)      // memory pool
{
    // create IN vector
    instr->m_inlen = (unsigned short) len;
    instr->m_invector =
        (vf_MapEntry *) vf_palloc(pool, len * sizeof(vf_MapEntry));
}                               // vf_new_in_vector

/**
 * Sets check attribute for a code instruction in the IN stack map vector entry.
 */
static inline void vf_set_in_vector_check(vf_InstrHandle instr, // code instruction
                                          unsigned num, // IN vector entry number
                                          vf_CheckConstraint check)     // constraint check type
{
    vf_set_vector_check(instr->m_invector, num, check);
}                               // vf_set_in_vector_check

/**
 * Sets constant pool index for a code instruction IN stack map vector entry.
 */
static inline void vf_set_in_vector_check_index(vf_InstrHandle instr,   // code instruction
                                                unsigned num,   // IN vector entry number
                                                unsigned short index)   // constraint pool index
{
    vf_set_vector_check_index(instr->m_invector, num, index);
}                               // vf_set_in_vector_check_index

/**
 * Sets a given data type to stack map vector entry.
 */
static inline void vf_set_in_vector_type(vf_InstrHandle instr,  // code instruction
                                         unsigned num,  // vector entry number
                                         vf_MapType type)       // stack map entry type
{
    vf_set_vector_type(instr->m_invector, num, type);
}                               // vf_set_in_vector_type

/**
 * Sets int data type for code instruction IN stack map vector entry.
 */
static inline void vf_set_in_vector_stack_entry_int(vf_InstrHandle instr,       // code instruction
                                                    unsigned num)       // IN vector entry number
{
    vf_set_vector_stack_entry_int(instr->m_invector, num);
}                               // vf_set_vector_stack_entry_int

/**
 * Sets float data type for code instruction IN stack map vector entry.
 */
static inline void vf_set_in_vector_stack_entry_float(vf_InstrHandle instr,     // code instruction
                                                      unsigned num)     // IN vector entry number
{
    vf_set_vector_stack_entry_float(instr->m_invector, num);
}                               // vf_set_in_vector_stack_entry_float

/**
 * Sets long data type for code instruction IN stack map vector entry.
 */
static inline void vf_set_in_vector_stack_entry_long(vf_InstrHandle instr,      // code instruction
                                                     unsigned num)      // IN vector entry number
{
    vf_set_vector_stack_entry_long(instr->m_invector, num);
}                               // vf_set_in_vector_stack_entry_long

/**
 * Sets double data type for code instruction IN stack map vector entry.
 */
static inline void vf_set_in_vector_stack_entry_double(vf_InstrHandle instr,    // code instruction
                                                       unsigned num)    // IN vector entry number
{
    vf_set_vector_stack_entry_double(instr->m_invector, num);
}                               // vf_set_in_vector_stack_entry_double

/**
 * Sets reference data type for code instruction IN stack map vector entry.
 */
static inline void vf_set_in_vector_stack_entry_ref(vf_InstrHandle instr,       // code instruction
                                                    unsigned num,       // IN vector entry number
                                                    vf_ValidType *type) // reference type
{
    vf_set_vector_stack_entry_ref(instr->m_invector, num, type);
}                               // vf_set_in_vector_stack_entry_ref

/**
 * Sets single word data type for code instruction IN stack map vector entry.
 */
static inline void vf_set_in_vector_stack_entry_word(vf_InstrHandle instr,      // code instruction
                                                     unsigned num)      // IN vector entry number
{
    vf_set_vector_stack_entry_word(instr->m_invector, num);
}                               // vf_set_in_vector_stack_entry_word

/**
 * Sets double word data type for code instruction IN stack map vector entry.
 */
static inline void vf_set_in_vector_stack_entry_word2(vf_InstrHandle instr,     // code instruction
                                                      unsigned num)     // IN vector entry number
{
    vf_set_vector_stack_entry_word2(instr->m_invector, num);
}                               // vf_set_in_vector_stack_entry_word2

/**
 * Sets int data type for code instruction IN local variable vector entry.
 */
static inline void vf_set_in_vector_local_var_int(vf_InstrHandle instr, // code instruction
                                                  unsigned num, // IN vector entry number
                                                  unsigned local)       // local variable number
{
    vf_set_vector_local_var_int(instr->m_invector, num, local);
}                               // vf_set_in_vector_local_var_int

/**
 * Sets float data type for code instruction IN local variable vector entry.
 */
static inline void vf_set_in_vector_local_var_float(vf_InstrHandle instr,       // code instruction
                                                    unsigned num,       // IN vector entry number
                                                    unsigned local)     // local variable number
{
    vf_set_vector_local_var_float(instr->m_invector, num, local);
}                               // vf_set_in_vector_local_var_float 

/**
 * Sets long data type for code instruction IN local variable vector entry.
 */
static inline void vf_set_in_vector_local_var_long(vf_InstrHandle instr,        // code instruction
                                                   unsigned num,        // IN vector entry number
                                                   unsigned local)      // local variable number
{
    vf_set_vector_local_var_long(instr->m_invector, num, local);
}                               // vf_set_in_vector_local_var_long 

/**
 * Sets double data type for code instruction IN local variable vector entry.
 */
static inline void vf_set_in_vector_local_var_double(vf_InstrHandle instr,      // code instruction
                                                     unsigned num,      // IN vector entry number
                                                     unsigned local)    // local variable number
{
    vf_set_vector_local_var_double(instr->m_invector, num, local);
}                               // vf_set_in_vector_local_var_double 

/**
 * Sets reference data type for code instruction IN local variable vector entry.
 */
static inline void vf_set_in_vector_local_var_ref(vf_InstrHandle instr, // code instruction
                                                  unsigned num, // IN vector entry number
                                                  vf_ValidType *type,   // reference type
                                                  unsigned local)       // local variable number
{
    vf_set_vector_local_var_ref(instr->m_invector, num, type, local);
}                               // vf_set_in_vector_local_var_ref 

/************************************************************
 ***************** OUT Vector functions *********************
 ************************************************************/

/**
 * Function allocates memory for new code instruction OUT stack map vector.
 */
static inline void vf_new_out_vector(vf_Instr *instr,   // code instruction
                                     unsigned len,      // vector length
                                     vf_Pool *pool)     // memory pool
{
    // create stack map OUT vector
    instr->m_outlen = (unsigned short) len;
    instr->m_outvector =
        (vf_MapEntry *) vf_palloc(pool, len * sizeof(vf_MapEntry));
}                               // vf_new_out_vector

/**
 * Sets a given data type to stack map OUT vector entry.
 */
static inline void vf_set_out_vector_type(vf_InstrHandle instr, // code instruction
                                          unsigned num, // vector entry number
                                          vf_MapType type)      // stack map entry type
{
    vf_set_vector_type(instr->m_outvector, num, type);
}                               // vf_set_out_vector_type

/**
 * Sets a given program counter to stack map OUT vector entry.
 */
static inline void vf_set_out_vector_opcode_new(vf_InstrHandle instr,   // code instruction
                                                unsigned num,   // vector entry number
                                                unsigned opcode_new)    // new opcode
{
    instr->m_outvector[num].m_new = opcode_new;
}                               // vf_set_out_vector_opcode_new

/**
 * Sets int data type for code instruction OUT stack map vector entry.
 */
static inline void vf_set_out_vector_stack_entry_int(vf_InstrHandle instr,      // code instruction
                                                     unsigned num)      // OUT vector entry number
{
    vf_set_vector_stack_entry_int(instr->m_outvector, num);
}                               // vf_set_vector_stack_entry_int

/**
 * Sets float data type for code instruction OUT stack map vector entry.
 */
static inline void vf_set_out_vector_stack_entry_float(vf_InstrHandle instr,    // code instruction
                                                       unsigned num)    // OUT vector entry number
{
    vf_set_vector_stack_entry_float(instr->m_outvector, num);
}                               // vf_set_out_vector_stack_entry_float

/**
 * Sets long data type for code instruction OUT stack map vector entry.
 */
static inline void vf_set_out_vector_stack_entry_long(vf_InstrHandle instr,     // code instruction
                                                      unsigned num)     // OUT vector entry number
{
    vf_set_vector_stack_entry_long(instr->m_outvector, num);
}                               // vf_set_out_vector_stack_entry_long

/**
 * Sets double data type for code instruction OUT stack map vector entry.
 */
static inline void vf_set_out_vector_stack_entry_double(vf_InstrHandle instr,   // code instruction
                                                        unsigned num)   // OUT vector entry number
{
    vf_set_vector_stack_entry_double(instr->m_outvector, num);
}                               // vf_set_out_vector_stack_entry_double

/**
 * Sets reference data type for code instruction OUT stack map vector entry.
 */
static inline void vf_set_out_vector_stack_entry_ref(vf_InstrHandle instr,      // code instruction
                                                     unsigned num,      // OUT vector entry number
                                                     vf_ValidType *type)        // reference type
{
    vf_set_vector_stack_entry_ref(instr->m_outvector, num, type);
}                               // vf_set_out_vector_stack_entry_ref

/**
 * Sets int data type for code instruction OUT local variable vector entry.
 */
static inline void vf_set_out_vector_local_var_int(vf_InstrHandle instr,        // code instruction
                                                   unsigned num,        // OUT vector entry number
                                                   unsigned local)      // local variable number
{
    vf_set_vector_local_var_int(instr->m_outvector, num, local);
}                               // vf_set_out_vector_local_var_int

/**
 * Sets float data type for code instruction OUT local variable vector entry.
 */
static inline void vf_set_out_vector_local_var_float(vf_InstrHandle instr,      // code instruction
                                                     unsigned num,      // OUT vector entry number
                                                     unsigned local)    // local variable number
{
    vf_set_vector_local_var_float(instr->m_outvector, num, local);
}                               // vf_set_out_vector_local_var_float 

/**
 * Sets long data type for code instruction OUT local variable vector entry.
 */
static inline void vf_set_out_vector_local_var_long(vf_InstrHandle instr,       // code instruction
                                                    unsigned num,       // OUT vector entry number
                                                    unsigned local)     // local variable number
{
    vf_set_vector_local_var_long(instr->m_outvector, num, local);
}                               // vf_set_out_vector_local_var_long 

/**
 * Sets double data type for code instruction OUT local variable vector entry.
 */
static inline void vf_set_out_vector_local_var_double(vf_InstrHandle instr,     // code instruction
                                                      unsigned num,     // OUT vector entry number
                                                      unsigned local)   // local variable number
{
    vf_set_vector_local_var_double(instr->m_outvector, num, local);
}                               // vf_set_out_vector_local_var_double 

/**
 * Sets a given data type for code instruction OUT local variable vector entry.
 */
static inline void vf_set_out_vector_local_var_type(vf_InstrHandle instr,       // code instruction
                                                    unsigned num,       // OUT vector entry number
                                                    vf_MapType type,    // stack map entry
                                                    unsigned local)     // local variable number
{
    vf_set_vector_local_var_type(instr->m_outvector, num, type, local);
}                               // vf_set_out_vector_local_var_type

/************************************************************
 ************** Parse description functions *****************
 ************************************************************/
/**
 * Parses method, class or field descriptions.
 */
void vf_parse_description(const char *descr,    // descriptor of method, class or field
                          unsigned short *inlen,        // returned number of IN descriptor parameters 
                          unsigned short *outlen)       // returned number of OUT descriptor
                                            // parameters (for method)
{
    /**
     * Check parameters
     */
    assert(descr);
    assert(inlen);

    /**
     * Parse description for defining method stack
     */
    *inlen = 0;
    if (outlen) {
        *outlen = 0;
    }
    bool array = false;

    // start parsing input parameters
    unsigned short *count = inlen;
    for (unsigned short index = 0; descr[index]; index++) {
        switch (descr[index]) {
        case 'L':
            // skip class name
            do {
                index++;
#if _VF_DEBUG
                if (!descr[index]) {
                    VF_DIE
                        ("vf_parse_description: incorrect structure of constant pool");
                }
#endif // _VF_DEBUG
            } while (descr[index] != ';');
        case 'B':
        case 'C':
        case 'F':
        case 'I':
        case 'S':
        case 'Z':
            if (!array) {
                (*count)++;     // increase stack value
            } else {
                array = false;  // reset array structure
            }
        case 'V':
            break;
        case 'D':
        case 'J':
            if (!array) {
                (*count) += 2;  // increase stack value
            } else {
                array = false;  // reset array structure
            }
            break;
        case '[':
            array = true;
            // skip array structure
            do {
                index++;
#if _VF_DEBUG
                if (!descr[index]) {
                    VF_DIE
                        ("vf_parse_description: incorrect structure of constant pool");
                }
#endif // _VF_DEBUG
            } while (descr[index] == '[');
            index--;
            (*count)++;         // increase stack value
            break;
        case '(':
            // parse arguments
            count = inlen;
            break;
        case ')':
            // parse return value
            count = outlen;
            break;
        default:
            VF_DIE
                ("vf_parse_description: incorrect structure of constant pool");
            break;
        }
    }
}                               // vf_parse_description

static inline void
vf_set_array_ref(const char *type,
                 unsigned len,
                 vf_MapEntry *vector, unsigned *index, vf_ContextHandle ctx)
{
    // set array reference entry
    vf_ValidType *valid = ctx->m_type->NewType(type, len);
    vf_set_vector_stack_entry_ref(vector, *index, valid);
    (*index)++;
}                               // vf_set_array_ref

/**
 * Function parses descriptor and sets input and output data flow vectors.
 */
void vf_set_description_vector(const char *descr,       // descriptor
                               unsigned short inlen,    // number of entries for IN vector
                               unsigned short add,      // additional number of entries
                               // to IN data flow vector
                               unsigned short outlen,   // number of entries for OUT vector
                               vf_MapEntry **invector,  // pointer to IN vector
                               vf_MapEntry **outvector, // pointer to OUT vector
                               vf_ContextHandle ctx)    // verification context
{
    const char *type = 0;
    unsigned index, count = 0, vector_index;
    vf_MapEntry **vector;
    vf_ValidType *valid;
    vf_Pool *pool = ctx->m_pool;

    /**
     * Check parameters
     */
    assert(descr);

    /**
     * Create vectors
     */
    if (inlen + add) {
        assert(invector);
        vf_new_vector(invector, inlen + add, pool);
    }
    if (outlen) {
        assert(outvector);
        vf_new_vector(outvector, outlen, pool);
    }

    /**
     * Parse description for fill vectors value
     */
    bool array = false;
    vector_index = add;
    for (index = 0, vector = invector; descr[index]; index++) {
        switch (descr[index]) {
        case 'L':
            // skip method name
            if (!array) {
                count = index;
                type = &descr[count];
            }
            do {
                index++;
            } while (descr[index] != ';');
            if (!array) {
                // set vector reference entry
                valid = ctx->m_type->NewType(type, index - count);
                vf_set_vector_stack_entry_ref(*vector, vector_index, valid);
                vector_index++;
            } else {
                vf_set_array_ref(type, index - count, *vector, &vector_index,
                                 ctx);
                // reset array structure
                array = false;
            }
            break;
        case 'Z':
            if (!array) {
                // set vector int entry
                vf_set_vector_stack_entry_int(*vector, vector_index);
                vector_index++;
            } else {
                // set integer array type
                unsigned iter;
                unsigned len = index - count + 1;
                char *int_array = (char *) vf_palloc(pool, len);
                for (iter = 0; iter < len - 1; iter++) {
                    int_array[iter] = '[';
                }
                int_array[iter] = 'B';
                vf_set_array_ref(int_array, len, *vector, &vector_index, ctx);
                // reset array structure
                array = false;
            }
            break;
        case 'I':
        case 'B':
        case 'C':
        case 'S':
            if (!array) {
                // set vector int entry
                vf_set_vector_stack_entry_int(*vector, vector_index);
                vector_index++;
            } else {
                vf_set_array_ref(type, index - count + 1, *vector,
                                 &vector_index, ctx);
                // reset array structure
                array = false;
            }
            break;
        case 'F':
            if (!array) {
                // set vector float entry
                vf_set_vector_stack_entry_float(*vector, vector_index);
                vector_index++;
            } else {
                vf_set_array_ref(type, index - count + 1, *vector,
                                 &vector_index, ctx);
                // reset array structure
                array = false;
            }
            break;
        case 'D':
            if (!array) {
                // set vector double entry
                vf_set_vector_stack_entry_double(*vector, vector_index);
                vector_index += 2;
            } else {
                vf_set_array_ref(type, index - count + 1, *vector,
                                 &vector_index, ctx);
                // reset array structure
                array = false;
            }
            break;
        case 'J':
            if (!array) {
                // set vector long entry
                vf_set_vector_stack_entry_long(*vector, vector_index);
                vector_index += 2;
            } else {
                vf_set_array_ref(type, index - count + 1, *vector,
                                 &vector_index, ctx);
                // reset array structure
                array = false;
            }
            break;
        case '[':
            // set vector reference entry
            array = true;
            count = index;
            type = &descr[count];
            // skip array structure
            do {
                index++;
            } while (descr[index] == '[');
            index--;
            break;
        case '(':
            // fill in vector (parse arguments)
            vector = invector;
            vector_index = add;
            break;
        case ')':
            // fill out vector (parse return value)
            vector = outvector;
            vector_index = 0;
            break;
        case 'V':
        default:
            break;
        }
    }
}                               // vf_set_description_vector

/************************************************************
 **************** Constant pool functions *******************
 ************************************************************/

/**
 * Function returns name string from NameAndType constant pool entry.
 */
static inline const char *vf_get_name_from_cp_nameandtype(unsigned short index, // constant pool entry index
                                                          vf_ContextHandle ctx) // verification context
{
    // get name constant pool index
    unsigned short name_cp_index =
        class_get_cp_name_index(ctx->m_class, index);
    // get name string from NameAndType constant pool entry
    const char *name = class_get_cp_utf8_bytes(ctx->m_class, name_cp_index);

    return name;
}                               // vf_get_name_from_cp_nameandtype

/**
 * Function returns descriptor string from NameAndType constant pool entry.
 */
static inline const char *vf_get_decriptor_from_cp_nameandtype(unsigned short index,    // constant pool entry index
                                                               vf_ContextHandle ctx)    // verification context
{
    // get description constant pool index
    unsigned short descr_cp_index =
        class_get_cp_descriptor_index(ctx->m_class, index);
    // get descriptor from NameAndType constant pool entry
    const char *descr = class_get_cp_utf8_bytes(ctx->m_class, descr_cp_index);

    return descr;
}                               // vf_get_cp_nameandtype

/**
 * Function returns valid type string by given class name.
 */
static inline const char *vf_get_class_valid_type(const char *class_name,       // class name
                                                  size_t name_len,      // class name length
                                                  size_t * len, // length of created string
                                                  vf_Pool *pool)        // memory pool
{
    char *result;

    // create valid type
    if (class_name[0] == '[') {
        // copy array class name
        if (class_name[name_len - 1] == ';') {
            // object type array
            *len = name_len - 1;
            result = (char *) class_name;
        } else {
            // primitive type array
            *len = name_len;
            result = (char *) class_name;
            switch (class_name[name_len - 1]) {
            case 'Z':
                // set integer array type
                result = (char *) vf_palloc(pool, (*len) + 1);
                memcpy(result, class_name, name_len);
                result[name_len - 1] = 'B';
                break;
            default:
                // don't need change type
                result = (char *) class_name;
                break;
            }
        }
    } else {
        // create class signature
        *len = name_len + 1;
        result = (char *) vf_palloc(pool, (*len) + 1);
        result[0] = 'L';
        memcpy(&result[1], class_name, name_len);
    }
    return result;
}                               // vf_get_class_valid_type

/**
 * Function creates valid type by given class name.
 */
vf_ValidType *vf_create_class_valid_type(const char *class_name,        // class name
                                         vf_ContextHandle ctx)  // verification context
{
    size_t len;
    vf_ValidType *result;

    // get class valid type
    const char *type = vf_get_class_valid_type(class_name,
                                               strlen(class_name),
                                               &len, ctx->m_pool);
    // create valid type
    result = ctx->m_type->NewType(type, len);
    return result;
}                               // vf_create_class_valid_type

/**
 * Checks constant pool reference entry.
 * Function returns result of check and
 * name string, type descriptor string and class name string (if it's needed).
 */
static inline void vf_check_cp_ref(unsigned short index,        // constant pool entry index
                                   const char **name,   // pointer to name
                                   const char **descr,  // pointer to descriptor
                                   const char **class_name,     // pointer to class name
                                   vf_ContextHandle ctx)        // verification context
{
    assert(name);
    assert(descr);

    // get class name if it's needed
    if (class_name) {
        unsigned short class_cp_index =
            class_get_cp_ref_class_index(ctx->m_class, index);
        *class_name = vf_get_cp_class_name(ctx->m_class, class_cp_index);
    }
    // get name and descriptor from NameAndType constant pool entry
    unsigned short name_and_type_cp_index =
        class_get_cp_ref_name_and_type_index(ctx->m_class, index);
    *name = vf_get_name_from_cp_nameandtype(name_and_type_cp_index, ctx);
    *descr =
        vf_get_decriptor_from_cp_nameandtype(name_and_type_cp_index, ctx);
}                               // vf_check_cp_ref

/**
 * Checks constant pool Methodref and InterfaceMethodref entries.
 * Sets IN and OUT vectors for constant pool entries.
 * @return result of the check
 */
static vf_Result vf_check_cp_method(unsigned short index,       // constant pool entry index
                                    vf_Parse * cp_parse,        // entry parse result structure
                                    bool is_interface,  // interface flag
                                    bool stack_with_ref,        // stack with this reference flag
                                    vf_Context *ctx)    // verification context
{
    // get constant pool length
    unsigned short len = class_get_cp_size(ctx->m_class);

    // check constant pool index
    CHECK_CONST_POOL_ID(index, len, ctx);
    // check constant pool type
    if (is_interface) {
        CHECK_CONST_POOL_INTERFACE(ctx, index);
    } else {
        CHECK_CONST_POOL_METHOD(ctx, index);
    }

    // get method description and name
    const char *name = NULL;
    const char *descr = NULL;
    const char *class_name;
    vf_check_cp_ref(index, &name, &descr,
                    (stack_with_ref ? &class_name : NULL), ctx);
    // check result
    assert(name);
    assert(descr);

    // set method name
    cp_parse->method.m_name = name;

    // parse method description
    vf_parse_description(descr, &cp_parse->method.m_inlen,
                         &cp_parse->method.m_outlen);
    // create instruction vectors
    if (stack_with_ref) {
        vf_set_description_vector(descr, cp_parse->method.m_inlen,
                                  1 /* ref + vector */ ,
                                  cp_parse->method.m_outlen,
                                  &cp_parse->method.m_invector,
                                  &cp_parse->method.m_outvector, ctx);
        // set first ref
        cp_parse->method.m_inlen += 1;
        vf_ValidType *type = vf_create_class_valid_type(class_name, ctx);
        vf_set_vector_stack_entry_ref(cp_parse->method.m_invector, 0, type);
    } else {
        vf_set_description_vector(descr, cp_parse->method.m_inlen,
                                  0 /* only vector */ ,
                                  cp_parse->method.m_outlen,
                                  &cp_parse->method.m_invector,
                                  &cp_parse->method.m_outvector, ctx);
    }
    return VF_OK;
}                               // vf_check_cp_method

/**
 * Checks constant pool Fieldref entry.
 * Sets data flow vector for constant pool entry.
 * @return result of the check
 */
static vf_Result vf_check_cp_field(unsigned short index,        // constant pool entry index
                                   vf_Parse * cp_parse, // entry parse result structure
                                   bool stack_with_ref, // stack with this reference flag
                                   vf_Context *ctx)     // verification context
{
    // check constant pool index and type
    unsigned short len = class_get_cp_size(ctx->m_class);
    CHECK_CONST_POOL_ID(index, len, ctx);
    CHECK_CONST_POOL_FIELD(ctx, index);

    // get field description and name
    const char *name = NULL;
    const char *descr = NULL;
    const char *class_name;
    vf_check_cp_ref(index, &name, &descr,
                    (stack_with_ref ? &class_name : NULL), ctx);
    // check result
    assert(name);
    assert(descr);

    // parse field description
    vf_parse_description(descr, &cp_parse->field.f_vlen, NULL);
    // create instruction vectors
    if (stack_with_ref) {
        vf_set_description_vector(descr, cp_parse->field.f_vlen,
                                  1 /* ref + vector */ ,
                                  0, &cp_parse->field.f_vector, NULL, ctx);
        // set first ref
        cp_parse->field.f_vlen += 1;
        vf_ValidType *type = vf_create_class_valid_type(class_name, ctx);
        vf_set_vector_stack_entry_ref(cp_parse->field.f_vector, 0, type);
    } else {
        vf_set_description_vector(descr, cp_parse->field.f_vlen,
                                  0 /* only vector */ ,
                                  0, &cp_parse->field.f_vector, NULL, ctx);
    }
    return VF_OK;
}                               // vf_check_cp_field

/**
 * Checks constant pool Integer, Float and String entries.
 * Sets data flow vector for constant pool entry.
 * @return result of the check
 */
static inline vf_Result vf_check_cp_single_const(unsigned short index,  // constant pool entry index
                                                 vf_Parse * cp_parse,   // entry parse result structure
                                                 vf_Context *ctx)       // verification context
{
    // check constant pool index
    unsigned short len = class_get_cp_size(ctx->m_class);
    CHECK_CONST_POOL_ID(index, len, ctx);
    // create stack map entry
    cp_parse->field.f_vlen = 1;
    vf_new_vector(&(cp_parse->field.f_vector), 1, ctx->m_pool);

    // check constant pool type and set stack map entry
    vf_ValidType *type;
    switch (class_get_cp_tag(ctx->m_class, index)) {
    case _CONSTANT_Integer:
        vf_set_vector_stack_entry_int(cp_parse->field.f_vector, 0);
        break;
    case _CONSTANT_Float:
        vf_set_vector_stack_entry_float(cp_parse->field.f_vector, 0);
        break;
    case _CONSTANT_String:
        type = ctx->m_type->NewType("Ljava/lang/String", 17);
        vf_set_vector_stack_entry_ref(cp_parse->field.f_vector, 0, type);
        break;
    case _CONSTANT_Class:
        if (!vf_is_class_version_14(ctx)) {
            type = ctx->m_type->NewType("Ljava/lang/Class", 16);
            vf_set_vector_stack_entry_ref(cp_parse->field.f_vector, 0, type);
            break;
        }
        // if class version is 1.4 verifier fails in default
    default:
        VF_REPORT(ctx, "Illegal type for constant pool entry #"
                  << index << (vf_is_class_version_14(ctx)
                               ?
                               ": CONSTANT_Integer, CONSTANT_Float or CONSTANT_String"
                               :
                               ": CONSTANT_Integer, CONSTANT_Float, CONSTANT_String "
                               "or CONSTANT_Class")
                  << " are expected for ldc/ldc_w instruction");
        return VF_ErrorConstantPool;
    }
    return VF_OK;
}                               // vf_check_cp_single_const

/**
 * Checks constant pool Double and Long entries.
 * Sets data flow vector for constant pool entry.
 * @return result of the check
 */
static inline vf_Result vf_check_cp_double_const(unsigned short index,  // constant pool entry index
                                                 vf_Parse * cp_parse,   // entry parse result structure
                                                 vf_Context *ctx)       // verification context
{
    // check constant pool index
    unsigned short len = class_get_cp_size(ctx->m_class);
    CHECK_CONST_POOL_ID(index, len, ctx);
    // create stack map entry
    cp_parse->field.f_vlen = 2;
    vf_new_vector(&(cp_parse->field.f_vector), 2, ctx->m_pool);

    // check constant pool type and set stack map entry
    switch (class_get_cp_tag(ctx->m_class, index)) {
    case _CONSTANT_Double:
        vf_set_vector_stack_entry_double(cp_parse->field.f_vector, 0);
        break;
    case _CONSTANT_Long:
        vf_set_vector_stack_entry_long(cp_parse->field.f_vector, 0);
        break;
    default:
        VF_REPORT(ctx, "Illegal type in constant pool, "
                  << index <<
                  ": CONSTANT_Double or CONSTANT_Long are expected");
        return VF_ErrorConstantPool;
    }
    return VF_OK;
}                               // vf_check_cp_double_const

/**
 * Checks constant pool Class entry.
 * Sets data flow vector for constant pool entry (if it's needed).
 * @return result of the check
 */
static vf_Result vf_check_cp_class(unsigned short index,        // constant pool entry index
                                   vf_Parse * cp_parse, // entry parse result structure
                                   vf_Context *ctx)     // verification context
{
    // check constant pool index and type
    unsigned short len = class_get_cp_size(ctx->m_class);
    CHECK_CONST_POOL_ID(index, len, ctx);
    CHECK_CONST_POOL_CLASS(ctx, index);
    if (cp_parse) {
        // create valid type
        const char *name = vf_get_cp_class_name(ctx->m_class, index);
        // check result
        assert(name);

        // create valid type for class
        vf_ValidType *type = vf_create_class_valid_type(name, ctx);
        // create stack map vector
        cp_parse->field.f_vlen = 1;
        vf_new_vector(&cp_parse->field.f_vector, 1, ctx->m_pool);
        vf_set_vector_stack_entry_ref(cp_parse->field.f_vector, 0, type);
    }
    return VF_OK;
}                               // vf_check_cp_class

/**
 * Function parses constant pool entry for given instruction.
 * Sets data flow vectors for current constant pool entry.
 * Function returns parse result.
 */
static vf_Result vf_parse_const_pool(unsigned char instr,       // opcode number
                                     unsigned short index,      // constant pool entry index
                                     vf_Parse * cp_parse,       // entry parse result structure
                                     vf_Context *ctx)   // verification context
{
    vf_Result result;

    switch (instr) {
    case OPCODE_INVOKEVIRTUAL:
        result = vf_check_cp_method(index, cp_parse, false, true, ctx);
        break;
    case OPCODE_INVOKESPECIAL:
        result = vf_check_cp_method(index, cp_parse, false, true, ctx);
        break;
    case OPCODE_INVOKESTATIC:
        result = vf_check_cp_method(index, cp_parse, false, false, ctx);
        break;
    case OPCODE_INVOKEINTERFACE:
        result = vf_check_cp_method(index, cp_parse, true, true, ctx);
        break;
    case OPCODE_GETSTATIC:
    case OPCODE_PUTSTATIC:
        result = vf_check_cp_field(index, cp_parse, false, ctx);
        break;
    case OPCODE_GETFIELD:
    case OPCODE_PUTFIELD:
        result = vf_check_cp_field(index, cp_parse, true, ctx);
        break;
    case OPCODE_LDC:
    case OPCODE_LDC_W:
        result = vf_check_cp_single_const(index, cp_parse, ctx);
        break;
    case OPCODE_LDC2_W:
        result = vf_check_cp_double_const(index, cp_parse, ctx);
        break;
    case OPCODE_NEW:
        result = vf_check_cp_class(index, cp_parse, ctx);
        break;
    case OPCODE_ANEWARRAY:
    case OPCODE_CHECKCAST:
        result = vf_check_cp_class(index, cp_parse, ctx);
        break;
    case OPCODE_INSTANCEOF:
    case OPCODE_MULTIANEWARRAY:
        result = vf_check_cp_class(index, NULL, ctx);
        break;
    default:
        VF_DEBUG("vf_parse_const_pool: incorrect parse of code instruction");
        return VF_ErrorInternal;
    }
    return result;
}                               // vf_parse_const_pool

/************************************************************
 ****************** Bytecode functions **********************
 ************************************************************/

/**
 * Sets instruction structure for opcode aconst_null.
 */
static inline void vf_opcode_aconst_null(vf_Instr *instr,       // code instruction
                                         vf_Pool *pool) // memory pool
{
    vf_set_stack_modifier(instr, 1);
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_ref(instr, 0, NULL);
}                               // vf_opcode_aconst_null

/**
 * Sets instruction structure for opcode iconst_n.
 */
static inline void vf_opcode_iconst_n(vf_Instr *instr,  // code instruction
                                      vf_Pool *pool)    // memory pool
{
    vf_set_stack_modifier(instr, 1);
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_iconst_n

/**
 * Sets instruction structure for opcode lconst_n.
 */
static inline void vf_opcode_lconst_n(vf_Instr *instr,  // code instruction
                                      vf_Pool *pool)    // memory pool
{
    vf_set_stack_modifier(instr, 2);
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_lconst_n

/**
 * Sets instruction structure for opcode fconst_n.
 */
static inline void vf_opcode_fconst_n(vf_Instr *instr,  // code instruction
                                      vf_Pool *pool)    // memory pool
{
    vf_set_stack_modifier(instr, 1);
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_float(instr, 0);
}                               // vf_opcode_fconst_n

/**
 * Sets instruction structure for opcode dconst_n.
 */
static inline void vf_opcode_dconst_n(vf_Instr *instr,  // code instruction
                                      vf_Pool *pool)    // memory pool
{
    vf_set_stack_modifier(instr, 2);
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_double(instr, 0);
}                               // vf_opcode_dconst_n

/**
 * Sets instruction structure for opcodes ldcx.
 */
static inline vf_Result vf_opcode_ldcx(vf_Instr *instr, // code instruction
                                       int modifier,    // instruction stack modifier
                                       unsigned short cp_index, // constant pool entry index
                                       vf_Context *ctx) // verification context
{
    vf_Result result;
    vf_Parse cp_parse = { 0 };

    // check constant pool for instruction
    result = vf_parse_const_pool(*(instr->m_addr), cp_index, &cp_parse, ctx);
    if (VF_OK != result) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, modifier);
    // set stack out vector
    instr->m_outvector = cp_parse.field.f_vector;
    instr->m_outlen = (unsigned short) cp_parse.field.f_vlen;
    return VF_OK;
}                               // vf_opcode_ldcx

/**
 * Sets instruction structure for opcodes iloadx.
 */
static inline void vf_opcode_iloadx(vf_Instr *instr,    // code instruction
                                    unsigned local,     // local variable number
                                    vf_Pool *pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_local_var_int(instr, 0, local);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_iloadx

/**
 * Sets instruction structure for opcodes lloadx.
 */
static inline void vf_opcode_lloadx(vf_Instr *instr,    // code instruction
                                    unsigned local,     // local variable number
                                    vf_Pool *pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_local_var_long(instr, 0, local);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_lloadx

/**
 * Sets instruction structure for opcodes floadx.
 */
static inline void vf_opcode_floadx(vf_Instr *instr,    // code instruction
                                    unsigned local,     // local variable number
                                    vf_Pool *pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_local_var_float(instr, 0, local);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_float(instr, 0);
}                               // vf_opcode_floadx

/**
 * Sets instruction structure for opcodes dloadx.
 */
static inline void vf_opcode_dloadx(vf_Instr *instr,    // code instruction
                                    unsigned local,     // local variable number
                                    vf_Pool *pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_local_var_double(instr, 0, local);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_double(instr, 0);
}                               // vf_opcode_dloadx

/**
 * Sets instruction structure for opcodes aloadx.
 */
static inline void vf_opcode_aloadx(vf_Instr *instr,    // code instruction
                                    unsigned local,     // local variable number
                                    vf_Pool *pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_local_var_ref(instr, 0, NULL, local);
    vf_set_vector_check(instr->m_invector, 0, VF_CHECK_UNINITIALIZED_THIS);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_type(instr, 0, SM_COPY_0);
}                               // vf_opcode_aloadx

/**
 * Sets instruction structure for opcode iaload.
 */
static inline void vf_opcode_iaload(vf_Instr *instr,    // code instruction
                                    vf_ContextHandle ctx)       // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[I", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_iaload

/**
 * Sets instruction structure for opcode baload.
 */
static inline void vf_opcode_baload(vf_Instr *instr,    // code instruction
                                    vf_ContextHandle ctx)       // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[B", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_baload

/**
 * Sets instruction structure for opcode caload.
 */
static inline void vf_opcode_caload(vf_Instr *instr,    // code instruction
                                    vf_ContextHandle ctx)       // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[C", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_caload

/**
 * Sets instruction structure for opcode saload.
 */
static inline void vf_opcode_saload(vf_Instr *instr,    // code instruction
                                    vf_ContextHandle ctx)       // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[S", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_saload

/**
 * Sets instruction structure for opcode laload.
 */
static inline void vf_opcode_laload(vf_Instr *instr,    // code instruction
                                    vf_ContextHandle ctx)       // verification context
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[J", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
    // create out vector
    vf_new_out_vector(instr, 2, ctx->m_pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_laload

/**
 * Sets instruction structure for opcode faload.
 */
static inline void vf_opcode_faload(vf_Instr *instr,    // code instruction
                                    vf_ContextHandle ctx)       // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[F", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_float(instr, 0);
}                               // vf_opcode_faload

/**
 * Sets instruction structure for opcode daload.
 */
static inline void vf_opcode_daload(vf_Instr *instr,    // code instruction
                                    vf_ContextHandle ctx)       // verification context
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[D", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
    // create out vector
    vf_new_out_vector(instr, 2, ctx->m_pool);
    vf_set_out_vector_stack_entry_double(instr, 0);
}                               // vf_opcode_daload

/**
 * Sets instruction structure for opcode aaload.
 */
static inline void vf_opcode_aaload(vf_Instr *instr,    // code instruction
                                    vf_ContextHandle ctx)       // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    // create type
    vf_set_in_vector_stack_entry_ref(instr, 0, NULL);
    vf_set_in_vector_check(instr, 0, VF_CHECK_REF_ARRAY);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_type(instr, 0, SM_UP_ARRAY);
}                               // vf_opcode_aaload

/**
 * Sets instruction structure for opcodes istorex.
 */
static inline void vf_opcode_istorex(vf_Instr *instr,   // code instruction
                                     unsigned local,    // local variable number
                                     vf_Pool *pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_local_var_int(instr, 0, local);
}                               // vf_opcode_istorex

/**
 * Sets instruction structure for opcodes lstorex.
 */
static inline void vf_opcode_lstorex(vf_Instr *instr,   // code instruction
                                     unsigned local,    // local variable number
                                     vf_Pool *pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_local_var_long(instr, 0, local);
}                               // vf_opcode_lstorex

/**
 * Sets instruction structure for opcodes fstorex.
 */
static inline void vf_opcode_fstorex(vf_Instr *instr,   // code instruction
                                     unsigned local,    // local variable number
                                     vf_Pool *pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_float(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_local_var_float(instr, 0, local);
}                               // vf_opcode_fstorex

/**
 * Sets instruction structure for opcodes dstorex.
 */
static inline void vf_opcode_dstorex(vf_Instr *instr,   // code instruction
                                     unsigned local,    // local variable number
                                     vf_Pool *pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_double(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_local_var_double(instr, 0, local);
}                               // vf_opcode_dstorex

/**
 * Sets instruction structure for opcodes astorex.
 */
static inline void vf_opcode_astorex(vf_Instr *instr,   // code instruction
                                     unsigned local,    // local variable number
                                     vf_Pool *pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_ref(instr, 0, NULL);
    vf_set_vector_check(instr->m_invector, 0, VF_CHECK_UNINITIALIZED_THIS);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_local_var_type(instr, 0, SM_COPY_0, local);
}                               // vf_opcode_astorex

/**
 * Sets instruction structure for opcode iastore.
 */
static inline void vf_opcode_iastore(vf_Instr *instr,   // code instruction
                                     vf_ContextHandle ctx)      // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -3);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 3);
    // create in vector
    vf_new_in_vector(instr, 3, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[I", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    vf_set_in_vector_stack_entry_int(instr, 2);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
}                               // vf_opcode_iastore

/**
 * Sets instruction structure for opcode bastore.
 */
static inline void vf_opcode_bastore(vf_Instr *instr,   // code instruction
                                     vf_ContextHandle ctx)      // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -3);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 3);
    // create in vector
    vf_new_in_vector(instr, 3, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[B", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    vf_set_in_vector_stack_entry_int(instr, 2);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
}                               // vf_opcode_bastore

/**
 * Sets instruction structure for opcode castore.
 */
static inline void vf_opcode_castore(vf_Instr *instr,   // code instruction
                                     vf_ContextHandle ctx)      // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -3);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 3);
    // create in vector
    vf_new_in_vector(instr, 3, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[C", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    vf_set_in_vector_stack_entry_int(instr, 2);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
}                               // vf_opcode_castore

/**
 * Sets instruction structure for opcode sastore.
 */
static inline void vf_opcode_sastore(vf_Instr *instr,   // code instruction
                                     vf_ContextHandle ctx)      // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -3);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 3);
    // create in vector
    vf_new_in_vector(instr, 3, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[S", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    vf_set_in_vector_stack_entry_int(instr, 2);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
}                               // vf_opcode_sastore

/**
 * Sets instruction structure for opcode lastore.
 */
static inline void vf_opcode_lastore(vf_Instr *instr,   // code instruction
                                     vf_ContextHandle ctx)      // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -4);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 4);
    // create in vector
    vf_new_in_vector(instr, 4, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[J", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    vf_set_in_vector_stack_entry_long(instr, 2);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
}                               // vf_opcode_lastore

/**
 * Sets instruction structure for opcode fastore.
 */
static inline void vf_opcode_fastore(vf_Instr *instr,   // code instruction
                                     vf_ContextHandle ctx)      // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -3);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 3);
    // create in vector
    vf_new_in_vector(instr, 3, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[F", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    vf_set_in_vector_stack_entry_float(instr, 2);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
}                               // vf_opcode_fastore

/**
 * Sets instruction structure for opcode dastore.
 */
static inline void vf_opcode_dastore(vf_Instr *instr,   // code instruction
                                     vf_ContextHandle ctx)      // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -4);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 4);
    // create in vector
    vf_new_in_vector(instr, 4, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("[D", 2);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
    vf_set_in_vector_stack_entry_int(instr, 1);
    vf_set_in_vector_stack_entry_double(instr, 2);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_EQUAL);
}                               // vf_opcode_dastore

/**
 * Sets instruction structure for opcode aastore.
 */
static inline void vf_opcode_aastore(vf_Instr *instr,   // code instruction
                                     vf_ContextHandle ctx)      // verification context
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -3);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 3);
    // create in vector
    vf_new_in_vector(instr, 3, ctx->m_pool);
    // create type
    vf_set_in_vector_stack_entry_ref(instr, 0, NULL);
    vf_set_in_vector_check(instr, 0, VF_CHECK_REF_ARRAY);
    vf_set_in_vector_stack_entry_int(instr, 1);
    vf_set_in_vector_stack_entry_ref(instr, 2, NULL);
}                               // vf_opcode_aastore

/**
 * Sets instruction structure for opcode pop.
 */
static inline void vf_opcode_pop(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_word(instr, 0);
}                               // vf_opcode_pop

/**
 * Sets instruction structure for opcode pop2.
 */
static inline void vf_opcode_pop2(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_word2(instr, 0);
}                               // vf_opcode_pop2

/**
 * Sets instruction structure for opcode dup.
 */
static inline void vf_opcode_dup(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_word(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_type(instr, 0, SM_COPY_0);
    vf_set_out_vector_type(instr, 1, SM_COPY_0);
}                               // vf_opcode_dup

/**
 * Sets instruction structure for opcode dup_x1.
 */
static inline void vf_opcode_dup_x1(vf_Instr *instr,    // code instruction
                                    vf_Pool *pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_word(instr, 0);
    vf_set_in_vector_stack_entry_word(instr, 1);
    // create out vector
    vf_new_out_vector(instr, 3, pool);
    vf_set_out_vector_type(instr, 0, SM_COPY_1);
    vf_set_out_vector_type(instr, 1, SM_COPY_0);
    vf_set_out_vector_type(instr, 2, SM_COPY_1);
}                               // vf_opcode_dup_x1

/**
 * Sets instruction structure for opcode dup_x2.
 */
static inline void vf_opcode_dup_x2(vf_Instr *instr,    // code instruction
                                    vf_Pool *pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 3);
    // create in vector
    vf_new_in_vector(instr, 3, pool);
    vf_set_in_vector_stack_entry_word2(instr, 0);
    vf_set_in_vector_stack_entry_word(instr, 2);
    // create out vector
    vf_new_out_vector(instr, 4, pool);
    vf_set_out_vector_type(instr, 0, SM_COPY_2);
    vf_set_out_vector_type(instr, 1, SM_COPY_0);
    vf_set_out_vector_type(instr, 2, SM_COPY_1);
    vf_set_out_vector_type(instr, 3, SM_COPY_2);
}                               // vf_opcode_dup_x2

/**
 * Sets instruction structure for opcode dup2.
 */
static inline void vf_opcode_dup2(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_word2(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 4, pool);
    vf_set_out_vector_type(instr, 0, SM_COPY_0);
    vf_set_out_vector_type(instr, 1, SM_COPY_1);
    vf_set_out_vector_type(instr, 2, SM_COPY_0);
    vf_set_out_vector_type(instr, 3, SM_COPY_1);
}                               // vf_opcode_dup2

/**
 * Sets instruction structure for opcode dup2_x1.
 */
static inline void vf_opcode_dup2_x1(vf_Instr *instr,   // code instruction
                                     vf_Pool *pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 3);
    // create in vector
    vf_new_in_vector(instr, 3, pool);
    vf_set_in_vector_stack_entry_word(instr, 0);
    vf_set_in_vector_stack_entry_word2(instr, 1);
    // create out vector
    vf_new_out_vector(instr, 5, pool);
    vf_set_out_vector_type(instr, 0, SM_COPY_1);
    vf_set_out_vector_type(instr, 1, SM_COPY_2);
    vf_set_out_vector_type(instr, 2, SM_COPY_0);
    vf_set_out_vector_type(instr, 3, SM_COPY_1);
    vf_set_out_vector_type(instr, 4, SM_COPY_2);
}                               // vf_opcode_dup2_x1

/**
 * Sets instruction structure for opcode dup2_x2.
 */
static inline void vf_opcode_dup2_x2(vf_Instr *instr,   // code instruction
                                     vf_Pool *pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 4);
    // create in vector
    vf_new_in_vector(instr, 4, pool);
    vf_set_in_vector_stack_entry_word2(instr, 0);
    vf_set_in_vector_stack_entry_word2(instr, 2);
    // create out vector
    vf_new_out_vector(instr, 6, pool);
    vf_set_out_vector_type(instr, 0, SM_COPY_2);
    vf_set_out_vector_type(instr, 1, SM_COPY_3);
    vf_set_out_vector_type(instr, 2, SM_COPY_0);
    vf_set_out_vector_type(instr, 3, SM_COPY_1);
    vf_set_out_vector_type(instr, 4, SM_COPY_2);
    vf_set_out_vector_type(instr, 5, SM_COPY_3);
}                               // vf_opcode_dup2_x2

/**
 * Sets instruction structure for opcode swap.
 */
static inline void vf_opcode_swap(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_word(instr, 0);
    vf_set_in_vector_stack_entry_word(instr, 1);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_type(instr, 0, SM_COPY_1);
    vf_set_out_vector_type(instr, 1, SM_COPY_0);
}                               // vf_opcode_swap

/**
 * Sets instruction structure for opcode iadd.
 */
static inline void vf_opcode_iadd(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_iadd

/**
 * Sets instruction structure for opcode ladd.
 */
static inline void vf_opcode_ladd(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 4);
    // create in vector
    vf_new_in_vector(instr, 4, pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    vf_set_in_vector_stack_entry_long(instr, 2);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_ladd

/**
 * Sets instruction structure for opcode fadd.
 */
static inline void vf_opcode_fadd(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_float(instr, 0);
    vf_set_in_vector_stack_entry_float(instr, 1);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_float(instr, 0);
}                               // vf_opcode_fadd

/**
 * Sets instruction structure for opcode dadd.
 */
static inline void vf_opcode_dadd(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 4);
    // create in vector
    vf_new_in_vector(instr, 4, pool);
    vf_set_in_vector_stack_entry_double(instr, 0);
    vf_set_in_vector_stack_entry_double(instr, 2);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_double(instr, 0);
}                               // vf_opcode_dadd

/**
 * Sets instruction structure for opcode ineg.
 */
static inline void vf_opcode_ineg(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_ineg

/**
 * Sets instruction structure for opcode lneg.
 */
static inline void vf_opcode_lneg(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_lneg

/**
 * Sets instruction structure for opcode fneg.
 */
static inline void vf_opcode_fneg(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_float(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_float(instr, 0);
}                               // vf_opcode_fneg

/**
 * Sets instruction structure for opcode dneg.
 */
static inline void vf_opcode_dneg(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_double(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_double(instr, 0);
}                               // vf_opcode_dneg

/**
 * Sets instruction structure for bit operation opcodes.
 */
static inline void vf_opcode_ibit(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    vf_set_in_vector_stack_entry_int(instr, 1);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_ibit

/**
 * Sets instruction structure for opcodes lshx.
 */
static inline void vf_opcode_lshx(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 3);
    // create in vector
    vf_new_in_vector(instr, 3, pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    vf_set_in_vector_stack_entry_int(instr, 2);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_lshx

/**
 * Sets instruction structure for opcode land.
 */
static inline void vf_opcode_land(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 4);
    // create in vector
    vf_new_in_vector(instr, 4, pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    vf_set_in_vector_stack_entry_long(instr, 2);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_land

/**
 * Sets instruction structure for opcode iinc.
 */
static inline void vf_opcode_iinc(vf_Instr *instr,      // code instruction
                                  unsigned local,       // local variable number
                                  vf_Pool *pool)        // memory pool
{
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_local_var_int(instr, 0, local);
}                               // vf_opcode_iinc

/**
 * Sets instruction structure for opcode i2l.
 */
static inline void vf_opcode_i2l(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_i2l

/**
 * Sets instruction structure for opcode i2f.
 */
static inline void vf_opcode_i2f(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_float(instr, 0);
}                               // vf_opcode_i2f

/**
 * Sets instruction structure for opcode i2d.
 */
static inline void vf_opcode_i2d(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_double(instr, 0);
}                               // vf_opcode_i2d

/**
 * Sets instruction structure for opcode l2i.
 */
static inline void vf_opcode_l2i(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_l2i

/**
 * Sets instruction structure for opcode l2f.
 */
static inline void vf_opcode_l2f(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_float(instr, 0);
}                               // vf_opcode_l2f

/**
 * Sets instruction structure for opcode l2d.
 */
static inline void vf_opcode_l2d(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_double(instr, 0);
}                               // vf_opcode_l2d

/**
 * Sets instruction structure for opcode f2i.
 */
static inline void vf_opcode_f2i(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_float(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_f2i

/**
 * Sets instruction structure for opcode f2l.
 */
static inline void vf_opcode_f2l(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_float(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_f2l

/**
 * Sets instruction structure for opcode f2d.
 */
static inline void vf_opcode_f2d(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_float(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_double(instr, 0);
}                               // vf_opcode_f2d

/**
 * Sets instruction structure for opcode d2i.
 */
static inline void vf_opcode_d2i(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_double(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_d2i

/**
 * Sets instruction structure for opcode d2l.
 */
static inline void vf_opcode_d2l(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_double(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 2, pool);
    vf_set_out_vector_stack_entry_long(instr, 0);
}                               // vf_opcode_d2l

/**
 * Sets instruction structure for opcode d2f.
 */
static inline void vf_opcode_d2f(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_double(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_float(instr, 0);
}                               // vf_opcode_d2f

/**
 * Sets instruction structure for opcode i2b.
 */
static inline void vf_opcode_i2b(vf_Instr *instr,       // code instruction
                                 vf_Pool *pool) // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_i2b

/**
 * Sets instruction structure for opcode lcmp.
 */
static inline void vf_opcode_lcmp(vf_Instr *instr,      // code instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -3);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 4);
    // create in vector
    vf_new_in_vector(instr, 4, pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    vf_set_in_vector_stack_entry_long(instr, 2);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_lcmp

/**
 * Sets instruction structure for opcodes fcmpx.
 */
static inline void vf_opcode_fcmpx(vf_Instr *instr,     // code instruction
                                   vf_Pool *pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_float(instr, 0);
    vf_set_in_vector_stack_entry_float(instr, 1);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_fcmpx

/**
 * Sets instruction structure for opcodes dcmpx.
 */
static inline void vf_opcode_dcmpx(vf_Instr *instr,     // code instruction
                                   vf_Pool *pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -3);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 4);
    // create in vector
    vf_new_in_vector(instr, 4, pool);
    vf_set_in_vector_stack_entry_double(instr, 0);
    vf_set_in_vector_stack_entry_double(instr, 2);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_dcmpx

/**
 * Sets instruction structure for opcode ifeq.
 */
static inline void vf_opcode_ifeq(vf_Instr *instr,      // code instruction
                                  vf_BCode *icode1,     // first branch instruction
                                  vf_BCode *icode2,     // second branch instruction
                                  vf_Pool *pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // set begin of basic block for branch instruction
    icode1->m_is_bb_start = true;
    // set begin of basic block for branch instruction
    icode2->m_is_bb_start = true;
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_ifeq

/**
 * Sets instruction structure for opcode if_icmpeq.
 */
static inline void vf_opcode_if_icmpeq(vf_Instr *instr, // code instruction
                                       vf_BCode *icode1,        // first branch instruction
                                       vf_BCode *icode2,        // second branch instruction
                                       vf_Pool *pool)   // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // set begin of basic block for branch instruction
    icode1->m_is_bb_start = true;
    // set begin of basic block for branch instruction
    icode2->m_is_bb_start = true;
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    vf_set_in_vector_stack_entry_int(instr, 1);
}                               // vf_opcode_if_icmpeq

/**
 * Sets instruction structure for opcode if_acmpeq.
 */
static inline void vf_opcode_if_acmpeq(vf_Instr *instr, // code instruction
                                       vf_BCode *icode1,        // first branch instruction
                                       vf_BCode *icode2,        // second branch instruction
                                       vf_Pool *pool)   // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // set begin of basic block for branch instruction
    icode1->m_is_bb_start = true;
    // set begin of basic block for branch instruction
    icode2->m_is_bb_start = true;
    // create in vector
    vf_new_in_vector(instr, 2, pool);
    vf_set_in_vector_stack_entry_ref(instr, 0, NULL);
    vf_set_in_vector_stack_entry_ref(instr, 1, NULL);
}                               // vf_opcode_if_acmpeq

/**
 * Sets instruction structure for opcode switch.
 */
static inline void vf_opcode_switch(vf_Instr *instr,    // code instruction
                                    vf_Pool *pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_switch

/**
 * Sets in vectors for return instruction.
 */
static inline void vf_opcode_return(vf_Instr *instr,    // a return instruction
                                    unsigned short next_pc,     // a start of the next instruction
                                    vf_Context *ctx)    // verification context
{
    // set instruction flag
    vf_set_instr_type(instr, VF_INSTR_RETURN);
    // create and set edge branch to exit
    vf_set_basic_block_flag(next_pc, ctx);
}                               // vf_opcode_return

/**
 * Sets instruction structure for opcode ireturn.
 */
static inline void vf_opcode_ireturn(vf_Instr *instr,   // a return instruction
                                     unsigned short next_pc,    // a start of the next instruction
                                     vf_Context *ctx)   // verification context
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, ctx->m_pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    // set flag and make checks
    vf_opcode_return(instr, next_pc, ctx);
}                               // vf_opcode_ireturn

/**
 * Sets instruction structure for opcode lreturn.
 */
static inline void vf_opcode_lreturn(vf_Instr *instr,   // a return instruction
                                     unsigned short next_pc,    // a start of the next instruction
                                     vf_Context *ctx)   // verification context
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    vf_set_in_vector_stack_entry_long(instr, 0);
    // set flag and make checks
    vf_opcode_return(instr, next_pc, ctx);
}                               // vf_opcode_lreturn

/**
 * Sets instruction structure for opcode freturn.
 */
static inline void vf_opcode_freturn(vf_Instr *instr,   // a return instruction
                                     unsigned short next_pc,    // a start of the next instruction
                                     vf_Context *ctx)   // verification context
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, ctx->m_pool);
    vf_set_in_vector_stack_entry_float(instr, 0);
    // set flag and make checks
    vf_opcode_return(instr, next_pc, ctx);
}                               // vf_opcode_freturn

/**
 * Sets instruction structure for opcode dreturn.
 */
static inline void vf_opcode_dreturn(vf_Instr *instr,   // a return instruction
                                     unsigned short next_pc,    // a start of the next instruction
                                     vf_Context *ctx)   // verification context
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 2);
    // create in vector
    vf_new_in_vector(instr, 2, ctx->m_pool);
    vf_set_in_vector_stack_entry_double(instr, 0);
    // set flag and make checks
    vf_opcode_return(instr, next_pc, ctx);
}                               // vf_opcode_dreturn

/**
 * Sets instruction structure for opcode areturn.
 */
static inline void vf_opcode_areturn(vf_Instr *instr,   // a return instruction
                                     unsigned short next_pc,    // a start of the next instruction
                                     vf_Context *ctx)   // verification context
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, ctx->m_pool);
    vf_set_in_vector_stack_entry_ref(instr, 0,
                                     (ctx->m_method_outlen) ? ctx->
                                     m_method_outvector->m_vtype : 0);

    // set flag and make checks
    vf_opcode_return(instr, next_pc, ctx);
}                               // vf_opcode_areturn

/**
 * Sets instruction structure for opcode getstatic.
 */
static inline vf_Result vf_opcode_getstatic(vf_Instr *instr,    // code instruction
                                            unsigned short cp_index,    // constant pool entry index
                                            vf_Context *ctx)    // verification context
{
    vf_Result result;
    vf_Parse cp_parse = { 0 };

    // check constant pool for instruction
    result = vf_parse_const_pool(OPCODE_GETSTATIC, cp_index, &cp_parse, ctx);
    if (VF_OK != result) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, cp_parse.field.f_vlen);
    // set stack out vector
    instr->m_outvector = cp_parse.field.f_vector;
    instr->m_outlen = (unsigned short) cp_parse.field.f_vlen;
    return VF_OK;
}                               // vf_opcode_getstatic

/**
 * Sets instruction structure for opcode putstatic.
 */
static inline vf_Result vf_opcode_putstatic(vf_Instr *instr,    // code instruction
                                            unsigned short cp_index,    // constant pool entry index
                                            vf_Context *ctx)    // verification context
{
    vf_Result result;
    vf_Parse cp_parse = { 0 };

    // check constant pool for instruction
    result = vf_parse_const_pool(OPCODE_PUTSTATIC, cp_index, &cp_parse, ctx);
    if (VF_OK != result) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -(int) cp_parse.field.f_vlen);
    // set minimal stack for instruction
    vf_set_min_stack(instr, cp_parse.field.f_vlen);
    // set stack out vector
    instr->m_invector = cp_parse.field.f_vector;
    instr->m_inlen = (unsigned short) cp_parse.field.f_vlen;
    // set assign check
    vf_set_in_vector_check(instr, instr->m_inlen - 1, VF_CHECK_ASSIGN);
    return VF_OK;
}                               // vf_opcode_putstatic

/**
 * Sets instruction structure for opcode getfield.
 */
static inline vf_Result vf_opcode_getfield(vf_Instr *instr,     // code instruction
                                           unsigned short cp_index,     // constant pool entry index
                                           vf_Context *ctx)     // verification context
{
    vf_Result result;
    vf_Parse cp_parse = { 0 };

    // check constant pool for instruction
    result = vf_parse_const_pool(OPCODE_GETFIELD, cp_index, &cp_parse, ctx);
    if (VF_OK != result) {
        return result;
    }
    // set stack modifier for instruction, remove double this reference
    vf_set_stack_modifier(instr, cp_parse.field.f_vlen - 2);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // set stack in vector
    instr->m_invector = cp_parse.field.f_vector;
    instr->m_inlen = 1;
    // set field access check
    vf_set_in_vector_check(instr, 0, VF_CHECK_ACCESS_FIELD);
    vf_set_in_vector_check_index(instr, 0, cp_index);
    // set stack out vector, skip this reference
    instr->m_outvector = cp_parse.field.f_vector + 1;
    instr->m_outlen = (unsigned short) cp_parse.field.f_vlen - 1;
    return VF_OK;
}                               // vf_opcode_getfield

/**
 * Sets instruction structure for opcode putfield.
 */
static inline vf_Result vf_opcode_putfield(vf_Instr *instr,     // code instruction
                                           unsigned short cp_index,     // constant pool entry index
                                           vf_Context *ctx)     // verification context
{
    vf_Result result;
    vf_Parse cp_parse = { 0 };

    // check constant pool for instruction
    result = vf_parse_const_pool(OPCODE_PUTFIELD, cp_index, &cp_parse, ctx);
    if (VF_OK != result) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -cp_parse.field.f_vlen);
    // set minimal stack for instruction
    vf_set_min_stack(instr, cp_parse.field.f_vlen);
    // set stack in vector
    instr->m_invector = cp_parse.field.f_vector;
    instr->m_inlen = (unsigned short) cp_parse.field.f_vlen;
    // set assign check
    vf_set_in_vector_check(instr, instr->m_inlen - 1, VF_CHECK_ASSIGN);
    // set field access check
    vf_set_in_vector_check(instr, 0, VF_CHECK_ACCESS_FIELD);
    vf_set_in_vector_check_index(instr, 0, cp_index);
    return VF_OK;
}                               // vf_opcode_putfield

/**
 * Sets instruction structure for invokes opcodes.
 */
static inline vf_Result vf_opcode_invoke(vf_Instr *instr,       // code instruction
                                         unsigned short cp_index,       // constant pool entry index
                                         vf_Context *ctx)       // verification context
{
    vf_Result result;
    vf_Parse cp_parse = { 0 };

    // check constant pool for instruction
    result = vf_parse_const_pool(*(instr->m_addr), cp_index, &cp_parse, ctx);
    if (VF_OK != result) {
        return result;
    }
    // check method name
    if (cp_parse.method.m_name[0] == '<') {
        VF_REPORT(ctx, "Must call initializers using invokespecial");
        return VF_ErrorConstantPool;
    }
    // check number of arguments
    if (cp_parse.method.m_inlen > 255) {
        VF_REPORT(ctx, "The number of method parameters is limited to 255");
        return VF_ErrorInstruction;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier(instr,
                          cp_parse.method.m_outlen - cp_parse.method.m_inlen);
    // set minimal stack for instruction
    vf_set_min_stack(instr, cp_parse.method.m_inlen);
    // set stack in vector
    instr->m_invector = cp_parse.method.m_invector;
    instr->m_inlen = (unsigned short) cp_parse.method.m_inlen;
    // set stack out vector
    instr->m_outvector = cp_parse.method.m_outvector;
    instr->m_outlen = (unsigned short) cp_parse.method.m_outlen;
    // set method access check for opcode invokevirtual
    if ((*instr->m_addr) == OPCODE_INVOKEVIRTUAL) {
        vf_set_in_vector_check(instr, 0, VF_CHECK_ACCESS_METHOD);
        vf_set_in_vector_check_index(instr, 0, cp_index);
    }
    return VF_OK;
}                               // vf_opcode_invoke


/**
 * Sets instruction structure for invokes opcodes.
 */
static inline vf_Result vf_opcode_invokespecial(vf_Instr *instr,        // code instruction
                                                unsigned short cp_index,        // constant pool entry index
                                                vf_Context *ctx)        // verification context
{
    vf_Result result;
    vf_Parse cp_parse = { 0 };

    // check constant pool for instruction
    result = vf_parse_const_pool(*(instr->m_addr), cp_index, &cp_parse, ctx);
    if (VF_OK != result) {
        return result;
    }
    // check number of arguments
    if (cp_parse.method.m_inlen > 255) {
        VF_REPORT(ctx, "The number of method parameters is limited to 255");
        return VF_ErrorInstruction;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier(instr,
                          cp_parse.method.m_outlen - cp_parse.method.m_inlen);
    // set minimal stack for instruction
    vf_set_min_stack(instr, cp_parse.method.m_inlen);
    // set stack in vector
    instr->m_invector = cp_parse.method.m_invector;
    instr->m_inlen = (unsigned short) cp_parse.method.m_inlen;
    // set stack out vector
    instr->m_outvector = cp_parse.method.m_outvector;
    instr->m_outlen = (unsigned short) cp_parse.method.m_outlen;
    // set method check for opcode invokespecial
    if (!strcmp(cp_parse.method.m_name, "<init>")) {
        // set uninitialized check
        vf_set_in_vector_type(instr, 0, SM_UNINITIALIZED);
        vf_set_in_vector_check(instr, 0, VF_CHECK_DIRECT_SUPER);
    } else {
        // set method access check
        vf_set_in_vector_check(instr, 0, VF_CHECK_INVOKESPECIAL);
    }
    vf_set_in_vector_check_index(instr, 0, cp_index);
    return VF_OK;
}                               // vf_opcode_invokespecial

/**
 * Sets instruction structure for opcode new.
 */
static inline vf_Result vf_opcode_new(vf_Instr *instr,  // code instruction
                                      unsigned short cp_index,  // constant pool entry index
                                      unsigned opcode_new,      // number of opcode new
                                      vf_Context *ctx)  // verification context
{
    vf_Result result;
    vf_Parse cp_parse = { 0 };

    // check constant pool for instruction
    result = vf_parse_const_pool(OPCODE_NEW, cp_index, &cp_parse, ctx);
    if (VF_OK != result) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // check created reference
    assert(cp_parse.field.f_vector->m_vtype->number == 1);
    if (cp_parse.field.f_vector->m_vtype->string[0][0] != 'L') {
        VF_REPORT(ctx, "Illegal creation of array");
        return VF_ErrorInstruction;
    }
    // set stack out vector
    instr->m_outvector = cp_parse.field.f_vector;
    instr->m_outlen = (unsigned short) cp_parse.field.f_vlen;
    // set uninitialized reference
    vf_set_out_vector_type(instr, 0, SM_UNINITIALIZED);
    // set opcode program counter
    vf_set_out_vector_opcode_new(instr, 0, opcode_new);
    return VF_OK;
}                               // vf_opcode_new

/**
 * Sets instruction structure for opcode newarray.
 */
static inline vf_Result vf_opcode_newarray(vf_Instr *instr,     // code instruction
                                           unsigned char type,  // array element type
                                           vf_Context *ctx)     // verification context
{
    vf_ValidType *vtype;

    switch (type) {
    case 4:                    // boolean
    case 8:                    // byte
        vtype = ctx->m_type->NewType("[B", 2);
        break;
    case 5:                    // char
        vtype = ctx->m_type->NewType("[C", 2);
        break;
    case 9:                    // short
        vtype = ctx->m_type->NewType("[S", 2);
        break;
    case 10:                   // int
        vtype = ctx->m_type->NewType("[I", 2);
        break;
    case 6:                    // float
        vtype = ctx->m_type->NewType("[F", 2);
        break;
    case 7:                    // double
        vtype = ctx->m_type->NewType("[D", 2);
        break;
    case 11:                   // long
        vtype = ctx->m_type->NewType("[J", 2);
        break;
    default:
        VF_REPORT(ctx, "Incorrect type in newarray instruction");
        return VF_ErrorInstruction;
    }
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, ctx->m_pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_ref(instr, 0, vtype);
    return VF_OK;
}                               // vf_opcode_newarray

/**
 * Receives valid type string of array by array element name.
 */
static inline const char *vf_get_class_array_valid_type(const char *element_name,       // array element name
                                                        size_t name_len,        // element name length
                                                        unsigned dimension,     // dimension of array
                                                        size_t * result_len,    // pointer to result string length
                                                        vf_Pool *pool)  // memory pool
{
    size_t len;
    unsigned index;
    char *result;

    // create valid type
    if (element_name[0] == '[') {
        // copy array class name
        if (element_name[name_len - 1] == ';') {
            // object type array
            len = name_len + dimension - 1;
            name_len--;
        } else {
            // primitive type array
            len = name_len + dimension;
            switch (element_name[name_len - 1]) {
            case 'Z':
                // set integer array type
                result = (char *) vf_palloc(pool, len + 1);
                for (index = 0; index < dimension; index++) {
                    result[index] = '[';
                }
                memcpy(&result[dimension], element_name, name_len);
                result[len - 1] = 'B';
                *result_len = len;
                return result;
            default:
                // don't need change type
                break;
            }
        }
        result = (char *) vf_palloc(pool, len + 1);
        for (index = 0; index < dimension; index++) {
            result[index] = '[';
        }
        memcpy(&result[dimension], element_name, name_len);
    } else {
        // create class signature
        len = name_len + dimension + 1;
        result = (char *) vf_palloc(pool, len + 1);
        for (index = 0; index < dimension; index++) {
            result[index] = '[';
        }
        result[dimension] = 'L';
        memcpy(&result[dimension + 1], element_name, name_len);
    }
    *result_len = len;
    return result;
}                               // vf_get_class_array_valid_type

/**
 * Sets instruction structure for opcode anewarray.
 */
static inline vf_Result vf_opcode_anewarray(vf_Instr *instr,    // code instruction
                                            unsigned short cp_index,    // constant pool entry index
                                            vf_Context *ctx)    // verification context
{
    // check constant pool for instruction
    vf_Result result = vf_parse_const_pool(OPCODE_ANEWARRAY,
                                           cp_index, NULL, ctx);
    if (VF_OK != result) {
        return result;
    }
    // get array element type name
    const char *name = vf_get_cp_class_name(ctx->m_class, cp_index);
    assert(name);

    // create valid type string
    size_t len;
    const char *array = vf_get_class_array_valid_type(name,
                                                      strlen(name), 1,
                                                      &len, ctx->m_pool);

    // check dimension
    unsigned short index;
    for (index = 0; array[index] == '['; index++) {
        continue;
    }
    if (index > 255) {
        VF_REPORT(ctx, "Array with too many dimensions");
        return VF_ErrorInstruction;
    }
    // create valid type
    vf_ValidType *type = ctx->m_type->NewType(array, len);

    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, ctx->m_pool);
    vf_set_in_vector_stack_entry_int(instr, 0);
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_ref(instr, 0, type);
    return VF_OK;
}                               // vf_opcode_anewarray

/**
 * Sets instruction structure for opcode arraylength.
 */
static inline void vf_opcode_arraylength(vf_Instr *instr,       // code instruction
                                         vf_ContextHandle ctx)  // verification context
{
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, ctx->m_pool);
    // create type
    vf_set_in_vector_stack_entry_ref(instr, 0, NULL);
    // set check
    vf_set_in_vector_check(instr, 0, VF_CHECK_ARRAY);
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
}                               // vf_opcode_arraylength

/**
 * Sets instruction structure for opcode athrow.
 */
static inline void vf_opcode_athrow(vf_Instr *instr,    // code instruction
                                    vf_ContextHandle ctx)       // verification context
{
    // set instruction flag
    vf_set_instr_type(instr, VF_INSTR_THROW);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, ctx->m_pool);
    // create type
    vf_ValidType *type = ctx->m_type->NewType("Ljava/lang/Throwable", 20);
    vf_set_in_vector_stack_entry_ref(instr, 0, type);
}                               // vf_opcode_athrow

/**
 * Sets instruction structure for opcode checkcast.
 */
static inline vf_Result vf_opcode_checkcast(vf_Instr *instr,    // code instruction
                                            unsigned short cp_index,    // constant pool entry index
                                            vf_Context *ctx)    // verification context
{
    vf_Result result;
    vf_Parse cp_parse = { 0 };

    // check constant pool for instruction
    result = vf_parse_const_pool(OPCODE_CHECKCAST, cp_index, &cp_parse, ctx);
    if (VF_OK != result) {
        return result;
    }
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, ctx->m_pool);
    vf_set_in_vector_stack_entry_ref(instr, 0, NULL);
    // set stack out vector
    instr->m_outvector = cp_parse.field.f_vector;
    instr->m_outlen = (unsigned short) cp_parse.field.f_vlen;
    return VF_OK;
}                               // vf_opcode_checkcast

/**
 * Sets instruction structure for opcode instanceof.
 */
static inline vf_Result vf_opcode_instanceof(vf_Instr *instr,   // code instruction
                                             unsigned short cp_index,   // constant pool entry index
                                             vf_Context *ctx)   // verification context
{
    // check constant pool for instruction
    vf_Result result =
        vf_parse_const_pool(OPCODE_INSTANCEOF, cp_index, NULL, ctx);
    if (VF_OK != result) {
        return result;
    }
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, ctx->m_pool);
    vf_set_in_vector_stack_entry_ref(instr, 0, NULL);
    // set stack out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_int(instr, 0);
    return VF_OK;
}                               // vf_opcode_instanceof

/**
 * Sets instruction structure for opcodes monitorx.
 */
static inline void vf_opcode_monitorx(vf_Instr *instr,  // code instruction
                                      vf_Pool *pool)    // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_ref(instr, 0, NULL);
}                               // vf_opcode_monitorx

/**
 * Sets instruction structure for opcode multianewarray.
 */
static inline vf_Result vf_opcode_multianewarray(vf_Instr *instr,       // code instruction
                                                 unsigned short cp_index,       // constant pool entry index
                                                 unsigned char dimensions,      // dimension of array
                                                 vf_Context *ctx)       // verification context
{
    // check constant pool for instruction
    vf_Result result =
        vf_parse_const_pool(OPCODE_MULTIANEWARRAY, cp_index, NULL, ctx);
    if (VF_OK != result) {
        return result;
    }
    // get array element type name
    const char *name = vf_get_cp_class_name(ctx->m_class, cp_index);
    assert(name);

    // get valid type string
    size_t len;
    const char *array = vf_get_class_valid_type(name,
                                                strlen(name),
                                                &len, ctx->m_pool);
    // check dimension
    unsigned short index;
    for (index = 0; array[index] == '['; index++) {
        continue;
    }
    if (index > 255) {
        VF_REPORT(ctx, "Array with too many dimensions");
        return VF_ErrorInstruction;
    }
    if (dimensions == 0 || index < dimensions) {
        VF_REPORT(ctx, "Illegal dimension argument");
        return VF_ErrorInstruction;
    }
    // create valid type
    vf_ValidType *type = ctx->m_type->NewType(array, len);

    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1 - dimensions);
    // set minimal stack for instruction
    vf_set_min_stack(instr, dimensions);
    // create in vector
    vf_new_in_vector(instr, dimensions, ctx->m_pool);
    for (index = 0; index < dimensions; index++) {
        vf_set_in_vector_stack_entry_int(instr, index);
    }
    // create out vector
    vf_new_out_vector(instr, 1, ctx->m_pool);
    vf_set_out_vector_stack_entry_ref(instr, 0, type);
    return VF_OK;
}                               // vf_opcode_multianewarray

/**
 * Sets instruction structure for opcodes ifxnull.
 */
static inline void vf_opcode_ifxnull(vf_Instr *instr,   // code instruction
                                     vf_BCode *icode1,  // first branch instruction
                                     vf_BCode *icode2,  // second branch instruction
                                     vf_Pool *pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, -1);
    // set minimal stack for instruction
    vf_set_min_stack(instr, 1);
    // set begin of basic block for branch instruction
    icode1->m_is_bb_start = true;
    // set begin of basic block for branch instruction
    icode2->m_is_bb_start = true;
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_in_vector_stack_entry_ref(instr, 0, NULL);
}                               // vf_opcode_ifxnull

/**
 * Sets instruction structure for opcodes jsr and jsr_w.
 */
static inline void vf_opcode_jsr(vf_Instr *instr,       // code instruction
                                 unsigned entry_point_pc,       // an pc of subroutine entry point
                                 vf_Pool *pool) // memory pool
{
    // set instruction flag
    vf_set_instr_type(instr, VF_INSTR_JSR);
    // set stack modifier for instruction
    vf_set_stack_modifier(instr, 1);
    // create out vector
    vf_new_out_vector(instr, 1, pool);
    vf_set_vector_stack_entry_addr(instr->m_outvector, 0, entry_point_pc);
}                               // vf_opcode_jsr

/**
 * Sets instruction structure for opcode ret.
 */
static inline void vf_opcode_ret(vf_Instr *instr,       // code instruction
                                 unsigned local,        // local variable number
                                 vf_Pool *pool) // memory pool
{
    // set instruction flag
    vf_set_instr_type(instr, VF_INSTR_RET);
    // create in vector
    vf_new_in_vector(instr, 1, pool);
    vf_set_vector_local_var_addr(instr->m_invector, 0, 0, local);
    // create out vector, so the return address value cannot be reused
    vf_new_out_vector(instr, 1, pool);
    vf_set_out_vector_local_var_type(instr, 0, SM_TOP, local);

}                               // vf_opcode_ret

/**
 * Parses bytecode, determines code instructions, fills instruction and bytecode
 * arrays and provides simple verifications.
 */
static vf_Result vf_parse_bytecode(vf_Context *ctx)
{
    // get bytecode parameters
    unsigned len = ctx->m_len;
    unsigned char *bytecode = ctx->m_bytes;

    /**
     * Allocate memory bytecode annotations. Add additional
     * bytes at the end to simplify basic block marking for
     * <code>jsr</code> instructions and exception handlers.
     * Also this guards against a case when parsing doesn't stop
     * at the end of the method. If bc[len - 1] represents jsr_w,
     * then bc[len + 4] will be marked with basic block end.
     */
    vf_Pool *pool = ctx->m_pool;
    vf_BCode *bc = (vf_BCode *) vf_palloc(pool, (len + GOTO_W_LEN)
                                          * sizeof(vf_BCode));
    ctx->m_bc = bc;

    // first instruction is always begin of basic block
    bc[0].m_is_bb_start = true;

    // allocate memory for instructions
    ctx->m_instr = (vf_InstrHandle) vf_palloc(pool, len * sizeof(vf_Instr));

    int offset;
    unsigned index, count;
    unsigned short local, const_index;
    unsigned char u1;
    vf_Result result = VF_OK;
    unsigned branches;
    unsigned ret_num = 0;

    vf_Instr *instr = (vf_Instr *) ctx->m_instr;

    // parse bytecode instructions and fill a instruction array
    for (index = 0; index < len; instr++) {
        instr->m_addr = &bytecode[index];
        bc[index].m_instr = instr;

        bool wide = (OPCODE_WIDE == bytecode[index]);   /* 0xc4 */
        if (wide) {
            switch (bytecode[++index])  // check the next instruction
            {
            case OPCODE_ILOAD:
            case OPCODE_FLOAD:
            case OPCODE_ALOAD:
            case OPCODE_LLOAD:
            case OPCODE_DLOAD:
            case OPCODE_ISTORE:
            case OPCODE_FSTORE:
            case OPCODE_ASTORE:
            case OPCODE_LSTORE:
            case OPCODE_DSTORE:
            case OPCODE_RET:
            case OPCODE_IINC:
                break;
            default:
                VF_REPORT(ctx,
                          "Instruction wide should be followed "
                          "by iload, fload, aload, lload, dload, istore, fstore, astore, "
                          "lstore, dstore, ret or iinc");
                return VF_ErrorInstruction;
            }
        }
        unsigned prev_index = index;    // remember offset of instruction
        index++;                // skip bytecode

        switch (bytecode[prev_index]) {
        case OPCODE_NOP:       /* 0x00 */
            break;
        case OPCODE_ACONST_NULL:       /* 0x01 */
            vf_opcode_aconst_null(instr, pool);
            break;
        case OPCODE_SIPUSH:    /* 0x11 + s2 */
            index++;            // skip parameter (s2)
        case OPCODE_BIPUSH:    /* 0x10 + s1 */
            index++;            // skip parameter (s1)
        case OPCODE_ICONST_M1: /* 0x02 */
        case OPCODE_ICONST_0:  /* 0x03 */
        case OPCODE_ICONST_1:  /* 0x04 */
        case OPCODE_ICONST_2:  /* 0x05 */
        case OPCODE_ICONST_3:  /* 0x06 */
        case OPCODE_ICONST_4:  /* 0x07 */
        case OPCODE_ICONST_5:  /* 0x08 */
            vf_opcode_iconst_n(instr, pool);
            break;
        case OPCODE_LCONST_0:  /* 0x09 */
        case OPCODE_LCONST_1:  /* 0x0a */
            vf_opcode_lconst_n(instr, pool);
            break;
        case OPCODE_FCONST_0:  /* 0x0b */
        case OPCODE_FCONST_1:  /* 0x0c */
        case OPCODE_FCONST_2:  /* 0x0d */
            vf_opcode_fconst_n(instr, pool);
            break;
        case OPCODE_DCONST_0:  /* 0x0e */
        case OPCODE_DCONST_1:  /* 0x0f */
            vf_opcode_dconst_n(instr, pool);
            break;
        case OPCODE_LDC:       /* 0x12 + u1 */
            // get constant pool index
            const_index = (unsigned short) bytecode[index];
            // skip constant pool index (u1)
            index++;
            result = vf_opcode_ldcx(instr, 1, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_LDC_W:     /* 0x13 + u2 */
        case OPCODE_LDC2_W:    /* 0x14 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result =
                vf_opcode_ldcx(instr, bytecode[prev_index] - OPCODE_LDC,
                               const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_ILOAD:     /* 0x15 + u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_iloadx(instr, local, pool);
            break;
        case OPCODE_LLOAD:     /* 0x16 + u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local + 1, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_lloadx(instr, local, pool);
            break;
        case OPCODE_FLOAD:     /* 0x17 + u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_floadx(instr, local, pool);
            break;
        case OPCODE_DLOAD:     /* 0x18 + u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local + 1, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_dloadx(instr, local, pool);
            break;
        case OPCODE_ALOAD:     /* 0x19 + u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_aloadx(instr, local, pool);
            break;
        case OPCODE_ILOAD_0:   /* 0x1a */
        case OPCODE_ILOAD_1:   /* 0x1b */
        case OPCODE_ILOAD_2:   /* 0x1c */
        case OPCODE_ILOAD_3:   /* 0x1d */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_ILOAD_0);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_iloadx(instr, local, pool);
            break;
        case OPCODE_LLOAD_0:   /* 0x1e */
        case OPCODE_LLOAD_1:   /* 0x1f */
        case OPCODE_LLOAD_2:   /* 0x20 */
        case OPCODE_LLOAD_3:   /* 0x21 */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_LLOAD_0);
            result = vf_check_local_var_number(local + 1, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_lloadx(instr, local, pool);
            break;
        case OPCODE_FLOAD_0:   /* 0x22 */
        case OPCODE_FLOAD_1:   /* 0x23 */
        case OPCODE_FLOAD_2:   /* 0x24 */
        case OPCODE_FLOAD_3:   /* 0x25 */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_FLOAD_0);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_floadx(instr, local, pool);
            break;
        case OPCODE_DLOAD_0:   /* 0x26 */
        case OPCODE_DLOAD_1:   /* 0x27 */
        case OPCODE_DLOAD_2:   /* 0x28 */
        case OPCODE_DLOAD_3:   /* 0x29 */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_DLOAD_0);
            result = vf_check_local_var_number(local + 1, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_dloadx(instr, local, pool);
            break;
        case OPCODE_ALOAD_0:   /* 0x2a */
        case OPCODE_ALOAD_1:   /* 0x2b */
        case OPCODE_ALOAD_2:   /* 0x2c */
        case OPCODE_ALOAD_3:   /* 0x2d */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_ALOAD_0);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_aloadx(instr, local, pool);
            break;
        case OPCODE_IALOAD:    /* 0x2e */
            vf_opcode_iaload(instr, ctx);
            break;
        case OPCODE_BALOAD:    /* 0x33 */
            vf_opcode_baload(instr, ctx);
            break;
        case OPCODE_CALOAD:    /* 0x34 */
            vf_opcode_caload(instr, ctx);
            break;
        case OPCODE_SALOAD:    /* 0x35 */
            vf_opcode_saload(instr, ctx);
            break;
        case OPCODE_LALOAD:    /* 0x2f */
            vf_opcode_laload(instr, ctx);
            break;
        case OPCODE_FALOAD:    /* 0x30 */
            vf_opcode_faload(instr, ctx);
            break;
        case OPCODE_DALOAD:    /* 0x31 */
            vf_opcode_daload(instr, ctx);
            break;
        case OPCODE_AALOAD:    /* 0x32 */
            vf_opcode_aaload(instr, ctx);
            break;
        case OPCODE_ISTORE:    /* 0x36 + u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_istorex(instr, local, pool);
            break;
        case OPCODE_LSTORE:    /* 0x37 + u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local + 1, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_lstorex(instr, local, pool);
            break;
        case OPCODE_FSTORE:    /* 0x38 +  u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_fstorex(instr, local, pool);
            break;
        case OPCODE_DSTORE:    /* 0x39 +  u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local + 1, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_dstorex(instr, local, pool);
            break;
        case OPCODE_ASTORE:    /* 0x3a + u1|u2 */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_astorex(instr, local, pool);
            break;
        case OPCODE_ISTORE_0:  /* 0x3b */
        case OPCODE_ISTORE_1:  /* 0x3c */
        case OPCODE_ISTORE_2:  /* 0x3d */
        case OPCODE_ISTORE_3:  /* 0x3e */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_ISTORE_0);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_istorex(instr, local, pool);
            break;
        case OPCODE_LSTORE_0:  /* 0x3f */
        case OPCODE_LSTORE_1:  /* 0x40 */
        case OPCODE_LSTORE_2:  /* 0x41 */
        case OPCODE_LSTORE_3:  /* 0x42 */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_LSTORE_0);
            result = vf_check_local_var_number(local + 1, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_lstorex(instr, local, pool);
            break;
        case OPCODE_FSTORE_0:  /* 0x43 */
        case OPCODE_FSTORE_1:  /* 0x44 */
        case OPCODE_FSTORE_2:  /* 0x45 */
        case OPCODE_FSTORE_3:  /* 0x46 */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_FSTORE_0);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_fstorex(instr, local, pool);
            break;
        case OPCODE_DSTORE_0:  /* 0x47 */
        case OPCODE_DSTORE_1:  /* 0x48 */
        case OPCODE_DSTORE_2:  /* 0x49 */
        case OPCODE_DSTORE_3:  /* 0x4a */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_DSTORE_0);
            result = vf_check_local_var_number(local + 1, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_dstorex(instr, local, pool);
            break;
        case OPCODE_ASTORE_0:  /* 0x4b */
        case OPCODE_ASTORE_1:  /* 0x4c */
        case OPCODE_ASTORE_2:  /* 0x4d */
        case OPCODE_ASTORE_3:  /* 0x4e */
            // get number of local variable
            local = (unsigned short) (bytecode[prev_index] - OPCODE_ASTORE_0);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_astorex(instr, local, pool);
            break;
        case OPCODE_IASTORE:   /* 0x4f */
            vf_opcode_iastore(instr, ctx);
            break;
        case OPCODE_BASTORE:   /* 0x54 */
            vf_opcode_bastore(instr, ctx);
            break;
        case OPCODE_CASTORE:   /* 0x55 */
            vf_opcode_castore(instr, ctx);
            break;
        case OPCODE_SASTORE:   /* 0x56 */
            vf_opcode_sastore(instr, ctx);
            break;
        case OPCODE_LASTORE:   /* 0x50 */
            vf_opcode_lastore(instr, ctx);
            break;
        case OPCODE_FASTORE:   /* 0x51 */
            vf_opcode_fastore(instr, ctx);
            break;
        case OPCODE_DASTORE:   /* 0x52 */
            vf_opcode_dastore(instr, ctx);
            break;
        case OPCODE_AASTORE:   /* 0x53 */
            vf_opcode_aastore(instr, ctx);
            break;
        case OPCODE_POP:       /* 0x57 */
            vf_opcode_pop(instr, pool);
            break;
        case OPCODE_POP2:      /* 0x58 */
            vf_opcode_pop2(instr, pool);
            break;
        case OPCODE_DUP:       /* 0x59 */
            vf_opcode_dup(instr, pool);
            break;
        case OPCODE_DUP_X1:    /* 0x5a */
            vf_opcode_dup_x1(instr, pool);
            break;
        case OPCODE_DUP_X2:    /* 0x5b */
            vf_opcode_dup_x2(instr, pool);
            break;
        case OPCODE_DUP2:      /* 0x5c */
            vf_opcode_dup2(instr, pool);
            break;
        case OPCODE_DUP2_X1:   /* 0x5d */
            vf_opcode_dup2_x1(instr, pool);
            break;
        case OPCODE_DUP2_X2:   /* 0x5e */
            vf_opcode_dup2_x2(instr, pool);
            break;
        case OPCODE_SWAP:      /* 0x5f */
            vf_opcode_swap(instr, pool);
            break;
        case OPCODE_IADD:      /* 0x60 */
        case OPCODE_ISUB:      /* 0x64 */
        case OPCODE_IMUL:      /* 0x68 */
        case OPCODE_IDIV:      /* 0x6c */
        case OPCODE_IREM:      /* 0x70 */
            vf_opcode_iadd(instr, pool);
            break;
        case OPCODE_LADD:      /* 0x61 */
        case OPCODE_LSUB:      /* 0x65 */
        case OPCODE_LMUL:      /* 0x69 */
        case OPCODE_LDIV:      /* 0x6d */
        case OPCODE_LREM:      /* 0x71 */
            vf_opcode_ladd(instr, pool);
            break;
        case OPCODE_FADD:      /* 0x62 */
        case OPCODE_FSUB:      /* 0x66 */
        case OPCODE_FMUL:      /* 0x6a */
        case OPCODE_FDIV:      /* 0x6e */
        case OPCODE_FREM:      /* 0x72 */
            vf_opcode_fadd(instr, pool);
            break;
        case OPCODE_DADD:      /* 0x63 */
        case OPCODE_DSUB:      /* 0x67 */
        case OPCODE_DMUL:      /* 0x6b */
        case OPCODE_DDIV:      /* 0x6f */
        case OPCODE_DREM:      /* 0x73 */
            vf_opcode_dadd(instr, pool);
            break;
        case OPCODE_INEG:      /* 0x74 */
            vf_opcode_ineg(instr, pool);
            break;
        case OPCODE_LNEG:      /* 0x75 */
            vf_opcode_lneg(instr, pool);
            break;
        case OPCODE_FNEG:      /* 0x76 */
            vf_opcode_fneg(instr, pool);
            break;
        case OPCODE_DNEG:      /* 0x77 */
            vf_opcode_dneg(instr, pool);
            break;
        case OPCODE_ISHL:      /* 0x78 */
        case OPCODE_ISHR:      /* 0x7a */
        case OPCODE_IUSHR:     /* 0x7c */
        case OPCODE_IAND:      /* 0x7e */
        case OPCODE_IOR:       /* 0x80 */
        case OPCODE_IXOR:      /* 0x82 */
            vf_opcode_ibit(instr, pool);
            break;
        case OPCODE_LSHL:      /* 0x79 */
        case OPCODE_LSHR:      /* 0x7b */
        case OPCODE_LUSHR:     /* 0x7d */
            vf_opcode_lshx(instr, pool);
            break;
        case OPCODE_LAND:      /* 0x7f */
        case OPCODE_LOR:       /* 0x81 */
        case OPCODE_LXOR:      /* 0x83 */
            vf_opcode_land(instr, pool);
            break;
        case OPCODE_IINC:      /* 0x84 + u1|u2 + s1|s2 */
            count = index;
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            count = index - count;
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_iinc(instr, local, pool);
            // skip 2nd parameter (s1|s2)
            index += count;
            break;
        case OPCODE_I2L:       /* 0x85 */
            vf_opcode_i2l(instr, pool);
            break;
        case OPCODE_I2F:       /* 0x86 */
            vf_opcode_i2f(instr, pool);
            break;
        case OPCODE_I2D:       /* 0x87 */
            vf_opcode_i2d(instr, pool);
            break;
        case OPCODE_L2I:       /* 0x88 */
            vf_opcode_l2i(instr, pool);
            break;
        case OPCODE_L2F:       /* 0x89 */
            vf_opcode_l2f(instr, pool);
            break;
        case OPCODE_L2D:       /* 0x8a */
            vf_opcode_l2d(instr, pool);
            break;
        case OPCODE_F2I:       /* 0x8b */
            vf_opcode_f2i(instr, pool);
            break;
        case OPCODE_F2L:       /* 0x8c */
            vf_opcode_f2l(instr, pool);
            break;
        case OPCODE_F2D:       /* 0x8d */
            vf_opcode_f2d(instr, pool);
            break;
        case OPCODE_D2I:       /* 0x8e */
            vf_opcode_d2i(instr, pool);
            break;
        case OPCODE_D2L:       /* 0x8f */
            vf_opcode_d2l(instr, pool);
            break;
        case OPCODE_D2F:       /* 0x90 */
            vf_opcode_d2f(instr, pool);
            break;
        case OPCODE_I2B:       /* 0x91 */
        case OPCODE_I2C:       /* 0x92 */
        case OPCODE_I2S:       /* 0x93 */
            vf_opcode_i2b(instr, pool);
            break;
        case OPCODE_LCMP:      /* 0x94 */
            vf_opcode_lcmp(instr, pool);
            break;
        case OPCODE_FCMPL:     /* 0x95 */
        case OPCODE_FCMPG:     /* 0x96 */
            vf_opcode_fcmpx(instr, pool);
            break;
        case OPCODE_DCMPL:     /* 0x97 */
        case OPCODE_DCMPG:     /* 0x98 */
            vf_opcode_dcmpx(instr, pool);
            break;
        case OPCODE_IFEQ:      /* 0x99 + s2 */
        case OPCODE_IFNE:      /* 0x9a + s2 */
        case OPCODE_IFLT:      /* 0x9b + s2 */
        case OPCODE_IFGE:      /* 0x9c + s2 */
        case OPCODE_IFGT:      /* 0x9d + s2 */
        case OPCODE_IFLE:      /* 0x9e + s2 */
            offset = vf_get_double_hword_branch_offset(instr,
                                                       prev_index, bytecode,
                                                       &index, pool);
            result = vf_check_branch_offset(offset, ctx);
            if (VF_OK != result) {
                return result;
            }
            result = vf_check_branch_offset(index, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_ifeq(instr, &bc[offset], &bc[index], pool);
            break;
        case OPCODE_IF_ICMPEQ: /* 0x9f + s2 */
        case OPCODE_IF_ICMPNE: /* 0xa0 + s2 */
        case OPCODE_IF_ICMPLT: /* 0xa1 + s2 */
        case OPCODE_IF_ICMPGE: /* 0xa2 + s2 */
        case OPCODE_IF_ICMPGT: /* 0xa3 + s2 */
        case OPCODE_IF_ICMPLE: /* 0xa4 + s2 */
            offset = vf_get_double_hword_branch_offset(instr,
                                                       prev_index, bytecode,
                                                       &index, pool);
            result = vf_check_branch_offset(offset, ctx);
            if (VF_OK != result) {
                return result;
            }
            result = vf_check_branch_offset(index, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_if_icmpeq(instr, &bc[offset], &bc[index], pool);
            break;
        case OPCODE_IF_ACMPEQ: /* 0xa5 + s2 */
        case OPCODE_IF_ACMPNE: /* 0xa6 + s2 */
            offset = vf_get_double_hword_branch_offset(instr,
                                                       prev_index, bytecode,
                                                       &index, pool);
            result = vf_check_branch_offset(offset, ctx);
            if (VF_OK != result) {
                return result;
            }
            result = vf_check_branch_offset(index, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_if_acmpeq(instr, &bc[offset], &bc[index], pool);
            break;
        case OPCODE_GOTO:      /* 0xa7 + s2 */
            offset = vf_get_single_hword_branch_offset(instr,
                                                       prev_index, bytecode,
                                                       &index, pool);
            result = vf_check_branch_offset(offset, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_set_basic_block_flag(index, ctx);
            break;
        case OPCODE_JSR:       /* 0xa8 + s2 */
            offset = vf_get_single_hword_branch_offset(instr,
                                                       prev_index, bytecode,
                                                       &index, pool);
            result = vf_check_branch_offset(offset, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_jsr(instr, offset, pool);
            vf_set_basic_block_flag(index, ctx);
            break;
        case OPCODE_RET:       /* 0xa9 + u1|u2  */
            local = vf_get_local_var_number(instr, bytecode, &index, wide);
            result = vf_check_local_var_number(local, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_ret(instr, local, pool);
            // create and set edge branch to exit
            vf_set_basic_block_flag(index, ctx);
            ret_num++;
            break;
        case OPCODE_TABLESWITCH:       /* 0xaa + pad + s4 * (3 + N) */
            vf_opcode_switch(instr, pool);
            branches = vf_set_tableswitch_offsets(instr,
                                                  prev_index, &index,
                                                  bytecode, pool);
            // check tableswitch branches and set begin of basic blocks
            for (count = 0; count < branches; count++) {
                offset = vf_get_instr_branch(instr, count);
                result = vf_check_branch_offset(offset, ctx);
                if (VF_OK != result) {
                    return result;
                }
            }
            if (index < len) {
                vf_set_basic_block_flag(index, ctx);
            }
            break;
        case OPCODE_LOOKUPSWITCH:      /* 0xab + pad + s4 * 2 * (N + 1) */
            vf_opcode_switch(instr, pool);
            result = vf_set_lookupswitch_offsets(instr,
                                                 prev_index, &index,
                                                 bytecode, &branches, ctx);
            if (VF_OK != result) {
                return result;
            }
            // check tableswitch branches and set begin of basic blocks
            for (count = 0; count < branches; count++) {
                offset = vf_get_instr_branch(instr, count);
                result = vf_check_branch_offset(offset, ctx);
                if (VF_OK != result) {
                    return result;
                }
            }
            if (index < len) {
                vf_set_basic_block_flag(index, ctx);
            }
            break;
        case OPCODE_IRETURN:   /* 0xac */
            vf_opcode_ireturn(instr, index, ctx);
            break;
        case OPCODE_LRETURN:   /* 0xad */
            vf_opcode_lreturn(instr, index, ctx);
            break;
        case OPCODE_FRETURN:   /* 0xae */
            vf_opcode_freturn(instr, index, ctx);
            break;
        case OPCODE_DRETURN:   /* 0xaf */
            vf_opcode_dreturn(instr, index, ctx);
            break;
        case OPCODE_ARETURN:   /* 0xb0 */
            vf_opcode_areturn(instr, index, ctx);
            break;
        case OPCODE_RETURN:    /* 0xb1 */
            vf_opcode_return(instr, index, ctx);
            break;
        case OPCODE_GETSTATIC: /* 0xb2 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_getstatic(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_PUTSTATIC: /* 0xb3 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_putstatic(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_GETFIELD:  /* 0xb4 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_getfield(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_PUTFIELD:  /* 0xb5 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_putfield(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_INVOKEVIRTUAL:     /* 0xb6 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_invoke(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_INVOKESPECIAL:     /* 0xb7 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_invokespecial(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_INVOKESTATIC:      /* 0xb8 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_invoke(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_INVOKEINTERFACE:   /* 0xb9 + u2 + u1 + u1 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_invoke(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            // check the number of arguments and last opcode byte
            if (instr->m_inlen != (unsigned short) bytecode[index]
                || bytecode[index + 1] != 0) {
                VF_REPORT(ctx, "Incorrect operand byte of invokeinterface");
                return VF_ErrorInstruction;
            }
            // skip 2 parameters (u1 + u1)
            index += 1 + 1;
            break;
        case OPCODE_NEW:       /* 0xbb + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            // zero number of opcode new is reserved for "uninitialized this"
            result =
                vf_opcode_new(instr, const_index,
                              vf_get_instr_index(instr, ctx) + 1, ctx);

            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_NEWARRAY:  /* 0xbc + u1 */
            // get array type
            u1 = (unsigned char) bytecode[index];
            // skip parameter (u1)
            index++;
            result = vf_opcode_newarray(instr, u1, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_ANEWARRAY: /* 0xbd + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_anewarray(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_ARRAYLENGTH:       /* 0xbe */
            vf_opcode_arraylength(instr, ctx);
            break;
        case OPCODE_ATHROW:    /* 0xbf */
            vf_opcode_athrow(instr, ctx);
            // create and set edge branch to exit
            vf_set_basic_block_flag(index, ctx);
            break;
        case OPCODE_CHECKCAST: /* 0xc0 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_checkcast(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_INSTANCEOF:        /* 0xc1 + u2 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_instanceof(instr, const_index, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_MONITORENTER:      /* 0xc2 */
        case OPCODE_MONITOREXIT:       /* 0xc3 */
            vf_opcode_monitorx(instr, pool);
            break;
        case OPCODE_MULTIANEWARRAY:    /* 0xc5 + u2 + u1 */
            // get constant pool index
            const_index =
                (unsigned short) ((bytecode[index] << 8) |
                                  (bytecode[index + 1]));
            // skip constant pool index (u2)
            index += 2;
            // get dimensions of array
            u1 = bytecode[index];
            // skip dimensions of array (u1)
            index++;
            result = vf_opcode_multianewarray(instr, const_index, u1, ctx);
            if (VF_OK != result) {
                return result;
            }
            break;
        case OPCODE_IFNULL:    /* 0xc6 + s2 */
        case OPCODE_IFNONNULL: /* 0xc7 + s2 */
            offset = vf_get_double_hword_branch_offset(instr,
                                                       prev_index, bytecode,
                                                       &index, pool);
            result = vf_check_branch_offset(offset, ctx);
            if (VF_OK != result) {
                return result;
            }
            result = vf_check_branch_offset(index, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_ifxnull(instr, &bc[offset], &bc[index], pool);
            break;
        case OPCODE_GOTO_W:    /* 0xc8 + s4 */
            offset = vf_get_single_word_branch_offset(instr,
                                                      prev_index, bytecode,
                                                      &index, pool);
            result = vf_check_branch_offset(offset, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_set_basic_block_flag(index, ctx);
            break;
        case OPCODE_JSR_W:     /* 0xc9 + s4 */
            offset = vf_get_single_word_branch_offset(instr,
                                                      prev_index, bytecode,
                                                      &index, pool);
            result = vf_check_branch_offset(offset, ctx);
            if (VF_OK != result) {
                return result;
            }
            vf_opcode_jsr(instr, offset, pool);
            vf_set_basic_block_flag(index, ctx);
            break;
        case _OPCODE_UNDEFINED:        /* 0xba */
        default:
            VF_REPORT(ctx, "Unknown bytecode instruction ");
            return VF_ErrorInstruction;
        }
        assert(instr->m_minstack + instr->m_stack >= 0);
    }

    if (index > len) {
        VF_REPORT(ctx, "The last instruction doesn't fit bytecode array");
        return VF_ErrorInstruction;
    }

    /**
     * Set handler basic blocks.
     */
    unsigned short handler_count = ctx->m_handlers;
    unsigned short constLen = class_get_cp_size(ctx->m_class);
    for (unsigned short handler_index = 0;
         handler_index < handler_count; handler_index++) {
        unsigned short start_pc, end_pc, handler_pc, handler_cp_index;
        method_get_exc_handler_info(ctx->m_method,
                                    handler_index, &start_pc, &end_pc,
                                    &handler_pc, &handler_cp_index);
        // check instruction range
        if ((start_pc >= len)
            || (end_pc > len)
            || (handler_pc >= len)) {
            VF_REPORT(ctx, "Exception handler pc is out of range");
            return VF_ErrorHandler;
        }
        if (start_pc >= end_pc) {
            VF_REPORT(ctx,
                      "Exception handler range starting point should be before ending point");
            return VF_ErrorHandler;
        }
        // check that handlers point to instruction
        // boundaries
        if (NULL == bc[start_pc].m_instr
            || NULL == bc[end_pc].m_instr || NULL == bc[handler_pc].m_instr) {
            VF_REPORT(ctx,
                      "At least one of exception handler parameters ["
                      << start_pc << ", " << end_pc << ", " <<
                      handler_pc << "] are out of instruction set");
            return VF_ErrorHandler;
        }
        bc[start_pc].m_is_bb_start = true;
        bc[end_pc].m_is_bb_start = true;
        bc[handler_pc].m_is_bb_start = true;
    }

    // store context values
    ctx->m_last_instr = instr;
    ctx->m_retnum = ret_num;

    VF_DUMP(DUMP_INSTR, vf_dump_bytecode(ctx));
    return result;
}                               // vf_parse_bytecode

#if _VF_DEBUG

/**
 * Prints code instruction array in stream.
 */
void vf_dump_bytecode(vf_ContextHandle ctx)     // verification context
{
    VF_DEBUG
        ("======================== VERIFIER METHOD DUMP ========================");
    VF_DEBUG("Method: " << class_get_name(ctx->m_class)
             << "." << ctx->m_name << ctx->m_descriptor << endl);
    VF_DEBUG("0 [-]: -> START-ENTRY");

    unsigned short handler_count = ctx->m_handlers;
    for (unsigned short handler_index = 0;
         handler_index < handler_count; handler_index++) {
        VF_DEBUG(handler_index +
                 1 << " [-]: -> HANDLER #" << handler_index + 1);
        unsigned short start_pc, end_pc, handler_pc, handler_cp_index;
        method_get_exc_handler_info(ctx->m_method,
                                    handler_index, &start_pc, &end_pc,
                                    &handler_pc, &handler_cp_index);

        VF_DEBUG(" from " << vf_bc_to_instr_index(start_pc, ctx) +
                 handler_count << " [" << start_pc << "]" " to " <<
                 vf_bc_to_instr_index(end_pc,
                                      ctx) +
                 handler_count << " [" << end_pc << "]" " --> " <<
                 vf_bc_to_instr_index(handler_pc,
                                      ctx) +
                 handler_count << " [" << handler_pc << "]" ", CP type "
                 << handler_cp_index);
    }

    unsigned char *bytecode = method_get_bytecode(ctx->m_method);
    vf_InstrHandle instr = ctx->m_instr;
    unsigned index;
    for (index = handler_count + 1;
         instr < ctx->m_last_instr; index++, instr++) {
        VF_DEBUG(index
                 << " [" << instr->m_addr - bytecode << "]:"
                 << ((instr->m_is_bb_start) ? " -> " : "    ")
                 << ((instr->m_stack < 0) ? "" : " ")
                 << instr->m_stack
                 << "|" << instr->m_minstack << "    "
                 << vf_opcode_names[*(instr->m_addr)]);
        for (unsigned count = 0; count < instr->m_offcount; count++) {
            unsigned offset = instr->m_off[count];
            VF_DEBUG(" --> "
                     << vf_bc_to_instr_index(offset,
                                             ctx) +
                     handler_count << " [" << offset << "]");
        }
    }
    VF_DEBUG(index << " [-]: -> END-ENTRY" << endl);
    VF_DEBUG
        ("======================================================================");
}                               // vf_dump_bytecode

#endif //_VF_DEBUG

/**
 * Provides initial verification of class.
 */
vf_Result vf_verify_class(class_handler klass,  // verified class
                          unsigned verifyAll,   // verification level flag
                          char **message)       // verifier error message
{
    assert(klass);
    assert(message);
#if VF_CLASS
    if (strcmp(class_get_name(klass), "")) {
        return VF_OK;
    }
#endif // VF_CLASS

    VF_TRACE("class.bytecode", "verify class: " << class_get_name(klass));

    /**
     * Create context
     */
    vf_Context ctx;

    /**
     * Set current class
     */
    ctx.m_class = klass;

    /**
     * Create type pool
     */
    ctx.m_type = new vf_TypePool ();

    /**
     * Create memory pool
     */
    ctx.m_pool = vf_create_pool();

    /**
     * Set valid types
     */
    const char *class_name = class_get_name(klass);
    size_t class_name_len = strlen(class_name);
    char *type_name = (char *) STD_ALLOCA(class_name_len + 1 + 1);
    // it will be a funny name for array :)
    memcpy(&type_name[1], class_name, class_name_len);
    type_name[0] = 'L';
    type_name[class_name_len + 1] = '\0';
    ctx.m_vtype.m_class = ctx.m_type->NewType(type_name, class_name_len + 1);
    ctx.m_vtype.m_throwable = ctx.m_type->NewType("Ljava/lang/Throwable", 20);
    ctx.m_vtype.m_object = ctx.m_type->NewType("Ljava/lang/Object", 17);
    ctx.m_vtype.m_array = ctx.m_type->NewType("[Ljava/lang/Object", 18);
    ctx.m_vtype.m_clone = ctx.m_type->NewType("Ljava/lang/Cloneable", 20);
    ctx.m_vtype.m_serialize =
        ctx.m_type->NewType("Ljava/io/Serializable", 21);

    /**
     * Set verification level flag
     */
    ctx.m_verify_all = verifyAll ? true : false;

    /**
     * Verify bytecode of methods
     */
    vf_Result result = VF_OK;
    unsigned short number = class_get_method_number(klass);
    for (unsigned short index = 0; index < number; index++) {
#if VF_METHOD
        if (!strcmp(method_get_name(class_get_method(klass, index)), ""))
#endif // VF_METHOD
        {
            /**
             * Set verified method
             */
            ctx.SetMethod(class_get_method(klass, index));
            result = vf_verify_method_bytecode(&ctx);
            ctx.ClearContext();
        }
        if (VF_OK != result) {
            goto labelEnd_verifyClass;
        }
    }

    VF_DUMP(DUMP_CONSTRAINT, ctx.m_type->DumpTypeConstraints(NULL));

    // check and set class constraints
    result = vf_check_class_constraints(&ctx);
    if (VF_OK != result) {
        goto labelEnd_verifyClass;
    }

  labelEnd_verifyClass:
    *message = ctx.m_error;
#if _VF_DEBUG
    if (VF_OK != result) {
        VF_TRACE("", "VerifyError: " << (ctx.m_error ? ctx.m_error : "NULL"));
    }
#endif // _VF_DEBUG

    return result;
}                               // vf_verify_class
