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
#define VERIFY_CLASS           0
// Macro sets verification only for defined method
#define VERIFY_METHOD          0
// Macro dumps type constraints for class
#define VERIFY_DUMP_CONSTRAINT 0
// Macro prints code array in stream
#define PRINT_CODE_ARRAY       0

/**
 * Set namespace Verifier
 */
namespace Verifier {

/**
 * Function parse bytecode, determines code instructions, fills code array and 
 * provides checks of simple verifications.
 */
static Verifier_Result
vf_parse_bytecode( vf_Context_t *context );     // verifier context

#if _VERIFY_DEBUG
/**
 * Function prints code instruction array in stream.
 */
void
vf_dump_bytecode( vf_Context_t *context );     // verifier context

/**
 * Array of opcode names. Available in debug mode.
 */
const char *vf_opcode_names[256] = {
    "NOP", "ACONST_NULL", "ICONST_M1", "ICONST_0", "ICONST_1", "ICONST_2",
    "ICONST_3", "ICONST_4", "ICONST_5", "LCONST_0", "LCONST_1", "FCONST_0",
    "FCONST_1", "FCONST_2", "DCONST_0", "DCONST_1", "BIPUSH", "SIPUSH",
    "LDC", "LDC_W", "LDC2_W", "ILOAD", "LLOAD", "FLOAD", "DLOAD", "ALOAD",
    "ILOAD_0", "ILOAD_1", "ILOAD_2", "ILOAD_3", "LLOAD_0", "LLOAD_1", "LLOAD_2",
    "LLOAD_3", "FLOAD_0", "FLOAD_1", "FLOAD_2", "FLOAD_3", "DLOAD_0", "DLOAD_1",
    "DLOAD_2", "DLOAD_3", "ALOAD_0", "ALOAD_1", "ALOAD_2", "ALOAD_3", "IALOAD",
    "LALOAD", "FALOAD", "DALOAD", "AALOAD", "BALOAD", "CALOAD", "SALOAD",
    "ISTORE", "LSTORE", "FSTORE", "DSTORE", "ASTORE", "ISTORE_0", "ISTORE_1",
    "ISTORE_2", "ISTORE_3", "LSTORE_0", "LSTORE_1", "LSTORE_2", "LSTORE_3",
    "FSTORE_0", "FSTORE_1", "FSTORE_2", "FSTORE_3", "DSTORE_0", "DSTORE_1",
    "DSTORE_2", "DSTORE_3", "ASTORE_0", "ASTORE_1", "ASTORE_2", "ASTORE_3",
    "IASTORE", "LASTORE", "FASTORE", "DASTORE", "AASTORE", "BASTORE", "CASTORE",
    "SASTORE", "POP", "POP2", "DUP", "DUP_X1", "DUP_X2", "DUP2", "DUP2_X1",
    "DUP2_X2", "SWAP", "IADD", "LADD", "FADD", "DADD", "ISUB", "LSUB", "FSUB",
    "DSUB", "IMUL", "LMUL", "FMUL", "DMUL", "IDIV", "LDIV", "FDIV", "DDIV",
    "IREM", "LREM", "FREM", "DREM", "INEG", "LNEG", "FNEG", "DNEG", "ISHL",
    "LSHL", "ISHR", "LSHR", "IUSHR", "LUSHR", "IAND", "LAND", "IOR", "LOR",
    "IXOR", "LXOR", "IINC", "I2L", "I2F", "I2D", "L2I", "L2F", "L2D", "F2I",
    "F2L", "F2D", "D2I", "D2L", "D2F", "I2B", "I2C", "I2S", "LCMP", "FCMPL",
    "FCMPG", "DCMPL", "DCMPG", "IFEQ", "IFNE", "IFLT", "IFGE", "IFGT", "IFLE",
    "IF_ICMPEQ", "IF_ICMPNE", "IF_ICMPLT", "IF_ICMPGE", "IF_ICMPGT",
    "IF_ICMPLE", "IF_ACMPEQ", "IF_ACMPNE", "GOTO", "JSR", "RET", "TABLESWITCH",
    "LOOKUPSWITCH", "IRETURN", "LRETURN", "FRETURN", "DRETURN", "ARETURN",
    "RETURN", "GETSTATIC", "PUTSTATIC", "GETFIELD", "PUTFIELD", "INVOKEVIRTUAL",
    "INVOKESPECIAL", "INVOKESTATIC", "INVOKEINTERFACE", "_OPCODE_UNDEFINED",
    "NEW", "NEWARRAY", "ANEWARRAY", "ARRAYLENGTH", "ATHROW", "CHECKCAST",
    "INSTANCEOF", "MONITORENTER", "MONITOREXIT", "WIDE", "MULTIANEWARRAY",
    "IFNULL", "IFNONNULL", "GOTO_W", "JSR_W",
};
#endif // _VERIFY_DEBUG

/** 
 * Function provides method bytecode verifications.
 */
static Verifier_Result
vf_verify_method_bytecode( vf_Context_t *ctex )     // verifier context
{
    unsigned len;
    Verifier_Result result = VER_OK;

    VERIFY_TRACE( "method", "verify method: " << class_get_name( ctex->m_class )
        << "." << method_get_name( ctex->m_method ) << method_get_descriptor( ctex->m_method ) );

    /**
     * Getting bytecode array and its length
     */
    if( !(len = method_get_code_length( ctex->m_method) ) ) {
        return VER_OK;
    }

    /**
     * Set method for type pool
     */
    ctex->m_type->SetMethod( ctex->m_method );

    /**
     * Allocate memory for code array
     * (+2) for start-entry and end-entry instruction
     */
    ctex->m_code = (vf_Code_t*)vf_alloc_pool_memory( ctex->m_pool, (len + 2
        + method_get_exc_handler_number( ctex->m_method )) * sizeof(vf_Code_t) );

    /**
     * Parse bytecode, fill instruction codeArray
     */
    result = vf_parse_bytecode( ctex );
    if( result != VER_OK ) {
        goto labelEnd_verifyClassBytecode;
    } else if( ctex->m_dump.m_with_subroutine ) {
        result = VER_NoSupportJSR;
        goto labelEnd_verifyClassBytecode;
    }
    
    /**
     * Build bytecode graph
     */
    result = vf_create_graph( ctex );
    if( result != VER_OK ) {
        goto labelEnd_verifyClassBytecode;
    }

    /**
     * Handle graph
     */
    result = vf_graph_checks( ctex );
    if( result != VER_OK ) {
        goto labelEnd_verifyClassBytecode;
    }

labelEnd_verifyClassBytecode:
    /**
     * Free memory
     */
    if( ctex->m_graph ) {
        delete ctex->m_graph;
    }

    return result;
} // vf_verify_method_bytecode

/************************************************************
 ******************** Offset functions **********************
 ************************************************************/

/** 
 * Function creates array of instruction branch offsets.
 */
static inline void
vf_create_instruction_offset( vf_Instr_t *instr,           // given instruction
                              unsigned offcount,           // number of offets
                              vf_VerifyPool_t *pool)      // memory pool
{
    assert( !instr->m_off );
    instr->m_off = (unsigned*)vf_alloc_pool_memory( pool, offcount * sizeof(unsigned) );
    instr->m_offcount = offcount;
    return;
} // vf_create_instruction_offset

/**
 * Function sets instruction branch offset.
 */
static inline void
vf_set_instruction_offset( vf_Instr_t *instr,      // given instruction
                           unsigned offnum,        // offset index in array
                           unsigned value)         // offset value
{
    assert( instr->m_off && offnum < instr->m_offcount );
    instr->m_off[offnum] = value;
    return;
} // vf_set_instruction_offset

/**
 * Function creates branch offset array on 1 element and sets given value.
 */
static inline void
vf_set_single_instruction_offset( vf_Instr_t *instr,    // given instruction    
                          unsigned value,               // offset value
                          vf_VerifyPool_t *pool)        // memory pool
{
    vf_create_instruction_offset( instr, 1, pool );
    vf_set_instruction_offset( instr, 0, value );
    return;
} // vf_set_single_instruction_offset

/************************************************************
 ****************** Modifier functions **********************
 ************************************************************/

/**
 * Function sets stack modifier attribute for given code instruction.
 */
static inline void
vf_set_stack_modifier( vf_Code_t *code,     // code instruction
                       int modify)          // stack modifier value
{
    // set stack modifier for instruction
    code->m_stack = (short)modify;
    return;
} // vf_set_stack_modifier

/**
 * Function sets minimal stack attribute for given code instruction.
 */
static inline void
vf_set_min_stack( vf_Code_t *code,      // code instruction
                  unsigned min_stack)   // minimal stack value
{
    // set minimal stack for instruction
    code->m_minstack = (unsigned short)min_stack;
    return;
} // vf_set_min_stack

/**
 * Function sets basic block attribute for instruction.
 */
static inline void
vf_set_basic_block_flag( vf_Instr_t *code )     // instruction
{
    // set begin of basic block for instruction
    code->m_mark = 1;
    return;
} // vf_set_basic_block_flag

/**
 * Function sets flags for code instruction.
 */
static inline void
vf_set_instruction_flag( vf_Code_t *code,   // code instruction
                         unsigned flag)     // given flags
{
    // set flag for instruction
    code->m_base |= flag;
    return;
} // vf_set_instruction_flag

/**
 * Function checks local number.
 */
static inline Verifier_Result
vf_check_local_var_number( unsigned local,      // local number
                           unsigned maxlocal,   // max local
                           vf_Context_t *ctex)  // verifier context
{
    // check local variable number
    if( local >= maxlocal ) {
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Incorrect usage of local variable" );
        return VER_ErrorLocals;
    }
    return VER_OK;
} // vf_check_local_var_number

/**
 * Function checks branch offset.
 */
static inline Verifier_Result
vf_check_branch_offset( int offset,         // given instruction offset
                        unsigned maxlen,    // bytecode length
                        vf_Context_t *ctex) // verifier context

{
    if( offset < 0 || (unsigned)offset >= maxlen ) {
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Instruction branch offset is out of range" );
        return VER_ErrorBranch;
    }
    return VER_OK;
} // vf_check_branch_offset

/**
 * Function parses local variable number from instruction bytecode.
 */
static inline unsigned
vf_get_local_var_number( vf_Code_t *code,           // code instruction
                         unsigned char *bytecode,   // method bytecode
                         unsigned *index_p)         // index in bytecode array
{
    unsigned local;

    if( (*index_p) != 1 && *((code - 1)->m_addr) == OPCODE_WIDE ) {
        // get number of local variable
        local = (unsigned)( (bytecode[(*index_p)] << 8)|(bytecode[(*index_p)+ 1]) );
        // skip parameter (u2)
        (*index_p) += 2;
    } else {
        // get number of local variable
        local = (unsigned)bytecode[(*index_p)];
        // skip parameter (u1)
        (*index_p)++;
    }
    return local;
} // vf_get_local_var_number

/**
 * Function returns instruction branch offset for given branch number.
 */
static inline int
vf_get_instruction_branch( vf_Instr_t *code,        // instruction
                           unsigned branch_num)     // branch number
{
    assert( branch_num < code->m_offcount );
    return code->m_off[branch_num];
} // vf_get_instruction_branch

/**
 * Function receives half word (2 bytes) instruction branch offset
 * value from bytecode array.
 */
static inline int
vf_get_hword_offset( unsigned code_pc,           // instruction offset in bytecode array
                     unsigned char *bytecode,    // bytecode array
                     unsigned *index_p)          // offset index in bytecode array
{
    // get first branch offset
    int offset = (int)code_pc
        + (short)( (bytecode[(*index_p)] << 8)|(bytecode[(*index_p) + 1]) );
    // skip parameter (s2)
    (*index_p) += 2;
    return offset;
} // vf_get_hword_offset

/**
 * Function receives word (4 bytes) instruction branch offset value from bytecode array.
 */
static inline int
vf_get_word_offset( unsigned code_pc,          // instruction offset in bytecode array
                    unsigned char *bytecode,   // bytecode array
                    unsigned *index_p)         // offset index in bytecode array
{
    // get switch branch offset
    int offset = (int)code_pc
        + (int)( (bytecode[(*index_p)]    << 24)|(bytecode[(*index_p) + 1] << 16)
                |(bytecode[(*index_p) + 2] << 8)|(bytecode[(*index_p) + 3]      ) );
    // skip parameter (s4) 
    (*index_p) += 4;
    return offset;
} // vf_get_word_offset

/**
 * Function receives half word (2 bytes) branch offset from bytecode array,
 * sets into instruction and returns it.
 */
static inline int
vf_get_single_hword_branch_offset( vf_Instr_t *code,        // instruction
                                   unsigned code_pc,        // offset in bytecode array
                                   unsigned char *bytecode, // bytecode array
                                   unsigned *index_p,       // offset index in bytecode array
                                   vf_VerifyPool_t * pool)  // memory pool
{
    // get first branch offset
    int offset = vf_get_hword_offset( code_pc, bytecode, index_p );
    // create and set edge branch for instruction
    vf_set_single_instruction_offset( code, offset, pool );
    return offset;
} // vf_get_single_hword_branch_offset

/**
 * Function receives word (4 bytes) branch offset from bytecode array,
 * sets into instruction and returns it.
 */
static inline int
vf_get_single_word_branch_offset( vf_Instr_t *code,        // instruction
                                  unsigned code_pc,        // offset in bytecode array
                                  unsigned char *bytecode, // bytecode array
                                  unsigned *index_p,       // offset index in bytecode array
                                  vf_VerifyPool_t * pool)  // memory pool
{
    // get first branch offset
    int offset = vf_get_word_offset( code_pc, bytecode, index_p );
    // create and set edge branch for instruction
    vf_set_single_instruction_offset( code, offset, pool );
    return offset;
} // vf_get_single_word_branch_offset

/**
 * Function receives half word (2 bytes) branch offset from bytecode array and
 * sets received offset and next instruction offset into instruction.
 * Function returns received offset.
 */
static inline int
vf_get_double_hword_branch_offset( vf_Instr_t *code,          // instruction
                                   unsigned code_pc,          // instruction offset in bytcode array
                                   unsigned char *bytecode,   // bytecode array
                                   unsigned *index_p,         // offset index in bytecode array
                                   vf_VerifyPool_t *pool)     // memory pool
{
    // get first branch offset
    int offset = vf_get_hword_offset( code_pc, bytecode, index_p );
    // create and set edge branches for instruction
    vf_create_instruction_offset( code, 2, pool );
    // set first edge branch for instruction
    vf_set_instruction_offset( code, 0, offset );
    // set second edge branch for instruction
    vf_set_instruction_offset( code, 1, (*index_p) );
    return offset;
} // vf_get_double_hword_branch_offset

/**
 * Function receives word (4 bytes) branch offset from bytecode array and
 * sets received offset and next instruction offset into instruction.
 * Function returns received offset.
 */
static inline int
vf_get_double_word_branch_offset( vf_Instr_t *code,          // instruction
                                  unsigned code_pc,          // instruction offset in bytcode array
                                  unsigned char *bytecode,   // bytecode array
                                  unsigned *index_p,         // offset index in bytecode array
                                  vf_VerifyPool_t *pool)     // memory pool
{
    // get first branch offset
    int offset = vf_get_word_offset( code_pc, bytecode, index_p );
    // create and set edge branches for instruction
    vf_create_instruction_offset( code, 2, pool );
    // set first edge branch for instruction
    vf_set_instruction_offset( code, 0, offset );
    // set second edge branch for instruction
    vf_set_instruction_offset( code, 1, (*index_p) );
    return offset;
} // vf_get_double_word_branch_offset

/**
 * Function receives tableswitch branch from bytecode array and
 * sets received offset into instruction.
 * Function returns received branch.
 */
static inline int
vf_get_tableswitch_alternative( vf_Instr_t *code,           // instruction
                                unsigned code_pc,           // offset in bytcode array
                                unsigned alternative,       // number of tableswitch branch
                                unsigned char *bytecode,    // bytecode array
                                unsigned *index_p)          // offset index in bytecode array
{
    // get first branch offset
    int offset = vf_get_word_offset( code_pc, bytecode, index_p );
    // set first edge branch for instruction
    vf_set_instruction_offset( code, alternative, offset );
    return offset;
} // vf_get_tableswitch_alternative

/**
 * Function receives tableswitch alternatives from bytecode array and
 * sets them into instruction.
 * Function returns number of alternatives.
 */
static inline int
vf_set_tableswitch_offsets( vf_Instr_t *code,           // instruction
                            unsigned code_pc,           // instruction offset in bytecode array
                            unsigned *index_p,          // offset index in bytecode array
                            unsigned char *bytecode,    // bytecode array
                            vf_VerifyPool_t *pool)      // memory pool
{
    // skip padding
    unsigned default_off = ((*index_p) + 0x3)&(~0x3U);
    unsigned index = default_off;
    // skip default offset
    index += 4;
    // get low and high tableswitch values
    int low = vf_get_word_offset( code_pc, bytecode, &index );
    int high = vf_get_word_offset( code_pc, bytecode, &index );
    int number = high - low + 2;
    // create tableswitch branches
    vf_create_instruction_offset( code, number, pool );
    // set default offset
    vf_get_tableswitch_alternative( code, code_pc, 0, bytecode, &default_off );
    // set another instruction offsets
    for( int count = 1; count < number; count++ ) {
        vf_get_tableswitch_alternative( code, code_pc, count, bytecode, 
                                        &index );
    }
    // set index next instruction
    (*index_p) = index;
    return number;
} // vf_set_tableswitch_offsets

/**
 * Function receives lookupswitch alternatives from bytecode array and
 * sets them into instruction.
 * Function returns number of alternatives.
 */
static inline Verifier_Result
vf_set_lookupswitch_offsets( vf_Instr_t *code,          // instruction
                             unsigned code_pc,          // instruction offset in bytecode
                             unsigned *index_p,         // offset index in bytecode array
                             unsigned char *bytecode,   // array of bytecode
                             unsigned *branch_p,        // number of alternatives
                             vf_Context_t *ctex)        // verifier context
{
    // skip padding
    unsigned default_off = ((*index_p) + 0x3)&(~0x3U);
    unsigned index = default_off;
    // skip default offset
    index += 4;
    // get alternative number of lookupswitch (add default alternative)
    int number = vf_get_word_offset( 0, bytecode, &index ) + 1;
    *branch_p = number;
    // create and tableswitch branches
    vf_create_instruction_offset( code, number, ctex->m_pool );
    // set default offset
    vf_get_tableswitch_alternative( code, code_pc, 0, bytecode, &default_off );
    // set another instruction offsets
    int old_key = INT_MIN;
    for( int count = 1; count < number; count++ ) {
        // get and check branch key value
        int key = vf_get_word_offset( 0, bytecode, &index );
        if( old_key < key ) {
            old_key = key;
        } else if( key != INT_MIN ) {
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class )
                << ", method: " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") Instruction lookupswitch has unsorted key values" );
            return VER_ErrorInstruction;
        }
        // get lookupswitch alternative and set offset to instruction
        vf_get_tableswitch_alternative( code, code_pc, count, bytecode, 
                                        &index );
    }
    // set index next instruction
    (*index_p) = index;
    return VER_OK;
} // vf_set_lookupswitch_offsets

/************************************************************
 ********************* Vector functions *********************
 ************************************************************/

/**
 * Function allocates memory for new stack map vector.
 */
static inline void 
vf_new_vector( vf_MapEntry_t **vector,      // pointer to vector
               unsigned len,                // vector length
               vf_VerifyPool_t *pool)       // memory pool
{
    // create new vector
    (*vector) = (vf_MapEntry_t*)vf_alloc_pool_memory( pool, 
                                    len * sizeof(vf_MapEntry_t) );
    return;
} // vf_new_vector

/**
 * Function sets check type for a given stack map vector entry.
 */
static inline void
vf_set_vector_check( vf_MapEntry_t *vector,         // stack map vector
                     unsigned num,                  // vector entry number
                     vf_CheckConstraint_t check)    // constraint check type
{
    // set check for map vector entry
    assert( check < VF_CHECK_NUM );
    vector[num].m_ctype = check;
    return;
} // vf_set_vector_check

/**
 * Function sets constraint pool index for a given stack map vector entry.
 */
static inline void
vf_set_vector_check_index( vf_MapEntry_t *vector,         // stack map vector
                           unsigned num,                  // vector entry number
                           unsigned short index)          // constraint pool index
{
    // set index for a map vector entry
    vector[num].m_index = index;
    return;
} // vf_set_vector_check_index

/**
 * Function sets a given data type to stack map vector entry.
 */
static inline void
vf_set_vector_type( vf_MapEntry_t *vector,              // stack map vector
                    unsigned num,                       // vector entry number
                    vf_MapType_t type)                  // stack map entry type
{
    assert( type < SM_NUMBER );
    vector[num].m_type = type;
} // vf_set_vector_type

/**
 * Function sets null data type for given stack map vector entry.
 */
static inline void
vf_set_vector_stack_entry_null( vf_MapEntry_t *vector,  // stack map vector
                                unsigned num)           // vector entry number
{
    // set stack map vector entry by null
    vector[num].m_type = SM_NULL;
    return;
} // vf_set_vector_stack_entry_null

/**
 * Function sets int data type for given stack map vector entry.
 */
static inline void
vf_set_vector_stack_entry_int( vf_MapEntry_t *vector,   // stack map vector
                               unsigned num)            // vector entry number
{
    // set stack map vector entry by int
    vector[num].m_type = SM_INT;
    return;
} // vf_set_vector_stack_entry_int

/**
 * Function sets float data type for given stack map vector entry.
 */
static inline void
vf_set_vector_stack_entry_float( vf_MapEntry_t *vector,     // stack map vector
                                 unsigned num)              // vector entry number
{
    // set stack map vector entry by float
    vector[num].m_type = SM_FLOAT;
    return;
} // vf_set_vector_stack_entry_float

/**
 * Function sets long data type for given stack map vector entry.
 */
static inline void
vf_set_vector_stack_entry_long( vf_MapEntry_t *vector,  // stack map vector
                                unsigned num)           // vector entry number
{
    // set stack map vector entry by long
    vector[num].m_type = SM_LONG_HI;
    vector[num + 1].m_type = SM_LONG_LO;
    return;
} // vf_set_vector_stack_entry_long

/**
 * Function sets double data type for given stack map vector entry.
 */
static inline void
vf_set_vector_stack_entry_double( vf_MapEntry_t *vector,    // stack map vector
                                  unsigned num)             // vector entry number
{
    // set stack map vector entry by double
    vector[num].m_type = SM_DOUBLE_HI;
    vector[num + 1].m_type = SM_DOUBLE_LO;
    return;
} // vf_set_vector_stack_entry_double

/**
 * Function sets reference data type for given stack map vector entry.
 */
static inline void
vf_set_vector_stack_entry_ref( vf_MapEntry_t *vector,   // stack map vector
                               unsigned num,            // vector entry number
                               vf_ValidType_t *type)    // reference type
{
    // set stack map vector entry by ref
    vector[num].m_type = SM_REF;
    vector[num].m_vtype = type;
} // vf_set_vector_stack_entry_ref

/**
 * Function sets return address data type for given stack map vector entry.
 */
static inline void
vf_set_vector_stack_entry_addr( vf_MapEntry_t *vector,   // stack map vector
                                unsigned num,            // vector entry number
                                unsigned count)          // program count
{
    // set stack map vector entry by return address
    vector[num].m_type = SM_RETURN_ADDR;
    vector[num].m_pc = count;
} // vf_set_vector_stack_entry_addr

/**
 * Function sets single word data type for given stack map vector entry.
 */
static inline void
vf_set_vector_stack_entry_word( vf_MapEntry_t *vector,  // stack map vector
                                unsigned num)           // vector entry number
{
    // set stack map vector entry by word
    vector[num].m_type = SM_WORD;
    return;
} // vf_set_vector_stack_entry_word

/**
 * Function sets double word data type for given stack map vector entry.
 */
static inline void
vf_set_vector_stack_entry_word2( vf_MapEntry_t *vector,     // stack map vector
                                 unsigned num)              // vector entry number
{
    // set stack map vector entry by double word
    vector[num].m_type = SM_WORD2_HI;
    vector[num + 1].m_type = SM_WORD2_LO;
    return;
} // vf_set_vector_stack_entry_word2

/**
 * Function sets int data type for given local variable vector entry.
 */
static inline void
vf_set_vector_local_var_int( vf_MapEntry_t *vector,     // local variable vector
                             unsigned num,              // vector entry number
                             unsigned local)            // number of local variable
{
    // set local variable vector entry by int
    vector[num].m_type = SM_INT;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short)local;
    return;
} // vf_set_vector_local_var_int

/**
 * Function sets float data type for given local variable vector entry.
 */
static inline void
vf_set_vector_local_var_float( vf_MapEntry_t *vector,   // local variable vector
                               unsigned num,            // vector entry number
                               unsigned local)          // number of local variable
{
    // set local variable vector entry by float
    vector[num].m_type = SM_FLOAT;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short)local;
    return;
} // vf_set_vector_local_var_float

/**
 * Function sets long data type for given local variable vector entry.
 */
static inline void
vf_set_vector_local_var_long( vf_MapEntry_t *vector,    // local variable vector
                              unsigned num,             // vector entry number
                              unsigned local)           // number of local variable
{
    // set local variable vector entry by long
    vector[num].m_type = SM_LONG_HI;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short)local;
    vector[num + 1].m_type = SM_LONG_LO;
    vector[num + 1].m_is_local = true;
    vector[num + 1].m_local = (unsigned short)(local + 1);
    return;
} // vf_set_vector_local_var_long

/**
 * Function sets double data type for given local variable vector entry.
 */
static inline void
vf_set_vector_local_var_double( vf_MapEntry_t *vector,  // local variable vector
                                unsigned num,           // vector entry number
                                unsigned local)         // number of local variable
{
    // set local variable vector entry by double
    vector[num].m_type = SM_DOUBLE_HI;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short)local;
    vector[num + 1].m_type = SM_DOUBLE_LO;
    vector[num + 1].m_is_local = true;
    vector[num + 1].m_local = (unsigned short)(local + 1);
    return;
} // vf_set_vector_local_var_double

/**
 * Function sets reference data type for given local variable vector entry.
 */
static inline void
vf_set_vector_local_var_ref( vf_MapEntry_t *vector,     // local variable vector
                             unsigned num,              // vector entry number
                             vf_ValidType_t *type,      // reference type
                             unsigned local)            // number of local variable
{
    // set stack map vector entry by reference
    vector[num].m_type = SM_REF;
    vector[num].m_vtype = type;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short)local;
    return;
} // vf_set_vector_local_var_ref

/**
 * Function sets return address data type for given local variable vector entry.
 */
static inline void
vf_set_vector_local_var_addr( vf_MapEntry_t *vector,   // stack map vector
                              unsigned num,            // vector entry number
                              unsigned count,          // program count
                              unsigned local)          // number of local variable
{
    // set local variable vector entry to return address
    vector[num].m_type = SM_RETURN_ADDR;
    vector[num].m_pc = count;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short)local;
} // vf_set_vector_local_var_addr

/**
 * Function sets a given data type for a given local variable vector entry.
 */
static inline void
vf_set_vector_local_var_type( vf_MapEntry_t *vector,    // local variable vector
                              unsigned num,             // vector entry number
                              vf_MapType_t type,        // stack map entry type
                              unsigned local)           // number of local variable
{
    // set stack map vector entry by reference
    vector[num].m_type = type;
    vector[num].m_is_local = true;
    vector[num].m_local = (unsigned short)local;
    return;
} // vf_set_vector_local_var_type

/************************************************************
 ****************** IN Vector functions *********************
 ************************************************************/

/**
 * Function allocates memory for new code instruction IN stack map vector.
 */
static inline void 
vf_new_in_vector( vf_Code_t *code,          // code instruction
                  unsigned len,             // vector length
                  vf_VerifyPool_t *pool)    // memory pool
{
    // create IN vector
    code->m_inlen = (unsigned short)len;
    code->m_invector = 
        (vf_MapEntry_t*)vf_alloc_pool_memory( pool, 
                                len * sizeof(vf_MapEntry_t) );
    return;
} // vf_new_in_vector

/**
 * Function sets check attribute for a code instruction IN stack map vector entry.
 */
static inline void
vf_set_in_vector_check( vf_Code_t *code,              // code instruction
                        unsigned num,                 // IN vector entry number
                        vf_CheckConstraint_t check)   // constraint check type
{
    vf_set_vector_check( code->m_invector, num, check );
    return;
} // vf_set_in_vector_check

/**
 * Function sets constant pool index for a code instruction IN stack map vector entry.
 */
static inline void
vf_set_in_vector_check_index( vf_Code_t *code,              // code instruction
                              unsigned num,                 // IN vector entry number
                              unsigned short index)         // constraint pool index
{
    vf_set_vector_check_index( code->m_invector, num, index );
    return;
} // vf_set_in_vector_check_index

/**
 * Function sets a given data type to stack map vector entry.
 */
static inline void
vf_set_in_vector_type( vf_Code_t *code,                     // code instruction
                       unsigned num,                        // vector entry number
                       vf_MapType_t type)                   // stack map entry type
{
    vf_set_vector_type( code->m_invector, num, type );
    return;
} // vf_set_in_vector_type

/**
 * Function sets null data type for code instruction IN stack map vector entry.
 */
static inline void UNUSED
vf_set_in_vector_stack_entry_null( vf_Code_t *code,     // code instruction
                                   unsigned num)        // IN vector entry number
{
    vf_set_vector_stack_entry_null( code->m_invector, num );
    return;
} // vf_set_in_vector_stack_entry_null

/**
 * Function sets int data type for code instruction IN stack map vector entry.
 */
static inline void
vf_set_in_vector_stack_entry_int( vf_Code_t *code,  // code instruction
                                  unsigned num)     // IN vector entry number
{
    vf_set_vector_stack_entry_int( code->m_invector, num );
    return;
} // vf_set_vector_stack_entry_int

/**
 * Function sets float data type for code instruction IN stack map vector entry.
 */
static inline void
vf_set_in_vector_stack_entry_float( vf_Code_t *code,    // code instruction
                                    unsigned num)       // IN vector entry number
{
    vf_set_vector_stack_entry_float( code->m_invector, num );
    return;
} // vf_set_in_vector_stack_entry_float

/**
 * Function sets long data type for code instruction IN stack map vector entry.
 */
static inline void
vf_set_in_vector_stack_entry_long( vf_Code_t *code,     // code instruction
                                   unsigned num)        // IN vector entry number
{
    vf_set_vector_stack_entry_long( code->m_invector, num );
    return;
} // vf_set_in_vector_stack_entry_long

/**
 * Function sets double data type for code instruction IN stack map vector entry.
 */
static inline void
vf_set_in_vector_stack_entry_double( vf_Code_t *code,   // code instruction
                                     unsigned num)      // IN vector entry number
{
    vf_set_vector_stack_entry_double( code->m_invector, num );
    return;
} // vf_set_in_vector_stack_entry_double

/**
 * Function sets reference data type for code instruction IN stack map vector entry.
 */
static inline void
vf_set_in_vector_stack_entry_ref( vf_Code_t *code,          // code instruction
                                  unsigned num,             // IN vector entry number
                                  vf_ValidType_t *type)     // reference type
{
    vf_set_vector_stack_entry_ref( code->m_invector, num, type );
    return;
} // vf_set_in_vector_stack_entry_ref

/**
 * Function sets single word data type for code instruction IN stack map vector entry.
 */
static inline void
vf_set_in_vector_stack_entry_word( vf_Code_t *code,     // code instruction
                                   unsigned num)        // IN vector entry number
{
    vf_set_vector_stack_entry_word( code->m_invector, num );
    return;
} // vf_set_in_vector_stack_entry_word

/**
 * Function sets double word data type for code instruction IN stack map vector entry.
 */
static inline void
vf_set_in_vector_stack_entry_word2( vf_Code_t *code,    // code instruction
                                    unsigned num)       // IN vector entry number
{
    vf_set_vector_stack_entry_word2( code->m_invector, num );
    return;
} // vf_set_in_vector_stack_entry_word2

/**
 * Function sets int data type for code instruction IN local variable vector entry.
 */
static inline void
vf_set_in_vector_local_var_int( vf_Code_t *code,    // code instruction
                                unsigned num,       // IN vector entry number
                                unsigned local)     // local variable number
{
    vf_set_vector_local_var_int( code->m_invector, num, local );
    return;
} // vf_set_in_vector_local_var_int

/**
 * Function sets float data type for code instruction IN local variable vector entry.
 */
static inline void
vf_set_in_vector_local_var_float( vf_Code_t *code,  // code instruction
                                  unsigned num,     // IN vector entry number
                                  unsigned local)   // local variable number
{
    vf_set_vector_local_var_float( code->m_invector, num, local );
    return;
} // vf_set_in_vector_local_var_float 

/**
 * Function sets long data type for code instruction IN local variable vector entry.
 */
static inline void
vf_set_in_vector_local_var_long( vf_Code_t *code,   // code instruction
                                 unsigned num,      // IN vector entry number
                                 unsigned local)    // local variable number
{
    vf_set_vector_local_var_long( code->m_invector, num, local );
    return;
} // vf_set_in_vector_local_var_long 

/**
 * Function sets double data type for code instruction IN local variable vector entry.
 */
static inline void
vf_set_in_vector_local_var_double( vf_Code_t *code,     // code instruction
                                   unsigned num,        // IN vector entry number
                                   unsigned local)      // local variable number
{
    vf_set_vector_local_var_double( code->m_invector, num, local );
    return;
} // vf_set_in_vector_local_var_double 

/**
 * Function sets reference data type for code instruction IN local variable vector entry.
 */
static inline void
vf_set_in_vector_local_var_ref( vf_Code_t *code,                // code instruction
                                unsigned num,                   // IN vector entry number
                                vf_ValidType_t *type,           // reference type
                                unsigned local)                 // local variable number
{
    vf_set_vector_local_var_ref( code->m_invector, num, type, local );
    return;
} // vf_set_in_vector_local_var_ref 

/**
 * Function sets return address data type for code instruction IN local variable vector entry.
 */
static inline void
vf_set_in_vector_local_var_addr( vf_Code_t *code,                // code instruction
                                 unsigned num,                   // IN vector entry number
                                 unsigned count,                 // program count
                                 unsigned local)                 // local variable number
{
    vf_set_vector_local_var_addr( code->m_invector, num, count, local );
    return;
} // vf_set_in_vector_local_var_addr

/**
 * Function sets a given data type for code instruction IN local variable vector entry.
 */
static inline void UNUSED
vf_set_in_vector_local_var_type( vf_Code_t *code,                // code instruction
                                 unsigned num,                   // IN vector entry number
                                 vf_MapType_t type,              // stack map entry
                                 unsigned local)                 // local variable number
{
    vf_set_vector_local_var_type( code->m_invector, num, type, local );
    return;
} // vf_set_in_vector_local_var_type

/************************************************************
 ***************** OUT Vector functions *********************
 ************************************************************/

/**
 * Function allocates memory for new code instruction OUT stack map vector.
 */
static inline void 
vf_new_out_vector( vf_Code_t *code,         // code instruction
                   unsigned len,            // vector length
                   vf_VerifyPool_t *pool)   // memory pool
{
    // create stack map OUT vector
    code->m_outlen = (unsigned short)len;
    code->m_outvector = 
        (vf_MapEntry_t*)vf_alloc_pool_memory( pool, 
                                len * sizeof(vf_MapEntry_t) );
    return;
} // vf_new_out_vector

/**
 * Function sets check attribute for a code instruction OUT stack map vector entry.
 */
static inline void UNUSED
vf_set_out_vector_check( vf_Code_t *code,               // code instruction
                         unsigned num,                  // OUT vector entry number
                         vf_CheckConstraint_t check)    // constraint check type
{
    vf_set_vector_check( code->m_outvector, num, check );
    return;
} // vf_set_out_vector_check

/**
 * Function sets check attribute for code instruction OUT stack map vector entry.
 */
static inline void UNUSED
vf_set_out_vector_check_index( vf_Code_t *code,             // code instruction
                               unsigned num,                // OUT vector entry number
                               unsigned short index)        // constraint pool index
{
    vf_set_vector_check_index( code->m_outvector, num, index );
    return;
} // vf_set_out_vector_check_index

/**
 * Function sets a given data type to stack map OUT vector entry.
 */
static inline void
vf_set_out_vector_type( vf_Code_t *code,                    // code instruction
                        unsigned num,                       // vector entry number
                        vf_MapType_t type)                  // stack map entry type
{
    vf_set_vector_type( code->m_outvector, num, type );
    return;
} // vf_set_out_vector_type

/**
 * Function sets a given program counter to stack map OUT vector entry.
 */
static inline void
vf_set_out_vector_opcode_new( vf_Code_t *code,                  // code instruction
                              unsigned num,                     // vector entry number
                              unsigned opcode_new)              // new opcode
{
    code->m_outvector[num].m_new = opcode_new;
    return;
} // vf_set_out_vector_opcode_new

/**
 * Function sets null data type for code instruction OUT stack map vector entry.
 */
static inline void UNUSED
vf_set_out_vector_stack_entry_null( vf_Code_t *code,    // code instruction
                                    unsigned num)       // OUT vector entry number
{
    vf_set_vector_stack_entry_null( code->m_outvector, num );
    return;
} // vf_set_out_vector_stack_entry_null

/**
 * Function sets int data type for code instruction OUT stack map vector entry.
 */
static inline void
vf_set_out_vector_stack_entry_int( vf_Code_t *code,     // code instruction
                                   unsigned num)        // OUT vector entry number
{
    vf_set_vector_stack_entry_int( code->m_outvector, num );
    return;
} // vf_set_vector_stack_entry_int

/**
 * Function sets float data type for code instruction OUT stack map vector entry.
 */
static inline void
vf_set_out_vector_stack_entry_float( vf_Code_t *code,   // code instruction
                                     unsigned num)      // OUT vector entry number
{
    vf_set_vector_stack_entry_float( code->m_outvector, num );
    return;
} // vf_set_out_vector_stack_entry_float

/**
 * Function sets long data type for code instruction OUT stack map vector entry.
 */
static inline void
vf_set_out_vector_stack_entry_long( vf_Code_t *code,    // code instruction
                                    unsigned num)       // OUT vector entry number
{
    vf_set_vector_stack_entry_long( code->m_outvector, num );
    return;
} // vf_set_out_vector_stack_entry_long

/**
 * Function sets double data type for code instruction OUT stack map vector entry.
 */
static inline void
vf_set_out_vector_stack_entry_double( vf_Code_t *code,  // code instruction
                                      unsigned num)     // OUT vector entry number
{
    vf_set_vector_stack_entry_double( code->m_outvector, num );
    return;
} // vf_set_out_vector_stack_entry_double

/**
 * Function sets reference data type for code instruction OUT stack map vector entry.
 */
static inline void
vf_set_out_vector_stack_entry_ref( vf_Code_t *code,         // code instruction
                                   unsigned num,            // OUT vector entry number
                                   vf_ValidType_t *type)    // reference type
{
    vf_set_vector_stack_entry_ref( code->m_outvector, num, type );
    return;
} // vf_set_out_vector_stack_entry_ref

/**
 * Function sets return address data type for code instruction OUT stack map vector entry.
 */
static inline void
vf_set_out_vector_stack_entry_addr( vf_Code_t *code,          // code instruction
                                    unsigned num,             // OUT vector entry number
                                    unsigned count)           // program count
{
    vf_set_vector_stack_entry_addr( code->m_outvector, num, count );
    return;
} // vf_set_out_vector_stack_entry_addr

/**
 * Function sets int data type for code instruction OUT local variable vector entry.
 */
static inline void
vf_set_out_vector_local_var_int( vf_Code_t *code,   // code instruction
                                 unsigned num,      // OUT vector entry number
                                 unsigned local)    // local variable number
{
    vf_set_vector_local_var_int( code->m_outvector, num, local );
    return;
} // vf_set_out_vector_local_var_int

/**
 * Function sets float data type for code instruction OUT local variable vector entry.
 */
static inline void
vf_set_out_vector_local_var_float( vf_Code_t *code,     // code instruction
                                   unsigned num,        // OUT vector entry number
                                   unsigned local)      // local variable number
{
    vf_set_vector_local_var_float( code->m_outvector, num, local );
    return;
} // vf_set_out_vector_local_var_float 

/**
 * Function sets long data type for code instruction OUT local variable vector entry.
 */
static inline void
vf_set_out_vector_local_var_long( vf_Code_t *code,  // code instruction
                                  unsigned num,     // OUT vector entry number
                                  unsigned local)   // local variable number
{
    vf_set_vector_local_var_long( code->m_outvector, num, local );
    return;
} // vf_set_out_vector_local_var_long 

/**
 * Function sets double data type for code instruction OUT local variable vector entry.
 */
static inline void
vf_set_out_vector_local_var_double( vf_Code_t *code,    // code instruction
                                    unsigned num,       // OUT vector entry number
                                    unsigned local)     // local variable number
{
    vf_set_vector_local_var_double( code->m_outvector, num, local );
    return;
} // vf_set_out_vector_local_var_double 

/**
 * Function sets reference data type for code instruction OUT local variable vector entry.
 */
static inline void UNUSED
vf_set_out_vector_local_var_ref( vf_Code_t *code,       // code instruction
                                 unsigned num,          // OUT vector entry number
                                 vf_ValidType_t *type,  // reference type
                                 unsigned local)        // local variable number
{
    vf_set_vector_local_var_ref( code->m_outvector, num, type, local );
    return;
} // vf_set_out_vector_local_var_ref

/**
 * Function sets a given data type for code instruction OUT local variable vector entry.
 */
static inline void
vf_set_out_vector_local_var_type( vf_Code_t *code,                // code instruction
                                  unsigned num,                   // OUT vector entry number
                                  vf_MapType_t type,              // stack map entry
                                  unsigned local)                 // local variable number
{
    vf_set_vector_local_var_type( code->m_outvector, num, type, local );
    return;
} // vf_set_in_vector_local_var_type

/************************************************************
 ************** Parse description functions *****************
 ************************************************************/
/**
 * Function parses method, class or field descriptions.
 */
void
vf_parse_description( const char *descr,    // descriptor of method, class or field
                      int *inlen,           // returned number of IN descriptor parameters 
                      int *outlen)          // returned number of OUT descriptor
                                            // parameters (for method)
{
    int *count;
    unsigned index;

    /**
     * Check parameters
     */
    assert( descr );
    assert( inlen );

    /**
     * Parse description for defining method stack
     */
    *inlen = 0;
    if( outlen ) {
        *outlen = 0;
    }
    bool array = false;
    for( index = 0, count = inlen; descr[index]; index++ ) {
        switch( descr[index] ) 
        {
        case 'L':
            // skip method name
            do {
                index++;
#if _VERIFY_DEBUG
                if( !descr[index] ) {
                    VERIFY_DEBUG( "vf_parse_description: incorrect structure of constant pool" );
                    vf_error();
                }
#endif // _VERIFY_DEBUG
            } while( descr[index] != ';' );
        case 'B':
        case 'C':
        case 'F':
        case 'I':
        case 'S':
        case 'Z':
            if( !array ) {
                (*count)++;   // increase stack value
            } else {
                array = false;  // reset array structure
            }
        case 'V':
            break;
        case 'D':
        case 'J':
            if( !array ) {
                (*count) += 2;    // increase stack value
            } else {
                array = false;      // reset array structure
            }
            break;
        case '[':
            array = true;
            // skip array structure
            do {
                index++;
#if _VERIFY_DEBUG
                if( !descr[index] ) {
                    VERIFY_DEBUG( "vf_parse_description: incorrect structure of constant pool" );
                    vf_error();
                }
#endif // _VERIFY_DEBUG
            } while( descr[index] == '[' );
            index--;
            (*count)++;   // increase stack value
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
            LDIE(37, "Verifier: vf_parse_description: incorrect structure of constant pool" );
            break;
        }
    }
    return;
} // vf_parse_description

static inline void
vf_set_array_ref( const char *type,
                  unsigned len,
                  vf_MapEntry_t *vector,
                  unsigned *index,
                  vf_Context_t *ctex)
{
    // set array reference entry
    vf_ValidType_t *valid = ctex->m_type->NewType( type, len );
    vf_set_vector_stack_entry_ref( vector, *index, valid );
    (*index)++;
    return;
} // vf_set_array_ref

/**
 * Function parses descriptor and sets input and output data flow vectors.
 */
void
vf_set_description_vector( const char *descr,           // descriptor
                           int inlen,                   // number of entries for IN vector
                           int add,                     // additional number of entries
                                                        // to IN data flow vector
                           int outlen,                  // number of entries for OUT vector
                           vf_MapEntry_t **invector,    // pointer to IN vector
                           vf_MapEntry_t **outvector,   // pointer to OUT vector
                           vf_Context_t *ctex)          // verifier context
{
    const char *type = 0;
    unsigned index,
             count = 0,
             vector_index;
    vf_MapEntry_t **vector;
    vf_ValidType_t *valid;
    vf_VerifyPool_t *pool = ctex->m_pool;

    /**
     * Check parameters
     */
    assert( descr );

    /**
     * Create vectors
     */
    if( inlen + add ) {
        assert( invector );
        vf_new_vector( invector, inlen + add, pool );
    }
    if( outlen ) {
        assert( outvector );
        vf_new_vector( outvector, outlen, pool );
    }

    /**
     * Parse description for fill vectors value
     */
    bool array = false;
    vector_index = add;
    for( index = 0, vector = invector; descr[index]; index++ ) {
        switch( descr[index] ) 
        {
        case 'L':
            // skip method name
            if( !array ) {
                count = index;
                type = &descr[count];
            }
            do {
                index++;
            } while( descr[index] != ';' );
            if( !array ) {
                // set vector reference entry
                valid = ctex->m_type->NewType( type, index - count );
                vf_set_vector_stack_entry_ref( *vector, vector_index, valid );
                vector_index++;
            } else {
                vf_set_array_ref( type, index - count, *vector, &vector_index, ctex );
                // reset array structure
                array = false;
            }
            break;
        case 'Z':
            if( !array ) {
                // set vector int entry
                vf_set_vector_stack_entry_int( *vector, vector_index );
                vector_index++;
            } else {
                // set integer array type
                unsigned iter;
                unsigned len = index - count + 1;
                char *int_array = (char*)vf_alloc_pool_memory( pool, len );
                for( iter = 0; iter < len - 1; iter++ ) {
                    int_array[iter] = '[';
                }
                int_array[iter] = 'B';
                vf_set_array_ref( int_array, len, *vector, &vector_index, ctex );
                // reset array structure
                array = false;
            }
            break;
        case 'I':
        case 'B':
        case 'C':
        case 'S':
            if( !array ) {
                // set vector int entry
                vf_set_vector_stack_entry_int( *vector, vector_index );
                vector_index++;
            } else {
                vf_set_array_ref( type, index - count + 1, *vector, &vector_index, ctex );
                // reset array structure
                array = false;
            }
            break;
        case 'F':
            if( !array ) {
                // set vector float entry
                vf_set_vector_stack_entry_float( *vector, vector_index );
                vector_index++;
            } else {
                vf_set_array_ref( type, index - count + 1, *vector, &vector_index, ctex );
                // reset array structure
                array = false;
            }
            break;
        case 'D':
            if( !array ) {
                // set vector double entry
                vf_set_vector_stack_entry_double( *vector, vector_index );
                vector_index += 2;
            } else {
                vf_set_array_ref( type, index - count + 1, *vector, &vector_index, ctex );
                // reset array structure
                array = false;
            }
            break;
        case 'J':
            if( !array ) {
                // set vector long entry
                vf_set_vector_stack_entry_long( *vector, vector_index );
                vector_index += 2;
            } else {
                vf_set_array_ref( type, index - count + 1, *vector, &vector_index, ctex );
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
            } while( descr[index] == '[' );
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
    return;
} // vf_set_description_vector

/************************************************************
 **************** Constant pool functions *******************
 ************************************************************/

/**
 * Function returns name string from NameAndType constant pool entry.
 */
static inline const char *
vf_get_name_from_cp_nameandtype( unsigned short index,  // constant pool entry index
                                 vf_Context_t *ctex)    // verifier context
{
    // get name constant pool index
    unsigned short name_cp_index = 
        class_get_cp_name_index( ctex->m_class, index );
    // get name string from NameAndType constant pool entry
    const char* name = class_get_cp_utf8_bytes( ctex->m_class, name_cp_index );

    return name;
} // vf_get_name_from_cp_nameandtype

/**
 * Function returns descriptor string from NameAndType constant pool entry.
 */
static inline const char *
vf_get_decriptor_from_cp_nameandtype( unsigned short index,  // constant pool entry index
                                      vf_Context_t *ctex)    // verifier context
{
    // get description constant pool index
    unsigned short descr_cp_index = 
        class_get_cp_descriptor_index( ctex->m_class, index );
    // get descriptor from NameAndType constant pool entry
    const char* descr = class_get_cp_utf8_bytes( ctex->m_class, descr_cp_index );
    
    return descr;
} // vf_get_cp_nameandtype

/**
 * Function receives class name string.
 * Function returns result of constant pool UTF8 entry check and class name string.
 */
static inline const char *
vf_get_cp_class_name( unsigned short index,     // constant pool entry index
                      vf_Context_t *ctex)       // verifier context
{
    unsigned short class_name_index =
        class_get_cp_class_name_index( ctex->m_class, index );
    const char* name = class_get_cp_utf8_bytes( ctex->m_class, class_name_index );
    return name;
} // vf_get_cp_class_name

/**
 * Function returns valid type string by given class name.
 */
static inline const char *
vf_get_class_valid_type( const char *class_name,    // class name
                         unsigned name_len,         // class name length
                         unsigned *len,             // length of created string
                         vf_VerifyPool_t *pool)     // memory pool
{
    char *result;

    // create valid type
    if( class_name[0] == '[' ) {
        // copy array class name
        if( class_name[name_len - 1] == ';' ) {
            // object type array
            *len = name_len - 1;
            result = (char*)class_name;
        } else {
            // primitive type array
            *len = name_len;
            result = (char*)class_name;
            switch( class_name[name_len - 1] )
            {
            case 'Z':
                // set integer array type
                result = (char*)vf_alloc_pool_memory( pool, (*len) + 1 );
                memcpy( result, class_name, name_len );
                result[name_len - 1] = 'B';
                break;
            default:
                // don't need change type
                result = (char*)class_name;
                break;
            }
        }
    } else {
        // create class signature
        *len = name_len + 1;
        result = (char*)vf_alloc_pool_memory( pool, (*len) + 1 );
        result[0] = 'L';
        memcpy( &result[1], class_name, name_len );
    }
    return result;
} // vf_get_class_valid_type

/**
 * Function creates valid type by given class name.
 */
vf_ValidType_t *
vf_create_class_valid_type( const char *class_name,     // class name
                            vf_Context_t *ctex)         // verifier context
{
    unsigned len;
    vf_ValidType_t *result;

    // get class valid type
    unsigned class_name_len = strlen( class_name );
    const char *type = vf_get_class_valid_type( class_name, class_name_len,
                                                &len, ctex->m_pool );
    // create valid type
    result = ctex->m_type->NewType( type, len );
    return result;
} // vf_create_class_valid_type

/**
 * Function checks constant pool reference entry.
 * Function returns result of check and
 * name string, type descriptor string and class name string (if it's needed).
 */
static inline void
vf_check_cp_ref( unsigned short index,      // constant pool entry index
                 const char **name,         // pointer to name
                 const char **descr,        // pointer to descriptor
                 const char **class_name,   // pointer to class name
                 vf_Context_t *ctex)        // verifier context
{
    assert(name);
    assert(descr);

    // get class name if it's needed
    if( class_name ) {
        unsigned short class_cp_index = 
            class_get_cp_ref_class_index( ctex->m_class, index );
        *class_name = vf_get_cp_class_name( class_cp_index, ctex );
    }

    // get name and descriptor from NameAndType constant pool entry
    unsigned short name_and_type_cp_index = 
        class_get_cp_ref_name_and_type_index( ctex->m_class, index );
    *name = vf_get_name_from_cp_nameandtype( name_and_type_cp_index, ctex );
    *descr = vf_get_decriptor_from_cp_nameandtype( name_and_type_cp_index, ctex );
    return;
} // vf_check_cp_ref

/**
 * Function checks constant pool Methodref and InterfaceMethodref entries.
 * Function sets IN and OUT vectors for constant pool entries.
 * Function returns result of check.
 */
static Verifier_Result
vf_check_cp_method( unsigned short index,       // constant pool entry index
                    vf_Parse_t *cp_parse,       // entry parse result structure
                    bool is_interface,          // interface flag
                    bool stack_with_ref,        // stack with this reference flag
                    vf_Context_t *ctex)         // verifier context
{
    // get constant pool length
    unsigned short len = class_get_cp_size( ctex->m_class );

    // check constant pool index
    CHECK_CONST_POOL_ID( index, len, ctex );
    // check constant pool type
    if( is_interface ) {
        CHECK_CONST_POOL_INTERFACE( ctex, index );
    } else {
        CHECK_CONST_POOL_METHOD( ctex, index );
    }

    // get method description and name
    const char* name = NULL;
    const char* descr = NULL;
    const char* class_name;
    vf_check_cp_ref( index, &name, &descr, 
        (stack_with_ref ? &class_name : NULL), ctex );
    // check result
    assert( name );
    assert( descr );

    // set method name
    cp_parse->method.m_name = name;

    // parse method description
    vf_parse_description( descr, &cp_parse->method.m_inlen, &cp_parse->method.m_outlen );
    // create instruction vectors
    if( stack_with_ref ) {
        vf_set_description_vector( descr, cp_parse->method.m_inlen, 1 /* ref + vector */, 
            cp_parse->method.m_outlen, &cp_parse->method.m_invector,
            &cp_parse->method.m_outvector, ctex);
        // set first ref
        cp_parse->method.m_inlen += 1;
        vf_ValidType_t *type = vf_create_class_valid_type( class_name, ctex );
        vf_set_vector_stack_entry_ref( cp_parse->method.m_invector, 0, type );
    } else {
        vf_set_description_vector( descr, cp_parse->method.m_inlen, 0 /* only vector */, 
            cp_parse->method.m_outlen, &cp_parse->method.m_invector,
            &cp_parse->method.m_outvector, ctex);
    }
    return VER_OK;
} // vf_check_cp_method

/**
 * Function checks constant pool Fieldref entry.
 * Function sets data flow vector for constant pool entry.
 * Function returns result of check.
 */
static Verifier_Result
vf_check_cp_field( unsigned short index,    // constant pool entry index
                   vf_Parse_t *cp_parse,    // entry parse result structure
                   bool stack_with_ref,     // stack with this reference flag
                   vf_Context_t *ctex)      // verifier context
{
    // check constant pool index and type
    unsigned short len = class_get_cp_size( ctex->m_class );
    CHECK_CONST_POOL_ID( index, len, ctex );
    CHECK_CONST_POOL_FIELD( ctex, index );

    // get field description and name
    const char* name = NULL;
    const char* descr = NULL;
    const char* class_name;
    vf_check_cp_ref( index, &name, &descr,
        (stack_with_ref ? &class_name : NULL), ctex );
    // check result
    assert( name );
    assert( descr );

    // parse field description
    vf_parse_description( descr, &cp_parse->field.f_vlen, NULL );
    // create instruction vectors
    if( stack_with_ref ) {
        vf_set_description_vector( descr, cp_parse->field.f_vlen, 1 /* ref + vector */, 
            0, &cp_parse->field.f_vector, NULL, ctex);
        // set first ref
        cp_parse->field.f_vlen += 1;
        vf_ValidType_t *type = vf_create_class_valid_type( class_name, ctex );
        vf_set_vector_stack_entry_ref( cp_parse->field.f_vector, 0, type );
    } else {
        vf_set_description_vector( descr, cp_parse->field.f_vlen, 0 /* only vector */, 
            0, &cp_parse->field.f_vector, NULL, ctex);
    }
    return VER_OK;
} // vf_check_cp_field

/**
 * Function checks constant pool Integer, Float and String entries.
 * Function sets data flow vector for constant pool entry.
 * Function returns result of check.
 */
static inline Verifier_Result
vf_check_cp_single_const( unsigned short index,     // constant pool entry index
                          vf_Parse_t *cp_parse,     // entry parse result structure
                          vf_Context_t *ctex)       // verifier context
{
    // check constant pool index
    unsigned short len = class_get_cp_size( ctex->m_class );
    CHECK_CONST_POOL_ID( index, len, ctex );
    // create stack map entry
    cp_parse->field.f_vlen = 1;
    vf_new_vector( &(cp_parse->field.f_vector), 1, ctex->m_pool );

    // check constant pool type and set stack map entry
    vf_ValidType_t *type;
    switch( class_get_cp_tag( ctex->m_class, index ) ) 
    {
    case _CONSTANT_Integer:
        vf_set_vector_stack_entry_int( cp_parse->field.f_vector, 0 );
        break;
    case _CONSTANT_Float:
        vf_set_vector_stack_entry_float( cp_parse->field.f_vector, 0 );
        break;
    case _CONSTANT_String:
        type = ctex->m_type->NewType( "Ljava/lang/String", 17 );
        vf_set_vector_stack_entry_ref( cp_parse->field.f_vector, 0, type );
        break;
	case _CONSTANT_Class:
        if( !vf_is_class_version_14(ctex) ) {
            type = ctex->m_type->NewType( "Ljava/lang/Class", 16 );
            vf_set_vector_stack_entry_ref( cp_parse->field.f_vector, 0, type );
		    break;
        }
        // if class version is 1.4 verifier fails in default
    default:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class )
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Illegal type for constant pool entry #"
            << index
            << (vf_is_class_version_14(ctex)
                ? ": CONSTANT_Integer, CONSTANT_Float or CONSTANT_String"
                : ": CONSTANT_Integer, CONSTANT_Float, CONSTANT_String "
                  "or CONSTANT_Class")
            << " are expected for ldc/ldc_w instruction" );
        return VER_ErrorConstantPool;
    }
    return VER_OK;
} // vf_check_cp_single_const

/**
 * Function checks constant pool Double and Long entries.
 * Function sets data flow vector for constant pool entry.
 * Function returns result of check.
 */
static inline Verifier_Result
vf_check_cp_double_const( unsigned short index,     // constant pool entry index
                          vf_Parse_t *cp_parse,     // entry parse result structure
                          vf_Context_t *ctex)       // verifier context
{
    // check constant pool index
    unsigned short len = class_get_cp_size( ctex->m_class );
    CHECK_CONST_POOL_ID( index, len, ctex );
    // create stack map entry
    cp_parse->field.f_vlen = 2;
    vf_new_vector( &(cp_parse->field.f_vector), 2, ctex->m_pool );
    
    // check constant pool type and set stack map entry
    switch( class_get_cp_tag( ctex->m_class, index ) ) 
    {
    case _CONSTANT_Double:
        vf_set_vector_stack_entry_double( cp_parse->field.f_vector, 0 );
        break;
    case _CONSTANT_Long:
        vf_set_vector_stack_entry_long( cp_parse->field.f_vector, 0 );
        break;
    default:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class )
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Illegal type in constant pool,"
            << index << ": CONSTANT_Double or CONSTANT_Long are expected" );
        return VER_ErrorConstantPool;
    }
    return VER_OK;
} // vf_check_cp_double_const

/**
 * Function checks constant pool Class entry.
 * Function sets data flow vector for constant pool entry (if it's needed).
 * Function returns result of check.
 */
static Verifier_Result
vf_check_cp_class( unsigned short index,        // constant pool entry index
                   vf_Parse_t *cp_parse,        // entry parse result structure
                   vf_Context_t *ctex)          // verifier context
{
    // check constant pool index and type
    unsigned short len = class_get_cp_size( ctex->m_class );
    CHECK_CONST_POOL_ID( index, len, ctex );
    CHECK_CONST_POOL_CLASS( ctex, index );
    if( cp_parse ) {
        // create valid type
        const char *name = vf_get_cp_class_name( index, ctex );
        // check result
        assert( name );

        // create valid type for class
        vf_ValidType_t *type = vf_create_class_valid_type( name, ctex );
        // create stack map vector
        cp_parse->field.f_vlen = 1;
        vf_new_vector( &cp_parse->field.f_vector, 1, ctex->m_pool );
        vf_set_vector_stack_entry_ref( cp_parse->field.f_vector, 0, type );
    }
    return VER_OK;
} // vf_check_cp_class

/**
 * Function parses constant pool entry for given instruction.
 * Function sets data flow vectors for current constant pool entry.
 * Function returns parse result.
 */
static Verifier_Result
vf_parse_const_pool( unsigned char code,        // opcode number
                     unsigned short index,      // constant pool entry index
                     vf_Parse_t *cp_parse,      // entry parse result structure
                     vf_Context_t *ctex)        // verifier context
{
    Verifier_Result result;

    switch( code )
    {
    case OPCODE_INVOKEVIRTUAL:
        result = vf_check_cp_method( index, cp_parse, false, true, ctex );
        break;
    case OPCODE_INVOKESPECIAL:
        result = vf_check_cp_method( index, cp_parse, false, true, ctex );
        break;
    case OPCODE_INVOKESTATIC:
        result = vf_check_cp_method( index, cp_parse, false, false, ctex );
        break;
    case OPCODE_INVOKEINTERFACE:
        result = vf_check_cp_method( index, cp_parse, true, true, ctex );
        break;
    case OPCODE_GETSTATIC:
    case OPCODE_PUTSTATIC:
        result = vf_check_cp_field( index, cp_parse, false, ctex );
        break;
    case OPCODE_GETFIELD:
    case OPCODE_PUTFIELD:
        result = vf_check_cp_field( index, cp_parse, true, ctex );
        break;
    case OPCODE_LDC:
    case OPCODE_LDC_W:
        result = vf_check_cp_single_const( index, cp_parse, ctex );
        break;
    case OPCODE_LDC2_W:
        result = vf_check_cp_double_const( index, cp_parse, ctex );
        break;
    case OPCODE_NEW:
        result = vf_check_cp_class( index, cp_parse, ctex );
        break;
    case OPCODE_ANEWARRAY:
    case OPCODE_CHECKCAST:
        result = vf_check_cp_class( index, cp_parse, ctex );
        break;
    case OPCODE_INSTANCEOF:
    case OPCODE_MULTIANEWARRAY:
        result = vf_check_cp_class( index, NULL, ctex );
        break;
    default:
        VERIFY_DEBUG( "vf_parse_const_pool: incorrect parse of code instruction" );
        return VER_ErrorInternal;
    }
    return result;
} // vf_parse_const_pool

/************************************************************
 ****************** Bytecode functions **********************
 ************************************************************/

/**
 * Function sets code instruction structure for opcode null.
 */
static inline void
vf_opcode_aconst_null( vf_Code_t *code,         // code instruction
                       vf_VerifyPool_t * pool)  // memory pool
{
    vf_set_stack_modifier( code, 1 );
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_ref( code, 0, NULL );
    return;
} // vf_opcode_aconst_null

/**
 * Function sets code instruction structure for opcode iconst_n.
 */
static inline void
vf_opcode_iconst_n( vf_Code_t *code,            // code instruction
                    vf_VerifyPool_t * pool)     // memory pool
{
    vf_set_stack_modifier( code, 1 );
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_iconst_n

/**
 * Function sets code instruction structure for opcode lconst_n.
 */
static inline void
vf_opcode_lconst_n( vf_Code_t *code,            // code instruction
                    vf_VerifyPool_t * pool)     // memory pool
{
    vf_set_stack_modifier( code, 2 );
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_lconst_n

/**
 * Function sets code instruction structure for opcode fconst_n.
 */
static inline void
vf_opcode_fconst_n( vf_Code_t *code,            // code instruction
                    vf_VerifyPool_t * pool)     // memory pool
{
    vf_set_stack_modifier( code, 1 );
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_float( code, 0 );
    return;
} // vf_opcode_fconst_n

/**
 * Function sets code instruction structure for opcode dconst_n.
 */
static inline void
vf_opcode_dconst_n( vf_Code_t *code,            // code instruction
                    vf_VerifyPool_t * pool)     // memory pool
{
    vf_set_stack_modifier( code, 2 );
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_double( code, 0 );
    return;
} // vf_opcode_dconst_n

/**
 * Function sets code instruction structure for opcodes ldcx.
 */
static inline Verifier_Result
vf_opcode_ldcx( vf_Code_t *code,                // code instruction
                int modifier,                   // instruction stack modifier
                unsigned short cp_index,        // constant pool entry index
                vf_Context_t *ctex)             // verifier context
{
    Verifier_Result result;
    vf_Parse_t cp_parse = {0};

    // check constant pool for instruction
    result = vf_parse_const_pool( *(code->m_addr), cp_index, &cp_parse, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier( code, modifier );
    // set stack out vector
    code->m_outvector = cp_parse.field.f_vector;
    code->m_outlen = (unsigned short)cp_parse.field.f_vlen;
    return VER_OK;
} // vf_opcode_ldcx

/**
 * Function sets code instruction structure for opcodes iloadx.
 */
static inline void
vf_opcode_iloadx( vf_Code_t *code,              // code instruction
                  unsigned local,               // local variable number
                  vf_VerifyPool_t * pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_local_var_int( code, 0, local );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_iloadx

/**
 * Function sets code instruction structure for opcodes lloadx.
 */
static inline void
vf_opcode_lloadx( vf_Code_t *code,              // code instruction
                  unsigned local,               // local variable number
                  vf_VerifyPool_t * pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_local_var_long( code, 0, local );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_lloadx

/**
 * Function sets code instruction structure for opcodes floadx.
 */
static inline void
vf_opcode_floadx( vf_Code_t *code,              // code instruction
                  unsigned local,               // local variable number
                  vf_VerifyPool_t * pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_local_var_float( code, 0, local );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_float( code, 0 );
    return;
} // vf_opcode_floadx

/**
 * Function sets code instruction structure for opcodes dloadx.
 */
static inline void
vf_opcode_dloadx( vf_Code_t *code,              // code instruction
                  unsigned local,               // local variable number
                  vf_VerifyPool_t * pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_local_var_double( code, 0, local );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_double( code, 0 );
    return;
} // vf_opcode_dloadx

/**
 * Function sets code instruction structure for opcodes aloadx.
 */
static inline void
vf_opcode_aloadx( vf_Code_t *code,              // code instruction
                  unsigned local,               // local variable number
                  vf_VerifyPool_t * pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_local_var_ref( code, 0, NULL, local );
    vf_set_vector_check( code->m_invector, 0, VF_CHECK_UNINITIALIZED_THIS );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_type( code, 0, SM_COPY_0 );
    return;
} // vf_opcode_aloadx

/**
 * Function sets code instruction structure for opcode iaload.
 */
static inline void
vf_opcode_iaload( vf_Code_t *code,          // code instruction
                  vf_Context_t *ctex)       // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[I", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_iaload

/**
 * Function sets code instruction structure for opcode baload.
 */
static inline void
vf_opcode_baload( vf_Code_t *code,          // code instruction
                  vf_Context_t *ctex)       // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[B", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_baload

/**
 * Function sets code instruction structure for opcode caload.
 */
static inline void
vf_opcode_caload( vf_Code_t *code,          // code instruction
                  vf_Context_t *ctex)       // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[C", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_caload

/**
 * Function sets code instruction structure for opcode saload.
 */
static inline void
vf_opcode_saload( vf_Code_t *code,          // code instruction
                  vf_Context_t *ctex)       // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[S", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_saload

/**
 * Function sets code instruction structure for opcode laload.
 */
static inline void
vf_opcode_laload( vf_Code_t *code,          // code instruction
                  vf_Context_t *ctex)       // verifier context
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[J", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    // create out vector
    vf_new_out_vector( code, 2, ctex->m_pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_laload

/**
 * Function sets code instruction structure for opcode faload.
 */
static inline void
vf_opcode_faload( vf_Code_t *code,          // code instruction
                  vf_Context_t *ctex)       // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[F", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_float( code, 0 );
    return;
} // vf_opcode_faload

/**
 * Function sets code instruction structure for opcode daload.
 */
static inline void
vf_opcode_daload( vf_Code_t *code,          // code instruction
                  vf_Context_t *ctex)       // verifier context
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[D", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    // create out vector
    vf_new_out_vector( code, 2, ctex->m_pool );
    vf_set_out_vector_stack_entry_double( code, 0 );
    return;
} // vf_opcode_daload

/**
 * Function sets code instruction structure for opcode aaload.
 */
static inline void
vf_opcode_aaload( vf_Code_t *code,          // code instruction
                  vf_Context_t *ctex)       // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, ctex->m_pool );
    // create type
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    vf_set_in_vector_check( code, 0, VF_CHECK_REF_ARRAY );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_type( code, 0, SM_UP_ARRAY );
    return;
} // vf_opcode_aaload

/**
 * Function sets code instruction structure for opcodes istorex.
 */
static inline void
vf_opcode_istorex( vf_Code_t *code,             // code instruction
                   unsigned local,              // local variable number
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_local_var_int( code, 0, local );
    return;
} // vf_opcode_istorex

/**
 * Function sets code instruction structure for opcodes lstorex.
 */
static inline void
vf_opcode_lstorex( vf_Code_t *code,             // code instruction
                   unsigned local,              // local variable number
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_local_var_long( code, 0, local );
    return;
} // vf_opcode_lstorex

/**
 * Function sets code instruction structure for opcodes fstorex.
 */
static inline void
vf_opcode_fstorex( vf_Code_t *code,             // code instruction
                   unsigned local,              // local variable number
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_float( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_local_var_float( code, 0, local );
    return;
} // vf_opcode_fstorex

/**
 * Function sets code instruction structure for opcodes dstorex.
 */
static inline void
vf_opcode_dstorex( vf_Code_t *code,             // code instruction
                   unsigned local,              // local variable number
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_double( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_local_var_double( code, 0, local );
    return;
} // vf_opcode_dstorex

/**
 * Function sets code instruction structure for opcodes astorex.
 */
static inline void
vf_opcode_astorex( vf_Code_t *code,             // code instruction
                   unsigned local,              // local variable number
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    vf_set_vector_check( code->m_invector, 0, VF_CHECK_UNINITIALIZED_THIS );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_local_var_type( code, 0, SM_COPY_0, local );
    return;
} // vf_opcode_astorex

/**
 * Function sets code instruction structure for opcode iastore.
 */
static inline void
vf_opcode_iastore( vf_Code_t *code,         // code instruction
                   vf_Context_t *ctex)      // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -3 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 3 );
    // create in vector
    vf_new_in_vector( code, 3, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[I", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    vf_set_in_vector_stack_entry_int( code, 2 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    return;
} // vf_opcode_iastore

/**
 * Function sets code instruction structure for opcode bastore.
 */
static inline void
vf_opcode_bastore( vf_Code_t *code,         // code instruction
                   vf_Context_t *ctex)      // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -3 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 3 );
    // create in vector
    vf_new_in_vector( code, 3, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[B", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    vf_set_in_vector_stack_entry_int( code, 2 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    return;
} // vf_opcode_bastore

/**
 * Function sets code instruction structure for opcode castore.
 */
static inline void
vf_opcode_castore( vf_Code_t *code,         // code instruction
                   vf_Context_t *ctex)      // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -3 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 3 );
    // create in vector
    vf_new_in_vector( code, 3, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[C", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    vf_set_in_vector_stack_entry_int( code, 2 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    return;
} // vf_opcode_castore

/**
 * Function sets code instruction structure for opcode sastore.
 */
static inline void
vf_opcode_sastore( vf_Code_t *code,         // code instruction
                   vf_Context_t *ctex)      // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -3 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 3 );
    // create in vector
    vf_new_in_vector( code, 3, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[S", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    vf_set_in_vector_stack_entry_int( code, 2 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    return;
} // vf_opcode_sastore

/**
 * Function sets code instruction structure for opcode lastore.
 */
static inline void
vf_opcode_lastore( vf_Code_t *code,         // code instruction
                   vf_Context_t *ctex)      // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -4 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 4 );
    // create in vector
    vf_new_in_vector( code, 4, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[J", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    vf_set_in_vector_stack_entry_long( code, 2 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    return;
} // vf_opcode_lastore

/**
 * Function sets code instruction structure for opcode fastore.
 */
static inline void
vf_opcode_fastore( vf_Code_t *code,         // code instruction
                   vf_Context_t *ctex)      // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -3 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 3 );
    // create in vector
    vf_new_in_vector( code, 3, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[F", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    vf_set_in_vector_stack_entry_float( code, 2 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    return;
} // vf_opcode_fastore

/**
 * Function sets code instruction structure for opcode dastore.
 */
static inline void
vf_opcode_dastore( vf_Code_t *code,         // code instruction
                   vf_Context_t *ctex)      // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -4 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 4 );
    // create in vector
    vf_new_in_vector( code, 4, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "[D", 2 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    vf_set_in_vector_stack_entry_int( code, 1 );
    vf_set_in_vector_stack_entry_double( code, 2 );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_EQUAL );
    return;
} // vf_opcode_dastore

/**
 * Function sets code instruction structure for opcode aastore.
 */
static inline void
vf_opcode_aastore( vf_Code_t *code,         // code instruction
                   vf_Context_t *ctex)      // verifier context
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -3 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 3 );
    // create in vector
    vf_new_in_vector( code, 3, ctex->m_pool );
    // create type
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    vf_set_in_vector_check( code, 0, VF_CHECK_REF_ARRAY );
    vf_set_in_vector_stack_entry_int( code, 1 );
    if( vf_is_class_version_14(ctex) ) {
        vf_set_in_vector_type( code, 2, SM_UP_ARRAY );
    } else {
        vf_set_in_vector_stack_entry_ref( code, 2, NULL );
    }
    return;
} // vf_opcode_aastore

/**
 * Function sets code instruction structure for opcode pop.
 */
static inline void
vf_opcode_pop( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_word( code, 0 );
    return;
} // vf_opcode_pop

/**
 * Function sets code instruction structure for opcode pop2.
 */
static inline void
vf_opcode_pop2( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_word2( code, 0 );
    return;
} // vf_opcode_pop2

/**
 * Function sets code instruction structure for opcode dup.
 */
static inline void
vf_opcode_dup( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_word( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_type( code, 0, SM_COPY_0 );
    vf_set_out_vector_type( code, 1, SM_COPY_0 );
    return;
} // vf_opcode_dup

/**
 * Function sets code instruction structure for opcode dup_x1.
 */
static inline void
vf_opcode_dup_x1( vf_Code_t *code,              // code instruction
                  vf_VerifyPool_t * pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_word( code, 0 );
    vf_set_in_vector_stack_entry_word( code, 1 );
    // create out vector
    vf_new_out_vector( code, 3, pool );
    vf_set_out_vector_type( code, 0, SM_COPY_1 );
    vf_set_out_vector_type( code, 1, SM_COPY_0 );
    vf_set_out_vector_type( code, 2, SM_COPY_1 );
    return;
} // vf_opcode_dup_x1

/**
 * Function sets code instruction structure for opcode dup_x2.
 */
static inline void
vf_opcode_dup_x2( vf_Code_t *code,              // code instruction
                  vf_VerifyPool_t * pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 3 );
    // create in vector
    vf_new_in_vector( code, 3, pool );
    vf_set_in_vector_stack_entry_word2( code, 0 );
    vf_set_in_vector_stack_entry_word( code, 2 );
    // create out vector
    vf_new_out_vector( code, 4, pool );
    vf_set_out_vector_type( code, 0, SM_COPY_2 );
    vf_set_out_vector_type( code, 1, SM_COPY_0 );
    vf_set_out_vector_type( code, 2, SM_COPY_1 );
    vf_set_out_vector_type( code, 3, SM_COPY_2 );
    return;
} // vf_opcode_dup_x2

/**
 * Function sets code instruction structure for opcode dup2.
 */
static inline void
vf_opcode_dup2( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_word2( code, 0 );
    // create out vector
    vf_new_out_vector( code, 4, pool );
    vf_set_out_vector_type( code, 0, SM_COPY_0 );
    vf_set_out_vector_type( code, 1, SM_COPY_1 );
    vf_set_out_vector_type( code, 2, SM_COPY_0 );
    vf_set_out_vector_type( code, 3, SM_COPY_1 );
    return;
} // vf_opcode_dup2

/**
 * Function sets code instruction structure for opcode dup2_x1.
 */
static inline void
vf_opcode_dup2_x1( vf_Code_t *code,             // code instruction
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 3 );
    // create in vector
    vf_new_in_vector( code, 3, pool );
    vf_set_in_vector_stack_entry_word( code, 0 );
    vf_set_in_vector_stack_entry_word2( code, 1 );
    // create out vector
    vf_new_out_vector( code, 5, pool );
    vf_set_out_vector_type( code, 0, SM_COPY_1 );
    vf_set_out_vector_type( code, 1, SM_COPY_2 );
    vf_set_out_vector_type( code, 2, SM_COPY_0 );
    vf_set_out_vector_type( code, 3, SM_COPY_1 );
    vf_set_out_vector_type( code, 4, SM_COPY_2 );
    return;
} // vf_opcode_dup2_x1

/**
 * Function sets code instruction structure for opcode dup2_x2.
 */
static inline void
vf_opcode_dup2_x2( vf_Code_t *code,             // code instruction
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 4 );
    // create in vector
    vf_new_in_vector( code, 4, pool );
    vf_set_in_vector_stack_entry_word2( code, 0 );
    vf_set_in_vector_stack_entry_word2( code, 2 );
    // create out vector
    vf_new_out_vector( code, 6, pool );
    vf_set_out_vector_type( code, 0, SM_COPY_2 );
    vf_set_out_vector_type( code, 1, SM_COPY_3 );
    vf_set_out_vector_type( code, 2, SM_COPY_0 );
    vf_set_out_vector_type( code, 3, SM_COPY_1 );
    vf_set_out_vector_type( code, 4, SM_COPY_2 );
    vf_set_out_vector_type( code, 5, SM_COPY_3 );
    return;
} // vf_opcode_dup2_x2

/**
 * Function sets code instruction structure for opcode swap.
 */
static inline void
vf_opcode_swap( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_word( code, 0 );
    vf_set_in_vector_stack_entry_word( code, 1 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_type( code, 0, SM_COPY_1 );
    vf_set_out_vector_type( code, 1, SM_COPY_0 );
    return;
} // vf_opcode_swap

/**
 * Function sets code instruction structure for opcode iadd.
 */
static inline void
vf_opcode_iadd( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_iadd

/**
 * Function sets code instruction structure for opcode ladd.
 */
static inline void
vf_opcode_ladd( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 4 );
    // create in vector
    vf_new_in_vector( code, 4, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    vf_set_in_vector_stack_entry_long( code, 2 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_ladd

/**
 * Function sets code instruction structure for opcode fadd.
 */
static inline void
vf_opcode_fadd( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_float( code, 0 );
    vf_set_in_vector_stack_entry_float( code, 1 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_float( code, 0 );
    return;
} // vf_opcode_fadd

/**
 * Function sets code instruction structure for opcode dadd.
 */
static inline void
vf_opcode_dadd( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 4 );
    // create in vector
    vf_new_in_vector( code, 4, pool );
    vf_set_in_vector_stack_entry_double( code, 0 );
    vf_set_in_vector_stack_entry_double( code, 2 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_double( code, 0 );
    return;
} // vf_opcode_dadd

/**
 * Function sets code instruction structure for opcode ineg.
 */
static inline void
vf_opcode_ineg( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_ineg

/**
 * Function sets code instruction structure for opcode lneg.
 */
static inline void
vf_opcode_lneg( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_lneg

/**
 * Function sets code instruction structure for opcode fneg.
 */
static inline void
vf_opcode_fneg( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_float( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_float( code, 0 );
    return;
} // vf_opcode_fneg

/**
 * Function sets code instruction structure for opcode dneg.
 */
static inline void
vf_opcode_dneg( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_double( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_double( code, 0 );
    return;
} // vf_opcode_dneg

/**
 * Function sets code instruction structure for bit operation opcodes.
 */
static inline void
vf_opcode_ibit( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    vf_set_in_vector_stack_entry_int( code, 1 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_ibit

/**
 * Function sets code instruction structure for opcodes lshx.
 */
static inline void
vf_opcode_lshx( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 3 );
    // create in vector
    vf_new_in_vector( code, 3, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    vf_set_in_vector_stack_entry_int( code, 2 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_lshx

/**
 * Function sets code instruction structure for opcode land.
 */
static inline void
vf_opcode_land( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 4 );
    // create in vector
    vf_new_in_vector( code, 4, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    vf_set_in_vector_stack_entry_long( code, 2 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_land

/**
 * Function sets code instruction structure for opcode iinc.
 */
static inline void
vf_opcode_iinc( vf_Code_t *code,            // code instruction
                unsigned local,             // local variable number
                vf_VerifyPool_t * pool)     // memory pool
{
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_local_var_int( code, 0, local );
    return;
} // vf_opcode_iinc

/**
 * Function sets code instruction structure for opcode i2l.
 */
static inline void
vf_opcode_i2l( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_i2l

/**
 * Function sets code instruction structure for opcode i2f.
 */
static inline void
vf_opcode_i2f( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_float( code, 0 );
    return;
} // vf_opcode_i2f

/**
 * Function sets code instruction structure for opcode i2d.
 */
static inline void
vf_opcode_i2d( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_double( code, 0 );
    return;
} // vf_opcode_i2d

/**
 * Function sets code instruction structure for opcode l2i.
 */
static inline void
vf_opcode_l2i( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_l2i

/**
 * Function sets code instruction structure for opcode l2f.
 */
static inline void
vf_opcode_l2f( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_float( code, 0 );
    return;
} // vf_opcode_l2f

/**
 * Function sets code instruction structure for opcode l2d.
 */
static inline void
vf_opcode_l2d( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_double( code, 0 );
    return;
} // vf_opcode_l2d

/**
 * Function sets code instruction structure for opcode f2i.
 */
static inline void
vf_opcode_f2i( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_float( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_f2i

/**
 * Function sets code instruction structure for opcode f2l.
 */
static inline void
vf_opcode_f2l( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_float( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_f2l

/**
 * Function sets code instruction structure for opcode f2d.
 */
static inline void
vf_opcode_f2d( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_float( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_double( code, 0 );
    return;
} // vf_opcode_f2d

/**
 * Function sets code instruction structure for opcode d2i.
 */
static inline void
vf_opcode_d2i( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_double( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_d2i

/**
 * Function sets code instruction structure for opcode d2l.
 */
static inline void
vf_opcode_d2l( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_double( code, 0 );
    // create out vector
    vf_new_out_vector( code, 2, pool );
    vf_set_out_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_d2l

/**
 * Function sets code instruction structure for opcode d2f.
 */
static inline void
vf_opcode_d2f( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_double( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_float( code, 0 );
    return;
} // vf_opcode_d2f

/**
 * Function sets code instruction structure for opcode i2b.
 */
static inline void
vf_opcode_i2b( vf_Code_t *code,             // code instruction
               vf_VerifyPool_t * pool)      // memory pool
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_i2b

/**
 * Function sets code instruction structure for opcode lcmp.
 */
static inline void
vf_opcode_lcmp( vf_Code_t *code,            // code instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -3 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 4 );
    // create in vector
    vf_new_in_vector( code, 4, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    vf_set_in_vector_stack_entry_long( code, 2 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_lcmp

/**
 * Function sets code instruction structure for opcodes fcmpx.
 */
static inline void
vf_opcode_fcmpx( vf_Code_t *code,               // code instruction
                 vf_VerifyPool_t * pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_float( code, 0 );
    vf_set_in_vector_stack_entry_float( code, 1 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_fcmpx

/**
 * Function sets code instruction structure for opcodes dcmpx.
 */
static inline void
vf_opcode_dcmpx( vf_Code_t *code,               // code instruction
                 vf_VerifyPool_t * pool)        // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -3 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 4 );
    // create in vector
    vf_new_in_vector( code, 4, pool );
    vf_set_in_vector_stack_entry_double( code, 0 );
    vf_set_in_vector_stack_entry_double( code, 2 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_dcmpx

/**
 * Function sets code instruction structure for opcode ifeq.
 */
static inline void
vf_opcode_ifeq( vf_Code_t *code,            // code instruction
                vf_Instr_t *icode1,         // first branch instruction
                vf_Instr_t *icode2,         // second branch instruction
                vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // set begin of basic block for branch instruction
    vf_set_basic_block_flag( icode1 );
    // set begin of basic block for branch instruction
    vf_set_basic_block_flag( icode2 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_ifeq

/**
 * Function sets code instruction structure for opcode if_icmpeq.
 */
static inline void
vf_opcode_if_icmpeq( vf_Code_t *code,           // code instruction
                     vf_Instr_t *icode1,        // first branch instruction
                     vf_Instr_t *icode2,        // second branch instruction
                     vf_VerifyPool_t * pool)    // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // set begin of basic block for branch instruction
    vf_set_basic_block_flag( icode1 );
    // set begin of basic block for branch instruction
    vf_set_basic_block_flag( icode2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    vf_set_in_vector_stack_entry_int( code, 1 );
    return;
} // vf_opcode_if_icmpeq

/**
 * Function sets code instruction structure for opcode if_acmpeq.
 */
static inline void
vf_opcode_if_acmpeq( vf_Code_t *code,           // code instruction
                     vf_Instr_t *icode1,        // first branch instruction
                     vf_Instr_t *icode2,        // second branch instruction
                     vf_VerifyPool_t * pool)    // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // set begin of basic block for branch instruction
    vf_set_basic_block_flag( icode1 );
    // set begin of basic block for branch instruction
    vf_set_basic_block_flag( icode2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    vf_set_in_vector_stack_entry_ref( code, 1, NULL );
    return;
} // vf_opcode_if_acmpeq

/**
 * Function sets code instruction structure for opcode switch.
 */
static inline void
vf_opcode_switch( vf_Code_t *code,              // code instruction
                  vf_VerifyPool_t * pool)       // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_switch

/**
 * Function sets code instruction structure for opcode ireturn.
 */
static inline void
vf_opcode_ireturn( vf_Code_t *code,             // code instruction
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set instruction flag
    vf_set_instruction_flag( code, VF_FLAG_RETURN );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_ireturn

/**
 * Function sets code instruction structure for opcode lreturn.
 */
static inline void
vf_opcode_lreturn( vf_Code_t *code,             // code instruction
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set instruction flag
    vf_set_instruction_flag( code, VF_FLAG_RETURN );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_long( code, 0 );
    return;
} // vf_opcode_lreturn

/**
 * Function sets code instruction structure for opcode freturn.
 */
static inline void
vf_opcode_freturn( vf_Code_t *code,             // code instruction
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set instruction flag
    vf_set_instruction_flag( code, VF_FLAG_RETURN );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_float( code, 0 );
    return;
} // vf_opcode_freturn

/**
 * Function sets code instruction structure for opcode dreturn.
 */
static inline void
vf_opcode_dreturn( vf_Code_t *code,             // code instruction
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set instruction flag
    vf_set_instruction_flag( code, VF_FLAG_RETURN );
    // set minimal stack for instruction
    vf_set_min_stack( code, 2 );
    // create in vector
    vf_new_in_vector( code, 2, pool );
    vf_set_in_vector_stack_entry_double( code, 0 );
    return;
} // vf_opcode_dreturn

/**
 * Function sets code instruction structure for opcode areturn.
 */
static inline void
vf_opcode_areturn( vf_Code_t *code,             // code instruction
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set instruction flag
    vf_set_instruction_flag( code, VF_FLAG_RETURN );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    return;
} // vf_opcode_areturn

/**
 * Function sets code instruction structure for opcode getstatic.
 */
static inline Verifier_Result
vf_opcode_getstatic( vf_Code_t *code,               // code instruction
                     unsigned short cp_index,       // constant pool entry index
                     vf_Context_t *ctex)            // verifier context
{
    Verifier_Result result;
    vf_Parse_t cp_parse = {0};

    // check constant pool for instruction
    result = vf_parse_const_pool( OPCODE_GETSTATIC, cp_index, &cp_parse, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier( code, cp_parse.field.f_vlen );
    // set stack out vector
    code->m_outvector = cp_parse.field.f_vector;
    code->m_outlen = (unsigned short)cp_parse.field.f_vlen;
    return VER_OK;
} // vf_opcode_getstatic

/**
 * Function sets code instruction structure for opcode putstatic.
 */
static inline Verifier_Result
vf_opcode_putstatic( vf_Code_t *code,           // code instruction
                     unsigned short cp_index,   // constant pool entry index
                     vf_Context_t *ctex)        // verifier context
{
    Verifier_Result result;
    vf_Parse_t cp_parse = {0};

    // check constant pool for instruction
    result = vf_parse_const_pool( OPCODE_PUTSTATIC, cp_index, &cp_parse, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -(int)cp_parse.field.f_vlen );
    // set minimal stack for instruction
    vf_set_min_stack( code, cp_parse.field.f_vlen );
    // set stack out vector
    code->m_invector = cp_parse.field.f_vector;
    code->m_inlen = (unsigned short)cp_parse.field.f_vlen;
    // set assign check
    vf_set_in_vector_check( code, code->m_inlen - 1, VF_CHECK_ASSIGN );
    return VER_OK;
} // vf_opcode_putstatic

/**
 * Function sets code instruction structure for opcode getfield.
 */
static inline Verifier_Result
vf_opcode_getfield( vf_Code_t *code,            // code instruction
                    unsigned short cp_index,    // constant pool entry index
                    vf_Context_t *ctex)         // verifier context

{
    Verifier_Result result;
    vf_Parse_t cp_parse = {0};

    // check constant pool for instruction
    result = vf_parse_const_pool( OPCODE_GETFIELD, cp_index, &cp_parse, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // set stack modifier for instruction, remove double this reference
    vf_set_stack_modifier( code, cp_parse.field.f_vlen - 2 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // set stack in vector
    code->m_invector = cp_parse.field.f_vector;
    code->m_inlen = 1;
    // set field access check
    vf_set_in_vector_check( code, 0, VF_CHECK_ACCESS_FIELD );
    vf_set_in_vector_check_index( code, 0, cp_index );
    // set stack out vector, skip this reference
    code->m_outvector = cp_parse.field.f_vector + 1;
    code->m_outlen = (unsigned short)cp_parse.field.f_vlen - 1;
    return VER_OK;
} // vf_opcode_getfield

/**
 * Function sets code instruction structure for opcode putfield.
 */
static inline Verifier_Result
vf_opcode_putfield( vf_Code_t *code,            // code instruction
                    unsigned short cp_index,    // constant pool entry index
                    vf_Context_t *ctex)         // verifier context
{
    Verifier_Result result;
    vf_Parse_t cp_parse = {0};

    // check constant pool for instruction
    result = vf_parse_const_pool( OPCODE_PUTFIELD, cp_index, &cp_parse, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -cp_parse.field.f_vlen );
    // set minimal stack for instruction
    vf_set_min_stack( code, cp_parse.field.f_vlen );
    // set stack in vector
    code->m_invector = cp_parse.field.f_vector;
    code->m_inlen = (unsigned short)cp_parse.field.f_vlen;
    // set assign check
    vf_set_in_vector_check( code, code->m_inlen - 1, VF_CHECK_ASSIGN );
    // set field access check
    vf_set_in_vector_check( code, 0, VF_CHECK_ACCESS_FIELD );
    vf_set_in_vector_check_index( code, 0, cp_index );
    return VER_OK;
} // vf_opcode_putfield

/**
 * Function sets code instruction structure for invokes opcodes.
 */
static inline Verifier_Result
vf_opcode_invoke( vf_Code_t *code,              // code instruction
                  unsigned short cp_index,      // constant pool entry index
                  vf_Context_t *ctex)           // verifier context
{
    Verifier_Result result;
    vf_Parse_t cp_parse = {0};

    // check constant pool for instruction
    result = vf_parse_const_pool( *(code->m_addr), cp_index, &cp_parse, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // check method name
    if( cp_parse.method.m_name[0] == '<' ) {
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Must call initializers using invokespecial" );
        return VER_ErrorConstantPool;
    }
    // check number of arguments
    if( cp_parse.method.m_inlen > 255 ) {
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") The number of method parameters is limited to 255" );
        return VER_ErrorInstruction;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier( code, cp_parse.method.m_outlen - cp_parse.method.m_inlen );
    // set minimal stack for instruction
    vf_set_min_stack( code, cp_parse.method.m_inlen );
    // set stack in vector
    code->m_invector = cp_parse.method.m_invector;
    code->m_inlen = (unsigned short)cp_parse.method.m_inlen;
    // set stack out vector
    code->m_outvector = cp_parse.method.m_outvector;
    code->m_outlen = (unsigned short)cp_parse.method.m_outlen;
    // set method access check for opcode invokevirtual
    if( (*code->m_addr) == OPCODE_INVOKEVIRTUAL ) {
        vf_set_in_vector_check( code, 0, VF_CHECK_ACCESS_METHOD );
        vf_set_in_vector_check_index( code, 0, cp_index );
    }
    return VER_OK;
} // vf_opcode_invoke


/**
 * Function sets code instruction structure for invokes opcodes.
 */
static inline Verifier_Result
vf_opcode_invokespecial( vf_Code_t *code,              // code instruction
                         unsigned short cp_index,      // constant pool entry index
                         vf_Context_t *ctex)           // verifier context
{
    Verifier_Result result;
    vf_Parse_t cp_parse = {0};

    // check constant pool for instruction
    result = vf_parse_const_pool( *(code->m_addr), cp_index, &cp_parse, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // check number of arguments
    if( cp_parse.method.m_inlen > 255 ) {
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") The number of method parameters is limited to 255" );
        return VER_ErrorInstruction;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier( code, cp_parse.method.m_outlen - cp_parse.method.m_inlen );
    // set minimal stack for instruction
    vf_set_min_stack( code, cp_parse.method.m_inlen );
    // set stack in vector
    code->m_invector = cp_parse.method.m_invector;
    code->m_inlen = (unsigned short)cp_parse.method.m_inlen;
    // set stack out vector
    code->m_outvector = cp_parse.method.m_outvector;
    code->m_outlen = (unsigned short)cp_parse.method.m_outlen;
    // set method check for opcode invokespecial
    if( !strcmp( cp_parse.method.m_name, "<init>" ) ) {
        // set uninitialized check
        vf_set_in_vector_type( code, 0, SM_UNINITIALIZED );
        vf_set_in_vector_check( code, 0, VF_CHECK_DIRECT_SUPER );
    } else {
        // set method access check
        vf_set_in_vector_check( code, 0, VF_CHECK_INVOKESPECIAL );
    }
    vf_set_in_vector_check_index( code, 0, cp_index );
    return VER_OK;
} // vf_opcode_invokespecial

/**
 * Function sets code instruction structure for opcode new.
 */
static inline Verifier_Result
vf_opcode_new( vf_Code_t *code,             // code instruction
               unsigned short cp_index,     // constant pool entry index
               unsigned opcode_new,         // number of opcode new
               vf_Context_t *ctex)          // verifier context
{
    Verifier_Result result;
    vf_Parse_t cp_parse = {0};

    // check constant pool for instruction
    result = vf_parse_const_pool( OPCODE_NEW, cp_index, &cp_parse, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // check created reference
    assert( cp_parse.field.f_vector->m_vtype->number == 1 );
    if( cp_parse.field.f_vector->m_vtype->string[0][0] != 'L' ) {
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Illegal creation of array" );
        return VER_ErrorInstruction;
    }
    // set stack out vector
    code->m_outvector = cp_parse.field.f_vector;
    code->m_outlen = (unsigned short)cp_parse.field.f_vlen;
    // set uninitialized reference
    vf_set_out_vector_type( code, 0, SM_UNINITIALIZED );
    // set opcode program counter
    vf_set_out_vector_opcode_new( code, 0, opcode_new );
    return VER_OK;
} // vf_opcode_new

/**
 * Function sets code instruction structure for opcode newarray.
 */
static inline Verifier_Result
vf_opcode_newarray( vf_Code_t *code,        // code instruction
                    unsigned char type,     // array element type
                    vf_Context_t *ctex)     // verifier context
{
    vf_ValidType_t *vtype;

    switch( type )
    {
    case 4:  // boolean
    case 8:  // byte
        vtype = ctex->m_type->NewType( "[B", 2 );
        break;
    case 5:  // char
        vtype = ctex->m_type->NewType( "[C", 2 );
        break;
    case 9:  // short
        vtype = ctex->m_type->NewType( "[S", 2 );
        break;
    case 10: // int
        vtype = ctex->m_type->NewType( "[I", 2 );
        break;
    case 6:  // float
        vtype = ctex->m_type->NewType( "[F", 2 );
        break;
    case 7:  // double
        vtype = ctex->m_type->NewType( "[D", 2 );
        break;
    case 11: // long
        vtype = ctex->m_type->NewType( "[J", 2 );
        break;
    default:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Incorrect type in instruction newarray" );
        return VER_ErrorInstruction;
    }
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, ctex->m_pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_ref( code, 0, vtype );
    return VER_OK;
} // vf_opcode_newarray

/**
 * Function receives valid type string of array by array element name.
 */
static inline const char *
vf_get_class_array_valid_type( const char *element_name,    // array element name
                               unsigned name_len,           // element name length
                               unsigned dimension,          // dimension of array
                               unsigned *result_len,        // pointer to result string length
                               vf_VerifyPool_t *pool)       // memory pool
{
    unsigned len,
             index;
    char *result;

    // create valid type
    if( element_name[0] == '[' ) {
        // copy array class name
        if( element_name[name_len - 1] == ';' ) {
            // object type array
            len = name_len + dimension - 1;
            name_len--;
        } else {
            // primitive type array
            len = name_len + dimension;
            switch( element_name[name_len - 1] )
            {
            case 'Z':
                // set integer array type
                result = (char*)vf_alloc_pool_memory( pool, len + 1 );
                for( index = 0; index < dimension; index++ ) {
                    result[index] = '[';
                }
                memcpy( &result[dimension], element_name, name_len );
                result[len - 1] = 'B';
                *result_len = len;
                return result;
            default:
                // don't need change type
                break;
            }
        }
        result = (char*)vf_alloc_pool_memory( pool, len + 1 );
        for( index = 0; index < dimension; index++ ) {
            result[index] = '[';
        }
        memcpy( &result[dimension], element_name, name_len );
    } else {
        // create class signature
        len = name_len + dimension + 1;
        result = (char*)vf_alloc_pool_memory( pool, len + 1 );
        for( index = 0; index < dimension; index++ ) {
            result[index] = '[';
        }
        result[dimension] = 'L';
        memcpy( &result[dimension + 1], element_name, name_len );
    }
    *result_len = len;
    return result;
} // vf_get_class_array_valid_type

/**
 * Function sets code instruction structure for opcode anewarray.
 */
static inline Verifier_Result
vf_opcode_anewarray( vf_Code_t *code,           // code instruction
                     unsigned short cp_index,   // constant pool entry index
                     vf_Context_t *ctex)        // verifier context
{
    // check constant pool for instruction
    Verifier_Result result = vf_parse_const_pool( OPCODE_ANEWARRAY,
        cp_index, NULL, ctex );
    if( result != VER_OK ) {
        return result;
    }

    // get array element type name
    const char *name = vf_get_cp_class_name( cp_index, ctex );
    assert( name );

    // create valid type string
    unsigned len;
    unsigned class_name_len = strlen( name );
    const char *array = vf_get_class_array_valid_type( name, class_name_len, 1,
        &len, ctex->m_pool );

    // check dimension
    unsigned short index;
    for( index = 0; array[index] == '['; index++ ) {
        continue;
    }
    if( index > 255 ) {
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Array with too many dimensions" );
        return VER_ErrorInstruction;
    }

    // create valid type
    vf_ValidType_t *type = ctex->m_type->NewType( array, len );

    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, ctex->m_pool );
    vf_set_in_vector_stack_entry_int( code, 0 );
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_ref( code, 0, type );
    return VER_OK;
} // vf_opcode_anewarray

/**
 * Function sets code instruction structure for opcode arraylength.
 */
static inline void
vf_opcode_arraylength( vf_Code_t *code,         // code instruction
                       vf_Context_t *ctex)      // verifier context
{
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, ctex->m_pool );
    // create type
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    // set check
    vf_set_in_vector_check( code, 0, VF_CHECK_ARRAY );
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return;
} // vf_opcode_arraylength

/**
 * Function sets code instruction structure for opcode athrow.
 */
static inline void
vf_opcode_athrow( vf_Code_t *code,          // code instruction
                  vf_Context_t *ctex)       // verifier context
{
    // set instruction flag
    vf_set_instruction_flag( code, VF_FLAG_THROW );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, ctex->m_pool );
    // create type
    vf_ValidType_t *type = ctex->m_type->NewType( "Ljava/lang/Throwable", 20 );
    vf_set_in_vector_stack_entry_ref( code, 0, type );
    return;
} // vf_opcode_athrow

/**
 * Function sets code instruction structure for opcode checkcast.
 */
static inline Verifier_Result
vf_opcode_checkcast( vf_Code_t *code,           // code instruction
                     unsigned short cp_index,   // constant pool entry index
                     vf_Context_t *ctex)        // verifier context
{
    Verifier_Result result;
    vf_Parse_t cp_parse = {0};

    // check constant pool for instruction
    result = vf_parse_const_pool( OPCODE_CHECKCAST, cp_index, &cp_parse, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, ctex->m_pool );
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    // set stack out vector
    code->m_outvector = cp_parse.field.f_vector;
    code->m_outlen = (unsigned short)cp_parse.field.f_vlen;
    return VER_OK;
} // vf_opcode_checkcast

/**
 * Function sets code instruction structure for opcode instanceof.
 */
static inline Verifier_Result
vf_opcode_instanceof( vf_Code_t *code,              // code instruction
                      unsigned short cp_index,      // constant pool entry index
                      vf_Context_t *ctex)           // verifier context
{
    // check constant pool for instruction
    Verifier_Result result = vf_parse_const_pool( OPCODE_INSTANCEOF, cp_index, NULL, ctex );
    if( result != VER_OK ) {
        return result;
    }
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, ctex->m_pool );
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    // set stack out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_int( code, 0 );
    return VER_OK;
} // vf_opcode_instanceof

/**
 * Function sets code instruction structure for opcodes monitorx.
 */
static inline void
vf_opcode_monitorx( vf_Code_t *code,            // code instruction
                    vf_VerifyPool_t * pool)     // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    return;
} // vf_opcode_monitorx

/**
 * Function sets code instruction structure for opcode multianewarray.
 */
static inline Verifier_Result
vf_opcode_multianewarray( vf_Code_t *code,              // code instruction
                          unsigned short cp_index,      // constant pool entry index
                          unsigned short dimension,     // dimension of array
                          vf_Context_t *ctex)           // verifier context
{
    // check constant pool for instruction
    Verifier_Result result = vf_parse_const_pool( OPCODE_MULTIANEWARRAY, cp_index, NULL, ctex );
    if( result != VER_OK ) {
        return result;
    }

    // get array element type name
    const char *name = vf_get_cp_class_name( cp_index, ctex );
    assert( name );

    // get valid type string
    unsigned len;
    unsigned class_name_len = strlen( name );
    const char *array = vf_get_class_valid_type( name, class_name_len,
                                                 &len, ctex->m_pool );
    // check dimension
    unsigned short index;
    for( index = 0; array[index] == '['; index++ ) {
        continue;
    }
    if( index > 255 ) {
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Array with too many dimensions" );
        return VER_ErrorInstruction;
    }
    if( dimension == 0 || index < dimension ) {
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Illegal dimension argument" );
        return VER_ErrorInstruction;
    }

    // create valid type
    vf_ValidType_t *type = ctex->m_type->NewType( array, len );

    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 - dimension );
    // set minimal stack for instruction
    vf_set_min_stack( code, dimension );
    // create in vector
    vf_new_in_vector( code, dimension, ctex->m_pool );
    for( index = 0; index < dimension; index++ ) {
        vf_set_in_vector_stack_entry_int( code, index );
    }
    // create out vector
    vf_new_out_vector( code, 1, ctex->m_pool );
    vf_set_out_vector_stack_entry_ref( code, 0, type );
    return VER_OK;
} // vf_opcode_multianewarray

/**
 * Function sets code instruction structure for opcodes ifxnull.
 */
static inline void
vf_opcode_ifxnull( vf_Code_t *code,             // code instruction
                   vf_Instr_t *icode1,          // first branch instruction
                   vf_Instr_t *icode2,          // second branch instruction
                   vf_VerifyPool_t * pool)      // memory pool
{
    // set stack modifier for instruction
    vf_set_stack_modifier( code, -1 );
    // set minimal stack for instruction
    vf_set_min_stack( code, 1 );
    // set begin of basic block for branch instruction
    vf_set_basic_block_flag( icode1 );
    // set begin of basic block for branch instruction
    vf_set_basic_block_flag( icode2 );
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_stack_entry_ref( code, 0, NULL );
    return;
} // vf_opcode_ifxnull

/**
 * Function sets code instruction structure for opcodes jsr and jsr_w.
 */
static inline void
vf_opcode_jsr( vf_Code_t *code,            // code instruction
               unsigned code_num,          // program count of instruction
               vf_VerifyPool_t *pool)      // memory pool
{
    // set instruction flag
    vf_set_instruction_flag( code, VF_FLAG_SUBROUTINE );
    // set stack modifier for instruction
    vf_set_stack_modifier( code, 1 );
    // create out vector
    vf_new_out_vector( code, 1, pool );
    vf_set_out_vector_stack_entry_addr( code, 0, code_num );
    return;
} // vf_opcode_jsr

/**
 * Function sets code instruction structure for opcode ret.
 */
static inline void
vf_opcode_ret( vf_Code_t *code,              // code instruction
               unsigned local,               // local variable number
               vf_VerifyPool_t * pool)       // memory pool
{
    // create in vector
    vf_new_in_vector( code, 1, pool );
    vf_set_in_vector_local_var_addr( code, 0, 0, local );
    return;
} // vf_opcode_ret

/**
 * Function sets code instruction structure for end-entry.
 */
static inline void
vf_create_end_entry( vf_Code_t *code,           // code instruction
                     vf_Context_t *ctex)        // verifier context
{
    int inlen,
        outlen;
    vf_MapEntry_t *invector,
                  *outvector;

    // set end-entry flag
    vf_set_instruction_flag( code, VF_FLAG_BEGIN_BASIC_BLOCK | VF_FLAG_END_ENTRY);
    // get method description
    char *descr = (char*)method_get_descriptor( ctex->m_method );
    // parse description
    vf_parse_description( descr, &inlen, &outlen );
    // set minimal stack for instruction
    vf_set_min_stack( code, outlen );
    // create method vectors
    vf_set_description_vector( descr, inlen, 0, outlen, &invector, &outvector, ctex );
    // set stack in vector
    code->m_invector = outvector;
    code->m_inlen = (unsigned short)outlen;
    return;
} // vf_create_end_entry

/**
 * Function parse bytecode, determines code instructions, fills code array and 
 * provides checks of simple verifications.
 */
static Verifier_Result
vf_parse_bytecode( vf_Context_t *ctex )     // verifier context
{
    int offset,
        number;
    unsigned edges = 0,
             index,
             count,
             bbCount = 0;
    unsigned short local,
                   constIndex;
    Verifier_Result result = VER_OK;

    /**
     * Getting bytecode parameters
     */
    unsigned len = method_get_code_length( ctex->m_method );
    unsigned char *bytecode = method_get_bytecode( ctex->m_method );
    unsigned short locals = method_get_max_local( ctex->m_method );
    unsigned short handlcount = method_get_exc_handler_number( ctex->m_method );
    unsigned short constlen = class_get_cp_size( ctex->m_class );
    
    /**
     * Allocate memory for codeInstr
     * +1 more instruction is the end of exception handler
     */
    vf_Instr_t *codeInstr = (vf_Instr_t*)vf_alloc_pool_memory( ctex->m_pool,
                                    (len + 1) * sizeof(vf_Instr_t) );
    /**
     * Create start-entry instruction
     */
    unsigned codeNum = 0;
    vf_set_instruction_flag( &ctex->m_code[codeNum], 
        VF_FLAG_BEGIN_BASIC_BLOCK | VF_FLAG_START_ENTRY);
    codeNum++;

    /** 
     * Create handler instructions
     */
    vf_Code_t *code;
    for( index = 0, code = &ctex->m_code[codeNum];
         index < handlcount;
         index++, codeNum++, code = &ctex->m_code[codeNum] )
    {
        vf_set_instruction_flag( code, VF_FLAG_BEGIN_BASIC_BLOCK | VF_FLAG_HANDLER );
    }

    /**
     * Define bytecode instructions and fill code array
     */
    unsigned instr;
    unsigned branches;
    vf_VerifyPool_t *pool = ctex->m_pool;
    for( index = 0, code = &ctex->m_code[codeNum];
         index < len;
         codeNum++, code = &ctex->m_code[codeNum] )
    {
        code->m_addr = &bytecode[index];
        codeInstr[index].m_instr = codeNum;
        instr = index;  // remember offset of instruction
        index++; // skip bytecode
        switch( bytecode[instr] )
        {
        case OPCODE_NOP:            /* 0x00 */
        case OPCODE_WIDE:           /* 0xc4 */
            break;
        case OPCODE_ACONST_NULL:    /* 0x01 */
            vf_opcode_aconst_null( code, pool );
            break;
        case OPCODE_SIPUSH:         /* 0x11 + s2 */
            index++;    // skip parameter (s2)
        case OPCODE_BIPUSH:         /* 0x10 + s1 */
            index++;    // skip parameter (s1)
        case OPCODE_ICONST_M1:      /* 0x02 */
        case OPCODE_ICONST_0:       /* 0x03 */
        case OPCODE_ICONST_1:       /* 0x04 */
        case OPCODE_ICONST_2:       /* 0x05 */
        case OPCODE_ICONST_3:       /* 0x06 */
        case OPCODE_ICONST_4:       /* 0x07 */
        case OPCODE_ICONST_5:       /* 0x08 */
            vf_opcode_iconst_n( code, pool );
            break;
        case OPCODE_LCONST_0:       /* 0x09 */
        case OPCODE_LCONST_1:       /* 0x0a */
            vf_opcode_lconst_n( code, pool );
            break;
        case OPCODE_FCONST_0:       /* 0x0b */
        case OPCODE_FCONST_1:       /* 0x0c */
        case OPCODE_FCONST_2:       /* 0x0d */
            vf_opcode_fconst_n( code, pool );
            break;
        case OPCODE_DCONST_0:       /* 0x0e */
        case OPCODE_DCONST_1:       /* 0x0f */
            vf_opcode_dconst_n( code, pool );
            break;
        case OPCODE_LDC:            /* 0x12 + u1 */
            // get constant pool index
            constIndex = (unsigned short)bytecode[index];
            // skip constant pool index (u1)
            index++;
            result = vf_opcode_ldcx( code, 1, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_LDC_W:          /* 0x13 + u2 */
        case OPCODE_LDC2_W:         /* 0x14 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_ldcx( code, bytecode[instr] - OPCODE_LDC, constIndex,  ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_ILOAD:          /* 0x15 + u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_iloadx( code, local, pool );
            break;
        case OPCODE_LLOAD:          /* 0x16 + u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local + 1, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_lloadx( code, local, pool );
            break;
        case OPCODE_FLOAD:          /* 0x17 + u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_floadx( code, local, pool );
            break;
        case OPCODE_DLOAD:          /* 0x18 + u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local + 1, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_dloadx( code, local, pool );
            break;
        case OPCODE_ALOAD:          /* 0x19 + u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_aloadx( code, local, pool );
            break;
        case OPCODE_ILOAD_0:        /* 0x1a */
        case OPCODE_ILOAD_1:        /* 0x1b */
        case OPCODE_ILOAD_2:        /* 0x1c */
        case OPCODE_ILOAD_3:        /* 0x1d */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_ILOAD_0);
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_iloadx( code, local, pool );
            break;
        case OPCODE_LLOAD_0:        /* 0x1e */
        case OPCODE_LLOAD_1:        /* 0x1f */
        case OPCODE_LLOAD_2:        /* 0x20 */
        case OPCODE_LLOAD_3:        /* 0x21 */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_LLOAD_0);
            result = vf_check_local_var_number( local + 1, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_lloadx( code, local, pool );
            break;
        case OPCODE_FLOAD_0:        /* 0x22 */
        case OPCODE_FLOAD_1:        /* 0x23 */
        case OPCODE_FLOAD_2:        /* 0x24 */
        case OPCODE_FLOAD_3:        /* 0x25 */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_FLOAD_0);
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_floadx( code, local, pool );
            break;
        case OPCODE_DLOAD_0:        /* 0x26 */
        case OPCODE_DLOAD_1:        /* 0x27 */
        case OPCODE_DLOAD_2:        /* 0x28 */
        case OPCODE_DLOAD_3:        /* 0x29 */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_DLOAD_0);
            result = vf_check_local_var_number( local + 1, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_dloadx( code, local, pool );
            break;
        case OPCODE_ALOAD_0:        /* 0x2a */
        case OPCODE_ALOAD_1:        /* 0x2b */
        case OPCODE_ALOAD_2:        /* 0x2c */
        case OPCODE_ALOAD_3:        /* 0x2d */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_ALOAD_0);
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_aloadx( code, local, pool );
            break;
        case OPCODE_IALOAD:         /* 0x2e */
            vf_opcode_iaload( code, ctex );
            break;
        case OPCODE_BALOAD:         /* 0x33 */
            vf_opcode_baload( code, ctex );
            break;
        case OPCODE_CALOAD:         /* 0x34 */
            vf_opcode_caload( code, ctex );
            break;
        case OPCODE_SALOAD:         /* 0x35 */
            vf_opcode_saload( code, ctex );
            break;
        case OPCODE_LALOAD:         /* 0x2f */
            vf_opcode_laload( code, ctex );
            break;
        case OPCODE_FALOAD:         /* 0x30 */
            vf_opcode_faload( code, ctex );
            break;
        case OPCODE_DALOAD:         /* 0x31 */
            vf_opcode_daload( code, ctex );
            break;
        case OPCODE_AALOAD:         /* 0x32 */
            vf_opcode_aaload( code, ctex );
            break;
        case OPCODE_ISTORE:         /* 0x36 + u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_istorex( code, local, pool );
            break;
        case OPCODE_LSTORE:         /* 0x37 + u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local + 1, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_lstorex( code, local, pool );
            break;
        case OPCODE_FSTORE:         /* 0x38 +  u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_fstorex( code, local, pool );
            break;
        case OPCODE_DSTORE:         /* 0x39 +  u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local + 1, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_dstorex( code, local, pool );
            break;
        case OPCODE_ASTORE:         /* 0x3a + u1|u2 */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_astorex( code, local, pool );
            break;
        case OPCODE_ISTORE_0:       /* 0x3b */
        case OPCODE_ISTORE_1:       /* 0x3c */
        case OPCODE_ISTORE_2:       /* 0x3d */
        case OPCODE_ISTORE_3:       /* 0x3e */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_ISTORE_0);
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_istorex( code, local, pool );
            break;
        case OPCODE_LSTORE_0:       /* 0x3f */
        case OPCODE_LSTORE_1:       /* 0x40 */
        case OPCODE_LSTORE_2:       /* 0x41 */
        case OPCODE_LSTORE_3:       /* 0x42 */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_LSTORE_0);
            result = vf_check_local_var_number( local + 1, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_lstorex( code, local, pool );
            break;
        case OPCODE_FSTORE_0:       /* 0x43 */
        case OPCODE_FSTORE_1:       /* 0x44 */
        case OPCODE_FSTORE_2:       /* 0x45 */
        case OPCODE_FSTORE_3:       /* 0x46 */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_FSTORE_0);
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_fstorex( code, local, pool );
            break;
        case OPCODE_DSTORE_0:       /* 0x47 */
        case OPCODE_DSTORE_1:       /* 0x48 */
        case OPCODE_DSTORE_2:       /* 0x49 */
        case OPCODE_DSTORE_3:       /* 0x4a */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_DSTORE_0);
            result = vf_check_local_var_number( local + 1, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_dstorex( code, local, pool );
            break;
        case OPCODE_ASTORE_0:       /* 0x4b */
        case OPCODE_ASTORE_1:       /* 0x4c */
        case OPCODE_ASTORE_2:       /* 0x4d */
        case OPCODE_ASTORE_3:       /* 0x4e */
            // get number of local variable
            local = (unsigned short)(bytecode[instr] - OPCODE_ASTORE_0);
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_astorex( code, local, pool );
            break;
        case OPCODE_IASTORE:        /* 0x4f */
            vf_opcode_iastore( code, ctex );
            break;
        case OPCODE_BASTORE:        /* 0x54 */
            vf_opcode_bastore( code, ctex );
            break;
        case OPCODE_CASTORE:        /* 0x55 */
            vf_opcode_castore( code, ctex );
            break;
        case OPCODE_SASTORE:        /* 0x56 */
            vf_opcode_sastore( code, ctex );
            break;
        case OPCODE_LASTORE:        /* 0x50 */
            vf_opcode_lastore( code, ctex );
            break;
        case OPCODE_FASTORE:        /* 0x51 */
            vf_opcode_fastore( code, ctex );
            break;
        case OPCODE_DASTORE:        /* 0x52 */
            vf_opcode_dastore( code, ctex );
            break;
        case OPCODE_AASTORE:        /* 0x53 */
            vf_opcode_aastore( code, ctex );
            break;
        case OPCODE_POP:            /* 0x57 */
            vf_opcode_pop( code, pool );
            break;
        case OPCODE_POP2:           /* 0x58 */
            vf_opcode_pop2( code, pool );
            break;
        case OPCODE_DUP:            /* 0x59 */
            vf_opcode_dup( code, pool );
            break;
        case OPCODE_DUP_X1:         /* 0x5a */
            vf_opcode_dup_x1( code, pool );
            break;
        case OPCODE_DUP_X2:         /* 0x5b */
            vf_opcode_dup_x2( code, pool );
            break;
        case OPCODE_DUP2:           /* 0x5c */
            vf_opcode_dup2( code, pool );
            break;
        case OPCODE_DUP2_X1:        /* 0x5d */
            vf_opcode_dup2_x1( code, pool );
            break;
        case OPCODE_DUP2_X2:        /* 0x5e */
            vf_opcode_dup2_x2( code, pool );
            break;
        case OPCODE_SWAP:           /* 0x5f */
            vf_opcode_swap( code, pool );
            break;
        case OPCODE_IADD:           /* 0x60 */
        case OPCODE_ISUB:           /* 0x64 */
        case OPCODE_IMUL:           /* 0x68 */
        case OPCODE_IDIV:           /* 0x6c */
        case OPCODE_IREM:           /* 0x70 */
            vf_opcode_iadd( code, pool );
            break;
        case OPCODE_LADD:           /* 0x61 */
        case OPCODE_LSUB:           /* 0x65 */
        case OPCODE_LMUL:           /* 0x69 */
        case OPCODE_LDIV:           /* 0x6d */
        case OPCODE_LREM:           /* 0x71 */
            vf_opcode_ladd( code, pool );
            break;
        case OPCODE_FADD:           /* 0x62 */
        case OPCODE_FSUB:           /* 0x66 */
        case OPCODE_FMUL:           /* 0x6a */
        case OPCODE_FDIV:           /* 0x6e */
        case OPCODE_FREM:           /* 0x72 */
            vf_opcode_fadd( code, pool );
            break;
        case OPCODE_DADD:           /* 0x63 */
        case OPCODE_DSUB:           /* 0x67 */
        case OPCODE_DMUL:           /* 0x6b */
        case OPCODE_DDIV:           /* 0x6f */
        case OPCODE_DREM:           /* 0x73 */
            vf_opcode_dadd( code, pool );
            break;
        case OPCODE_INEG:           /* 0x74 */
            vf_opcode_ineg( code, pool );
            break;
        case OPCODE_LNEG:           /* 0x75 */
            vf_opcode_lneg( code, pool );
            break;
        case OPCODE_FNEG:           /* 0x76 */
            vf_opcode_fneg( code, pool );
            break;
        case OPCODE_DNEG:           /* 0x77 */
            vf_opcode_dneg( code, pool );
            break;
        case OPCODE_ISHL:           /* 0x78 */
        case OPCODE_ISHR:           /* 0x7a */
        case OPCODE_IUSHR:          /* 0x7c */
        case OPCODE_IAND:           /* 0x7e */
        case OPCODE_IOR:            /* 0x80 */
        case OPCODE_IXOR:           /* 0x82 */
            vf_opcode_ibit( code, pool );
            break;
        case OPCODE_LSHL:           /* 0x79 */
        case OPCODE_LSHR:           /* 0x7b */
        case OPCODE_LUSHR:          /* 0x7d */
            vf_opcode_lshx( code, pool );
            break;
        case OPCODE_LAND:           /* 0x7f */
        case OPCODE_LOR:            /* 0x81 */
        case OPCODE_LXOR:           /* 0x83 */
            vf_opcode_land( code, pool );
            break;
        case OPCODE_IINC:           /* 0x84 + u1|u2 + s1|s2 */
            count = index;
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            count = index - count;
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_iinc( code, local, pool );
            // skip 2nd parameter (s1|s2)
            index += count;
            break;
        case OPCODE_I2L:            /* 0x85 */
            vf_opcode_i2l( code, pool );
            break;
        case OPCODE_I2F:            /* 0x86 */
            vf_opcode_i2f( code, pool );
            break;
        case OPCODE_I2D:            /* 0x87 */
            vf_opcode_i2d( code, pool );
            break;
        case OPCODE_L2I:            /* 0x88 */
            vf_opcode_l2i( code, pool );
            break;
        case OPCODE_L2F:            /* 0x89 */
            vf_opcode_l2f( code, pool );
            break;
        case OPCODE_L2D:            /* 0x8a */
            vf_opcode_l2d( code, pool );
            break;
        case OPCODE_F2I:            /* 0x8b */
            vf_opcode_f2i( code, pool );
            break;
        case OPCODE_F2L:            /* 0x8c */
            vf_opcode_f2l( code, pool );
            break;
        case OPCODE_F2D:            /* 0x8d */
            vf_opcode_f2d( code, pool );
            break;
        case OPCODE_D2I:            /* 0x8e */
            vf_opcode_d2i( code, pool );
            break;
        case OPCODE_D2L:            /* 0x8f */
            vf_opcode_d2l( code, pool );
            break;
        case OPCODE_D2F:            /* 0x90 */
            vf_opcode_d2f( code, pool );
            break;
        case OPCODE_I2B:            /* 0x91 */
        case OPCODE_I2C:            /* 0x92 */
        case OPCODE_I2S:            /* 0x93 */
            vf_opcode_i2b( code, pool );
            break;
        case OPCODE_LCMP:           /* 0x94 */
            vf_opcode_lcmp( code, pool );
            break;
        case OPCODE_FCMPL:          /* 0x95 */
        case OPCODE_FCMPG:          /* 0x96 */
            vf_opcode_fcmpx( code, pool );
            break;
        case OPCODE_DCMPL:          /* 0x97 */
        case OPCODE_DCMPG:          /* 0x98 */
            vf_opcode_dcmpx( code, pool );
            break;
        case OPCODE_IFEQ:           /* 0x99 + s2 */
        case OPCODE_IFNE:           /* 0x9a + s2 */
        case OPCODE_IFLT:           /* 0x9b + s2 */
        case OPCODE_IFGE:           /* 0x9c + s2 */
        case OPCODE_IFGT:           /* 0x9d + s2 */
        case OPCODE_IFLE:           /* 0x9e + s2 */
            offset = vf_get_double_hword_branch_offset( &codeInstr[instr],
                            instr, bytecode, &index, pool );
            result = vf_check_branch_offset( offset, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            result = vf_check_branch_offset( index, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_ifeq( code, &codeInstr[offset],
                            &codeInstr[index], pool );
            break;
        case OPCODE_IF_ICMPEQ:      /* 0x9f + s2 */
        case OPCODE_IF_ICMPNE:      /* 0xa0 + s2 */
        case OPCODE_IF_ICMPLT:      /* 0xa1 + s2 */
        case OPCODE_IF_ICMPGE:      /* 0xa2 + s2 */
        case OPCODE_IF_ICMPGT:      /* 0xa3 + s2 */
        case OPCODE_IF_ICMPLE:      /* 0xa4 + s2 */
            offset = vf_get_double_hword_branch_offset( &codeInstr[instr],
                            instr, bytecode, &index, pool );
            result = vf_check_branch_offset( offset, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            result = vf_check_branch_offset( index, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_if_icmpeq( code, &codeInstr[offset],
                                 &codeInstr[index], pool );
            break;
        case OPCODE_IF_ACMPEQ:      /* 0xa5 + s2 */
        case OPCODE_IF_ACMPNE:      /* 0xa6 + s2 */
            offset = vf_get_double_hword_branch_offset( &codeInstr[instr],
                            instr, bytecode, &index, pool );
            result = vf_check_branch_offset( offset, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            result = vf_check_branch_offset( index, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_if_acmpeq( code, &codeInstr[offset],
                                 &codeInstr[index], pool );
            break;
        case OPCODE_GOTO:           /* 0xa7 + s2 */
            offset = vf_get_single_hword_branch_offset( &codeInstr[instr], 
                            instr, bytecode, &index, pool );
            result = vf_check_branch_offset( offset, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_set_basic_block_flag( &codeInstr[offset] );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_JSR:            /* 0xa8 + s2 */
            offset = vf_get_double_hword_branch_offset( &codeInstr[instr],
                            instr, bytecode, &index, pool );
            result = vf_check_branch_offset( offset, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            result = vf_check_branch_offset( index, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_jsr( code, codeNum, pool );
            ctex->m_dump.m_with_subroutine = 1;
            vf_set_basic_block_flag( &codeInstr[offset] );
            vf_set_basic_block_flag( &codeInstr[index] );
            break;
        case OPCODE_RET:            /* 0xa9 + u1|u2  */
            local = (unsigned short)vf_get_local_var_number( code, bytecode, &index );
            result = vf_check_local_var_number( local, locals, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_ret( code, local, pool );
            // create and set edge branch to exit
            vf_set_single_instruction_offset( &codeInstr[instr], ~0U, pool );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_TABLESWITCH:    /* 0xaa + pad + s4 * (3 + N) */
            vf_opcode_switch( code, pool );
            branches = vf_set_tableswitch_offsets( &codeInstr[instr],
                                    instr, &index, bytecode, pool);
            // check tableswitch branches and set begin of basic blocks
            for( count = 0; count < branches; count++ )
            {
                offset = vf_get_instruction_branch( &codeInstr[instr], count );
                result = vf_check_branch_offset( offset, len, ctex );
                if( result != VER_OK ) {
                    goto labelEnd_vf_parse_bytecode;
                }
                vf_set_basic_block_flag( &codeInstr[offset] );
            }
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_LOOKUPSWITCH:   /* 0xab + pad + s4 * 2 * (N + 1) */
            vf_opcode_switch( code, pool );
            result = vf_set_lookupswitch_offsets( &codeInstr[instr],
                                    instr, &index, bytecode, &branches, ctex);
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            // check tableswitch branches and set begin of basic blocks
            for( count = 0; count < branches; count++ )
            {
                offset = vf_get_instruction_branch( &codeInstr[instr], count );
                result = vf_check_branch_offset( offset, len, ctex );
                if( result != VER_OK ) {
                    goto labelEnd_vf_parse_bytecode;
                }
                vf_set_basic_block_flag( &codeInstr[offset] );
            }
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_IRETURN:        /* 0xac */
            vf_opcode_ireturn( code, pool );
            // create and set edge branch to exit
            vf_set_single_instruction_offset( &codeInstr[instr], ~0U, pool );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_LRETURN:        /* 0xad */
            vf_opcode_lreturn( code, pool );
            // create and set edge branch to exit
            vf_set_single_instruction_offset( &codeInstr[instr], ~0U, pool );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_FRETURN:        /* 0xae */
            vf_opcode_freturn( code, pool );
            // create and set edge branch to exit
            vf_set_single_instruction_offset( &codeInstr[instr], ~0U, pool );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_DRETURN:        /* 0xaf */
            vf_opcode_dreturn( code, pool );
            // create and set edge branch to exit
            vf_set_single_instruction_offset( &codeInstr[instr], ~0U, pool );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_ARETURN:        /* 0xb0 */
            vf_opcode_areturn( code, pool );
            // create and set edge branch to exit
            vf_set_single_instruction_offset( &codeInstr[instr], ~0U, pool );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_RETURN:         /* 0xb1 */
            // set instruction flag
            vf_set_instruction_flag( code, VF_FLAG_RETURN );
            // create and set edge branch to exit
            vf_set_single_instruction_offset( &codeInstr[instr], ~0U, pool );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_GETSTATIC:      /* 0xb2 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_getstatic( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_PUTSTATIC:      /* 0xb3 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_putstatic( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_GETFIELD:       /* 0xb4 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_getfield( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_PUTFIELD:       /* 0xb5 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_putfield( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_INVOKEVIRTUAL:  /* 0xb6 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_invoke( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_INVOKESPECIAL:  /* 0xb7 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_invokespecial( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_INVOKESTATIC:   /* 0xb8 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_invoke( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_INVOKEINTERFACE:/* 0xb9 + u2 + u1 + u1 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_invoke( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            // check the number of arguments and last opcode byte
            if( ctex->m_code[codeNum].m_inlen != (unsigned short)bytecode[index]
               || bytecode[index + 1] != 0 )
            {
                VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Incorrect operand byte of invokeinterface");
                result = VER_ErrorInstruction;
                goto labelEnd_vf_parse_bytecode;
            }
            // skip 2 parameters (u1 + u1)
            index += 1 + 1;
            break;
        case OPCODE_NEW:            /* 0xbb + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_new( code, constIndex, codeNum, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_NEWARRAY:       /* 0xbc + u1 */
            // get array type
            number = (int)bytecode[index];
            // skip parameter (u1)
            index++;
            result = vf_opcode_newarray( code, (unsigned char)number, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_ANEWARRAY:      /* 0xbd + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_anewarray( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_ARRAYLENGTH:    /* 0xbe */
            vf_opcode_arraylength( code, ctex );
            break;
        case OPCODE_ATHROW:         /* 0xbf */
            vf_opcode_athrow( code, ctex );
            // create and set edge branch to exit
            vf_set_single_instruction_offset( &codeInstr[instr], ~0U, pool );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_CHECKCAST:      /* 0xc0 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_checkcast( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_INSTANCEOF:     /* 0xc1 + u2 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            result = vf_opcode_instanceof( code, constIndex, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_MONITORENTER:   /* 0xc2 */
        case OPCODE_MONITOREXIT:    /* 0xc3 */
            vf_opcode_monitorx( code, pool );
            break;
        case OPCODE_MULTIANEWARRAY: /* 0xc5 + u2 + u1 */
            // get constant pool index
            constIndex = (unsigned short)( (bytecode[index] << 8)|(bytecode[index + 1]) );
            // skip constant pool index (u2)
            index += 2;
            // get dimensions of array
            number = (int)bytecode[index];
            // skip dimensions of array (u1)
            index++;
            result = vf_opcode_multianewarray( code, constIndex, (unsigned short)number, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            break;
        case OPCODE_IFNULL:         /* 0xc6 + s2 */
        case OPCODE_IFNONNULL:      /* 0xc7 + s2 */
            offset = vf_get_double_hword_branch_offset( &codeInstr[instr],
                            instr, bytecode, &index, pool );
            result = vf_check_branch_offset( offset, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            result = vf_check_branch_offset( index, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_ifxnull( &ctex->m_code[codeNum], &codeInstr[offset],
                               &codeInstr[index], pool );
            break;
        case OPCODE_GOTO_W:         /* 0xc8 + s4 */
            offset = vf_get_single_word_branch_offset( &codeInstr[instr], 
                            instr, bytecode, &index, pool );
            result = vf_check_branch_offset( offset, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_set_basic_block_flag( &codeInstr[offset] );
            if( index < len ) {
                vf_set_basic_block_flag( &codeInstr[index] );
            }
            break;
        case OPCODE_JSR_W:          /* 0xc9 + s4 */
            offset = vf_get_double_word_branch_offset( &codeInstr[instr],
                            instr, bytecode, &index, pool );
            result = vf_check_branch_offset( offset, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            result = vf_check_branch_offset( index, len, ctex );
            if( result != VER_OK ) {
                goto labelEnd_vf_parse_bytecode;
            }
            vf_opcode_jsr( code, codeNum, pool );
            ctex->m_dump.m_with_subroutine = 1;
            vf_set_basic_block_flag( &codeInstr[offset] );
            vf_set_basic_block_flag( &codeInstr[index] );
            break;
        case _OPCODE_UNDEFINED:     /* 0xba */
        default:
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Unknown instruction bytecode" );
            result = VER_ErrorInstruction;
            goto labelEnd_vf_parse_bytecode;
        }
    }

    /**
     * Create end-entry instruction
     */
    code = ctex->m_code;
    vf_create_end_entry( &code[codeNum], ctex );
    codeInstr[index].m_instr = codeNum;
    codeNum++;

    /**
     * Set handler basic blocks
     */
    edges = 0;
    for( index = 0; index < handlcount; index++ ) {
        // check instruction range
        unsigned short start_pc;
        unsigned short end_pc;
        unsigned short handler_pc;
        unsigned short handler_cp_index;
        method_get_exc_handler_info( ctex->m_method, (unsigned short)index, &start_pc, &end_pc,
            &handler_pc, &handler_cp_index );
        if( ( start_pc >= len ) || ( end_pc > len ) || ( handler_pc >= len ) )
        {
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Handler pc is out of range" );
            result = VER_ErrorHandler;
            goto labelEnd_vf_parse_bytecode;
        }
        // check constant pool index
        CHECK_HANDLER_CONST_POOL_ID( handler_cp_index, constlen, ctex );
        CHECK_HANDLER_CONST_POOL_CLASS( ctex, handler_cp_index );
        // check instruction relations
        if( (codeInstr[ start_pc ].m_instr == 0)
            || (codeInstr[ end_pc ].m_instr == 0) 
            || (codeInstr[ handler_pc ].m_instr == 0) )
        {
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Handler pc is out of instruction set" );
            result = VER_ErrorHandler;
            goto labelEnd_vf_parse_bytecode;
        }
        // set handler basic blocks
        codeInstr[ start_pc ].m_mark = 1;
        codeInstr[ end_pc ].m_mark = 1;
        codeInstr[ handler_pc ].m_mark = 1;

        /**
         * Set handler branch offset
         */
        edges++;
        code[index + 1].m_offcount = 1;
        code[index + 1].m_off = (unsigned*)vf_alloc_pool_memory( pool, 
                                                sizeof(unsigned) );
        // fill offset array for code instruction
        code[index + 1].m_off[0] = codeInstr[ handler_pc ].m_instr;
        // create handler valid type
        vf_ValidType_t *type = NULL;
        if( handler_cp_index ) {
            const char* name = vf_get_cp_class_name( handler_cp_index, ctex );
            assert(name);

            type = vf_create_class_valid_type( name, ctex );
            // set restriction for handler class
            if( ctex->m_vtype.m_throwable->string[0] != type->string[0] ) {
                ctex->m_type->SetRestriction( ctex->m_vtype.m_throwable->string[0],
                    type->string[0], 0, VF_CHECK_SUPER );
            }
        }
        // create out vector for handler
        vf_new_out_vector( &code[index + 1], 1, pool );
        vf_set_out_vector_stack_entry_ref( &code[index + 1], 0, type );

        /** 
         * Set handler branches
         * Set handler branches to last instructions of basic blocks
         */
        for( count = start_pc + 1; count <= end_pc; count++ ) {
            if( count < len && codeInstr[count].m_mark ) {
                // calculate code instruction number
                instr = codeInstr[count].m_instr - 1;
                // check existence of handler array
                if( code[instr].m_handler == NULL ) {
                    // create handler array for code instruction
                    code[instr].m_handler = 
                        (unsigned char*)vf_alloc_pool_memory( pool,
                        handlcount * sizeof(unsigned char) );
                }
                // count handler edges
                edges++;
                // set handler branch
                code[instr].m_handler[index] = 1;
            }
        }
    }

    /** 
     * Initialize basic block count
     * Include start-entry basic block, handler basic blocks, 
     * end-entry basic block.
     */
    bbCount = 1 + handlcount + 1;

    /**
     * Set code offsets
     * Check code instructions
     */
    for( index = 0; index < len; index++ ) {
        if( !index || codeInstr[index].m_mark ) {
            // first instruction is always begin of basic block
            if( (count = codeInstr[index].m_instr) == 0 ) {
                VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Illegal target of jump or branch" );
                result = VER_ErrorBranch;
                goto labelEnd_vf_parse_bytecode;
            }
            vf_set_instruction_flag( &code[count], VF_FLAG_BEGIN_BASIC_BLOCK );
            bbCount++;
            if( !index || code[count - 1].m_offcount == 0 ) {
                // considering first edge from start-entry to first instruction
                // count only edges that catenate 2 nearby instructions
                edges++;
            }
        }
        if( codeInstr[index].m_offcount ) {
            // calculate code instruction number
            instr = codeInstr[index].m_instr;
            // create offset array for code instruction
            edges += codeInstr[index].m_offcount;
            code[instr].m_offcount = codeInstr[index].m_offcount;
            code[instr].m_off = codeInstr[index].m_off;
            // fill offset array for code instruction
            for( count = 0; count < codeInstr[index].m_offcount; count++ ) {
                offset = codeInstr[index].m_off[count];
                if( offset == -1 ) {
                    code[instr].m_off[count] = codeNum - 1;
                } else {
                    code[instr].m_off[count] = codeInstr[offset].m_instr;
                }
            }
        }
    }

labelEnd_vf_parse_bytecode:

    /**
     * Free allocated memory
     */
    ctex->m_codeNum = codeNum;
    ctex->m_nodeNum = bbCount;
    ctex->m_edgeNum = edges;

#if _VERIFY_DEBUG
    if( ctex->m_dump.m_code ) {
        vf_dump_bytecode( ctex );
    }
#endif // _VERIFY_DEBUG

    return result;
} // vf_parse_bytecode

#if _VERIFY_DEBUG

/**
 * Function prints code instruction array in stream.
 */
void
vf_dump_bytecode( vf_Context_t *ctex )  // verifier context
{
    unsigned char* bytecode;
    unsigned index,
             count,
             handlcount;
    vf_Code_t *code = ctex->m_code;
    
    bytecode = method_get_bytecode( ctex->m_method );
    handlcount = method_get_exc_handler_number( ctex->m_method );
    VERIFY_DEBUG( "======================== VERIFIER METHOD DUMP ========================" );
    VERIFY_DEBUG( "Method: " << class_get_name( ctex->m_class )
              << "." << method_get_name( ctex->m_method )
              << method_get_descriptor( ctex->m_method ) << endl );
    VERIFY_DEBUG( "0 [-]: -> START-ENTRY" );
    for( index = 0; index < handlcount; index++ ) {
        VERIFY_DEBUG( index + 1 << " [-]: -> HANDLER #" << index + 1 );
        if( code[index + 1].m_offcount ) {
            for( count = 0; count < code[index + 1].m_offcount; count++ )
                if( code[index + 1].m_off[count] == ctex->m_codeNum - 1 ) {
                    VERIFY_DEBUG( " --> " << code[index + 1].m_off[count] << " [-]" );
                } else {
                    VERIFY_DEBUG( " --> " << code[index + 1].m_off[count]
                                   << " [" 
                                   << code[ code[index + 1].m_off[count] ].m_addr 
                                        - bytecode  << "]" );
                }
        }
    }
    for( index = handlcount + 1; index < ctex->m_codeNum - 1; index++ ) {
        VERIFY_DEBUG( index << " [" << code[index].m_addr - bytecode << "]:"
                     << (vf_is_begin_basic_block( &code[index] ) ? " -> " : "    ")
                     << ((code[index].m_stack < 0) ? "" : " " )
                     << code[index].m_stack 
                     << "|" << code[index].m_minstack << "    "
                     << vf_opcode_names[*(code[index].m_addr)] );
        if( code[index].m_offcount ) {
            for( count = 0; count < code[index].m_offcount; count++ )
                if( code[index].m_off[count] == ctex->m_codeNum - 1 ) {
                    VERIFY_DEBUG( " --> " << code[index].m_off[count] << " [-]" );
                } else {
                    VERIFY_DEBUG( " --> " << code[index].m_off[count]
                                   << " [" 
                                   << code[ code[index].m_off[count] ].m_addr 
                                        - bytecode  << "]" );
                }
        }
    }
    VERIFY_DEBUG( index << " [-]: -> END-ENTRY" << endl );
    VERIFY_DEBUG( "======================================================================" );
    return;
} // vf_dump_bytecode
#endif //_VERIFY_DEBUG

} // namespace Verifier

/**
 * Function provides initial verification of class.
 */
Verifier_Result
vf_verify_class( class_handler klass,      // verified class
                 unsigned verifyAll,       // verification level flag
                 char **message)           // verifier error message
{
    assert(klass);
    assert(message);
#if VERIFY_CLASS
    if( strcmp( class_get_name( klass ), "" ) ) {
        return VER_OK;
    }
#endif // VERIFY_CLASS

    VERIFY_TRACE( "class.bytecode", "verify class: " << class_get_name( klass ) );

    /**
     * Create context
     */
    vf_Context_t context;

    /**
     * Set current class
     */
    context.m_class = klass;

    /**
     * Create type pool
     */
    context.m_type = new vf_TypePool();

    /**
     * Create memory pool
     */
    context.m_pool = vf_create_pool();

    /**
     * Set valid types
     */
    const char *class_name = class_get_name( klass );
    unsigned class_name_len = strlen( class_name );
    char *type_name = (char*)STD_ALLOCA( class_name_len + 1 + 1 );
    // it will be a funny name for array :)
    memcpy( &type_name[1], class_name, class_name_len );
    type_name[0] = 'L';
    type_name[class_name_len + 1] = '\0';
    context.m_vtype.m_class = context.m_type->NewType( type_name, class_name_len + 1 );
    context.m_vtype.m_throwable = context.m_type->NewType( "Ljava/lang/Throwable", 20 );
    context.m_vtype.m_object = context.m_type->NewType( "Ljava/lang/Object", 17 );
    context.m_vtype.m_array = context.m_type->NewType( "[Ljava/lang/Object", 18 );
    context.m_vtype.m_clone = context.m_type->NewType( "Ljava/lang/Cloneable", 20 );
    context.m_vtype.m_serialize = context.m_type->NewType( "Ljava/io/Serializable", 21 );

    /**
     * Set verification level flag
     */
    context.m_dump.m_verify = verifyAll ? 1 : 0;

    /**
     * Verify bytecode of methods
     */
    Verifier_Result result = VER_OK;
    unsigned short number = class_get_method_number( klass );
    for( unsigned short index = 0; index < number; index++ ) {
#if VERIFY_METHOD
        if( !strcmp( method_get_name( class_get_method( klass, index ) ), "" ) )
#endif // VERIFY_METHOD
        {
            /**
             * Set verified method
             */
            context.m_method = class_get_method( klass, index );
            //context.m_dump.m_node_vector = 1;
            //context.m_dump.m_code_vector = 1;
            //context.m_dump.m_merge_vector = 1;
            result = vf_verify_method_bytecode( &context );
            context.ClearContext();
        }
        if( result == VER_NoSupportJSR ) {
            result = VER_OK;
        } else  if (result != VER_OK ) {
            goto labelEnd_verifyClass;
        }
    }

    /**
     * Check and set class constraints
     */
#if _VERIFY_DEBUG
    if( context.m_dump.m_constraint ) {
        context.m_type->DumpTypeConstraints( NULL );
    }
#endif // _VERIFY_DEBUG
    result = vf_check_class_constraints( &context );
    if( result != VER_OK ) {
        goto labelEnd_verifyClass;
    }
    
labelEnd_verifyClass:
    vf_delete_pool( context.m_pool );
    delete context.m_type;
    *message = context.m_error;
#if _VERIFY_DEBUG
    if( result != VER_OK ) {
        TRACE2("verifier", "VerifyError: " << (context.m_error ? context.m_error : "NULL") );
    }
#endif // _VERIFY_DEBUG

    return result;
} // vf_verify_class
