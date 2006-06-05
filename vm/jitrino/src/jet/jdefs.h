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
 * @version $Revision: 1.3.12.3.4.4 $
 */
 
/**
 * @file
 * @brief Common definitions used across the Jitrino.JET.
 * 
 * Contains common definitions used across the Jitrino.JET - VM types, some 
 * constants and related data and methods.
 */
 
#if !defined(__JDEFS_H_INCLUDED__)
#define __JDEFS_H_INCLUDED__

#include "open/types.h"
#include "open/bytecodes.h"
#include <assert.h>
#include <climits>

/**
 * @def MAX_REGS
 * @brief maximum number of registers (of any kind) available on the current 
 *        platform.
 */
#define MAX_REGS    8 // todo: move to encoder


namespace Jitrino {
namespace Jet {

#ifdef PLATFORM_POSIX
    /**
     * @brief Defines int64 constant.
     */
    #define MK_I64(a)   ((jlong)(a ## LL))
#else
    #define MK_I64(a)   ((jlong)(a ## L))
    #define snprintf    _snprintf
    #define vsnprintf    _vsnprintf
#endif


// Nothing (?) portable is defined by lib.c.limits for 64bit integers, thus 
// declaring our own.

/**
 * @brief Represents a Java's long aka 'signed int64'.
 */
typedef long long   jlong;

/**
 * @brief A maximum, positive value a #jlong can have.
 */
#define jLONG_MAX   MK_I64(0x7FFFFFFFFFFFFFFF)

/**
 * @brief A minimum, negative value a #jlong can have.
 */
#define jLONG_MIN   MK_I64(0x8000000000000000)

/**
 * @brief Empty value.
 *
 * Normally used when zero is not applicable to signal empty/non-initialized
 * value i.e. for PC values.
 */
#define NOTHING                   (~(unsigned)0)

/**
 * @brief Extracts lower 32 bits from the given 64 bits value.
 */
inline int lo32(jlong jl) { return (int)(jl & 0xFFFFFFFF); };

/**
 * @brief Extracts higher 32 bits from the given 64 bits value.
 */
inline int hi32(jlong jl) { return (int)(jl>>32); };

/**
 * @brief Size of the platform's machine word, in bits.
 */
const unsigned WORD_SIZE = sizeof(long)*CHAR_BIT;

/**
 * @brief Returns word index for a given index in bit array.
 */
inline unsigned word_no(unsigned idx)
{
    return idx/WORD_SIZE;
}

/**
 * @brief Returns bit index in a word for the given index in a bit array.
 */
inline unsigned bit_no(unsigned idx)
{
    return idx%WORD_SIZE;
}
/**
 * @brief Returns number of words needed to store the given number of bits.
 */
inline unsigned words(unsigned num)
{
    return (num+WORD_SIZE-1)/WORD_SIZE;
}

/**
 * @brief Sets a bit in the bit array at the specified position.
 * @param p - pointer the the bit array
 * @param idx - index of the bit
 */
inline void set(long * p, unsigned idx)
{
    p[ word_no(idx) ] |= 1<<bit_no(idx);
}

/**
 * @brief Clears a bit in the bit array at the specified position.
 * @param p - pointer the the bit array
 * @param idx - index of the bit
 */
inline void clr(long * p, unsigned idx)
{
    p[ word_no(idx) ] &= ~(1<<bit_no(idx));
}

/**
 * @brief Tests a bit in the provided bit array.
 * @param p - pointer the the bit array
 * @param idx - index of the bit
 * @return \b true if the bit set, \b false otherwise
 */
inline bool tst(const long * p, unsigned idx)
{
    return 0 != (p[word_no(idx)] & (1<<bit_no(idx)));
}

/**
 * @defgroup JITRINO_JET_MEM_TRANSF_FLAGS Flags that control spill to and \
 *           uploading from memory.
 * 
 * A bunch of flags controlling \link Jitrino::Jet::Compiler::gen_mem a 
 * function \endlink which generates unloading and uploading of registers 
 * to/from the memory.
 *
 * @{
 */

/**
 * @brief Unload registers to memory.
 */
#define MEM_TO_MEM      (0x00000001)
/**
 * @brief Upload registers from memory.
 */
#define MEM_FROM_MEM    (0x00000002)
/**
 * @brief Process local variables.
 */
#define MEM_VARS        (0x00000004)
/**
 * @brief Process stack items.
 */
#define MEM_STACK       (0x00000008)
/**
 * @brief Update state of the processed item.
 */
#define MEM_UPDATE      (0x00000010)
/**
 * @brief Do not update state of the processed item.
 */
#define MEM_NO_UPDATE   (0x00000020)
/**
 * @brief Inverse an order in which items are selected for update.
 *
 * Can only be used with MEM_FROM_MEM and MEM_NO_UPDATE.
 *
 * When specified, an items marked as 'on register' are \b uploaded
 * from the memory.
 */
#define MEM_INVERSE     (0x00000040)


///@} // ~ JITRINO_JET_MEM_TRANSF_FLAGS

/**
 * @defgroup JITRINO_JET_JAVA_METHOD_FLAGS Compilation control flags
 *
 * A bunch of flags, a Java method may be compiled with. Some of them also
 * affect runtime of the method.
 * 
 * Various java method flags used during compilation and some of them are 
 * also used at runtime.
 *
 * JMF_ stands for Java method's flag.
 */
 
/// @{ 

/** @brief Method reports 'this' during root set enumeration.*/
#define JMF_REPORT_THIS     (0x00000001)

/** @brief Generate code to perform a GC pooling on back branches.*/
#define JMF_BBPOOLING       (0x00000002)

/** @brief Generate profiling code for back branches and method entry.*/
#define JMF_PROF_ENTRY_BE   (0x00000004)

/** @brief Resolve classes during runtime instead of compile time. */
#define JMF_LAZY_RESOLUTION (0x00000008)

#ifdef JET_PROTO
/** @brief Aligns stack */
#define JMF_ALIGN_STACK     (0x00000010)
#else
// experimental feature, not for production build
#define JMF_ALIGN_STACK     (0)	
#endif

/** @brief Empty root set - method reports nothing to GC.*/
#define JMF_EMPTY_RS        (0x00000020)

/**
 * @brief Generate code so back branches and method entry counters get
 *        checked synchronously, during runtime, at method entrance. 
 */
#define JMF_PROF_SYNC_CHECK (0x00000040)

/**
 * @defgroup JITRINO_JET_DEBUG_METHOD_FLAGS Debugging flags
 *
 * These flags are also 'Java method's flags' but used for the debugging 
 * purposes only.
 *
 * Equal to zero in release mode, so a test against it (xx & DBG_ ) 
 * effectively leads to zero at compile time and thus the debug tracing code
 * is removed by optimizing compiler.
 *
 * These flags may be turned on in release mode if \b JIT_LOGS macro defined.
 *
 * @{
 */
#if defined(_DEBUG) || defined(JIT_LOGS) || defined(JET_PROTO)
    /** @brief Break at method's entry.*/
    #define DBG_BRK             (0x00100000)
    /** @brief Trace method's enter/exit.*/
    #define DBG_TRACE_EE        (0x00200000)
    /** @brief Trace execution of each bytecode instruction. */
    #define DBG_TRACE_BC        (0x00400000)
    /** 
     * @brief Trace runtime support events - stack unwinding, root set 
     * enumeration, byte code <-> native code mapping, etc.
     */
    #define DBG_TRACE_RT        (0x00800000)
    /** @brief Dump basic blocks, before code generation phase.*/
    #define DBG_DUMP_BBS        (0x01000000)
    /** @brief Trace code generation.*/
    #define DBG_TRACE_CG        (0x02000000)
    /** @brief Trace code layout (address ranges).*/
    #define DBG_TRACE_LAYOUT    (0x04000000)
    /** @brief Dump whole code after it's done.*/
    #define DBG_DUMP_CODE       (0x08000000)
    /** @brief Trace short summary about compiled method.*/
    #define DBG_TRACE_SUMM      (0x10000000)
    /** @brief Generates code to ensure stack integrity.*/
    #define DBG_CHECK_STACK     (0x20000000)
#else
    #define DBG_BRK             (0x00000000)
    #define DBG_TRACE_EE        (0x00000000)
    #define DBG_TRACE_BC        (0x00000000)
    #define DBG_TRACE_RT        (0x00000000)
    #define DBG_DUMP_BBS        (0x00000000)
    #define DBG_TRACE_CG        (0x00000000)
    #define DBG_TRACE_LAYOUT    (0x00000000)
    #define DBG_DUMP_CODE       (0x00000000)
    #define DBG_TRACE_SUMM      (0x00000000)
    #define DBG_CHECK_STACK     (0x00000000)
#endif
/// @{  //~JITRINO_JET_DEBUG_METHOD_FLAGS

/// @}  //~JITRINO_JET_JAVA_METHOD_FLAGS


/**
 * @brief Describes a kind/group of bytecode instruction, according to 
 *        the JVM Spec.
 */
enum InstrKind  {
    /// arithmetics
    ik_a,
    /// control transfer
    ik_cf,
    /// type conversion
    ik_cnv,
    /// load/store
    ik_ls,
    /// method invocation and return
    ik_meth,
    /// object creation and manipulation
    ik_obj,
    /// stack management
    ik_stack,
    /// throwing exceptions
    ik_throw,
    /// used for other opcodes like 'nop', 'wide' and 'unused'
    ik_none

};

/**
 * @brief (OPF stands for OPcode Flag) No special flags for the given opcode.
 */

#define OPF_NONE        (0x00000000)

/** 
 * @brief Opcode ends basic block (ATHROW/GOTOs/etc).
 */
#define OPF_ENDS_BB     (0x00000001)

/** 
 * @brief Opcode is a dead end in the control flow - no fall through - RET,
 *        GOTO, RETURN, ATHROW.
 */
#define OPF_DEAD_END    (0x00000002)

/**
 * @brief GC may happen on this instruction (calls to VM, method calls).
 */
#define OPF_GC_PT       (0x00000004)
/**
 * @brief Instruction terminates method (xRETURN or ATHROW).
 */
#define OPF_EXIT        (0x00000008)

/**
 * @brief An info associated with a byte code instruction.
 */
struct InstrDesc  {
    /**
     * @brief A kind of instruction. Used to groups processing of similar 
     *        instructions into same function.
     */
    InstrKind       ik;
#ifdef _DEBUG
    /**
     * @brief A byte code value. Only used internally in DEBUG mode to make 
     *        sure the \link #instrs array \endlink arranged properly.
     */
    JavaByteCodes   opcode;
#endif
    /**
     * @brief Total length of the instruction including additional bytes 
     *        (if any). 0 for 'wide' and for variable-length instructions.
     */
    unsigned        len;
    /**
     * @brief Various characteristics of the given opcode - see OPF_ flags.
     */
    unsigned        flags;
    /**
     * @brief Printable name of the opcode.
     */
    const char *    name;
    char            padding[32-20];
};

extern const InstrDesc instrs[OPCODE_COUNT];

/**
 * @brief Enumerates possible Java types
 *
 * The values are ordered by complexity ascending.
 */
enum jtype {
    /// signed 8 bits integer - Java's \c boolean or \c byte
    i8,
    /// signed 16-bits integer - Java's \c short
    i16,
    /// unsigned 16-bit integer - Java's \c char
    u16,
    /// signed 32 bit integer - Java's \c int
    i32,
    /// signed 64 bit integer - Java's \c long
    i64,
    /// single-precision 32 bit float - Java's \c float
    flt32,
    /// double-precision 64 bit float - Java's \c double
    dbl64,
    /// any object type
    jobj,
    /// void. no more, no less
    jvoid,
    /// jretAddr - a very special type for JSR things
    jretAddr,
    /// max number of types
    num_jtypes,
#ifdef _EM64T_
    iplatf=i64,
#else
    /// platform-native size for integer (fits into general-purpose register)
    iplatf=i32, 
#endif
};

/**
 * @brief Info about #jtype.
 */
struct JTypeDesc {
    /**
     * @brief #jtype this info refers to.
     */
    jtype        jt;
    /**
     * @brief Size of an item of given #jtype, in bytes.
     */
    unsigned     size;
    /**
     * @brief Offset of this #jtype items in arrays, in bytes.
     * @see vector_first_element_offset_unboxed
     */
    unsigned     rt_offset;
    /**
     * @brief Print name.
     */
    const char * name;
};

/**
 * @brief Info about all #jtype types.
 */
extern JTypeDesc jtypes[num_jtypes];

/**
 * @brief Tests whether specified #jtype represents floating point value.
 */
inline bool is_f( jtype jt )
{
    return jt==dbl64 || jt==flt32;
}

/**
 * @brief Tests whether specified #jtype occupies 2 slots (#i64 and #dbl64).
 */
inline bool is_wide(jtype jt)
{
    return jt==dbl64 || jt==i64;
}

/**
 * @brief Tests whether specified #jtype is too big to fit into a single 
 *        register on the current platform.
 * 
 * The only case currently is i64 on IA32.
 */
inline bool is_big(jtype jt)
{
    return jt==i64;
}

/**
 * @brief Converts a VM_Data_Type into #jtype.
 *
 * Java's byte (VM_DATA_TYPE_INT8) and boolean (VM_DATA_TYPE_BOOLEAN) are 
 * both returned as #i8.
 *
 * VM_DATA_TYPE_STRING, VM_DATA_TYPE_CLASS and VM_DATA_TYPE_ARRAY are all 
 * mapped onto #jobj.
 */
jtype to_jtype(VM_Data_Type vmtype);

/**
 * @brief Types used in lazy resolution scheme.
 *
 * Each RefType associated with an item to be resolved during runtime.
 * For example, RefType_StaticMeth associated with a values returned by 
 * resolve_static_method function. Composed with constant pool index, 
 * RefType values form an unique key which may be used to avoid resolving
 * the same item several times.
 */
enum RefType {
    /// Static method
    RefType_StaticMeth,
    /// Interface method
    RefType_InterfaceMeth,
    /// Special method
    RefType_SpecMeth,
    /// Virtual method
    RefType_VirtMethod,
    /// Class
    RefType_Class,
    /// Array calls
    RefType_ArrayClass,
    /// Class item, resolved via resolve_class_new
    RefType_ClassNew,
    /// Special case, used to make a key for object's size for new
    RefType_Size,
    /// Non-static field
    RefType_Field,
    /// Static field
    RefType_StaticField,
    /// Number of RefType-s in total
    RefType_Count
};

/**
 * @brief Composes a single int-sized key from the given RefType and
 *        constant pool index.
 * @note Although \c cp_idx is of unsigned type, it's treated as 'unsigned 
 *       short'.
 * @param type - one of RefType constants
 * @param cp_idx - constant pool index
 * @see toRefType()
 * @see ResState
 * @deprecated Used in lazy resolution scheme, not a production feature.
 */
inline unsigned ref_key(RefType type, unsigned cp_idx) {
    assert(type<RefType_Count);
    assert(cp_idx<USHRT_MAX);
    return (unsigned)((type<<16)|(cp_idx&0xFFFF));
}

/**
 * @brief Returns #RefType to be used for a given opcode.
 * @deprecated Used in lazy resolution scheme, not a production feature.
 */
RefType toRefType(JavaByteCodes opcode);

}
};    // ~namespace Jitrino::Jet

#endif  // __JDEFS_H_INCLUDED__
