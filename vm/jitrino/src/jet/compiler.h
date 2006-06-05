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
 * @version $Revision: 1.4.12.4.4.4 $
 */
/**
 * @file
 * @brief Declaration of Compiler class and related structures.
 */
 
#if !defined(__COMPILER_H_INCLUDED__)
#define __COMPILER_H_INCLUDED__


#include "jframe.h"
#include "rt.h"
#include "cg.h"

#if !defined(_IPF_)
    #include "cg_ia32.h"
#endif


#include "../shared/PlatformDependant.h"

#include <jit_export.h>

#include <assert.h>
#include <vector>
#include <map>
#include <string>
#include <stack>
#include <bitset>


namespace Jitrino {
namespace Jet {

/**
 * @brief Simple smart pointer.
 *
 * A trivial smart pointer implementation for dynamically allocated arrays.
 * Allocated memory is deallocated in destructor.
 *
 * Does not support neither copying, nor resizing of array. Used to avoid 
 * head ache about controlling raw pointers of allocated memory. 
 *
 * Used for arrays and includes bounds check in debug build - no bound 
 * checks performed in release one.
 */
template<class SomeType> class SmartPtr {
public:
    /**
     * @brief Initializes empty object.
     */
    SmartPtr(void)
    {
        m_data = NULL;
    }

    /**
     * @brief Allocates memory for \c count elements of SomeType.
     */
    void alloc(unsigned count)
    {
        m_data = new SomeType[count];
        m_count = count;
    }

    /**
     * @brief Returns number of elements in the allocates array.
     */
    unsigned count(void) const
    {
        return m_count;
    }

    /**
     * @brief Provides random access to an item by index.
     *
     * Bounds check is preformed only in debug build.
     */
    SomeType& operator[](unsigned idx)
    {
        assert(idx<m_count);
        return m_data[idx];
    }

    /**
     * @brief Provides a direct access to the allocated array.
     */
    SomeType* data(void)
    {
        return m_data;
    }

    /**
     * @brief Returns size of allocated memory, in bytes.
     */
    unsigned bytes(void)
    {
        return sizeof(SomeType)*m_count;
    }

    /**
     * @brief Fress allocated resources.
     */
    ~SmartPtr()
    {
        delete[] m_data;
    }
private:
    /**
     * @brief Pointer to the allocated memory.
     */
    SomeType *  m_data;
    /**
     * @brief Number of elements in the allocated array.
     */
    unsigned    m_count;
    /**
     * @brief Disallow copying.
     */
    SmartPtr(const SmartPtr&);
    /**
    * @brief Disallow copying.
    */
    SmartPtr& operator=(const SmartPtr&);
};

/**
 * @brief Reads out a 4-byte integer stored in the Java's format - 'most
 *        significant bytes first'.
 * @param pbc - a buffer to read from
 * @return integer
 */
inline int bc_int(const unsigned char * pbc)
{
    return (int)(((*(pbc+0))<<24) | ((*(pbc+1))<<16) | 
                ((*(pbc+2))<<8) | ((*(pbc+3))<<0));
}

/**
 * @brief Counts number of slots occupied by the specified set #jtype items.
 * @param args - array of #jtype
 * @return number of slots occupied by the specified set of #jtype items
 */
inline unsigned count_slots(const ::std::vector<jtype>& args)
{
    unsigned slots = 0;
    for (unsigned i=0; i<args.size(); i++) {
        slots  += (args[i] == dbl64 || args[i] == i64 ? 2 : 1);
    };
    return slots;
}

/**
 * @brief Counts number of slots occupied by the specified set #jtype items.
 * @param num - number of items in the \c args array
 * @param args - array of #jtype
 * @return number of slots occupied by the specified set of #jtype items
 */
inline unsigned count_slots(unsigned num, const jtype * args)
{
    unsigned slots = 0;
    for (unsigned i=0; i<num; i++) {
        slots += args[i] == dbl64 || args[i] == i64 ? 2 : 1;
    }
    return slots;
}

/**
 * @brief Java byte code instruction.
 */
struct JInst {
    /**
     * @brief Program counter of this instruction.
     */
    unsigned    pc;
    /**
     * @brief Program counter of next instruction.
     */
    unsigned    next;
    /**
     * @brief Instruction's opcode.
     */
    JavaByteCodes   opcode;
    /**
     * @brief Instruction's flags.
     */
    unsigned    flags;
    /**
     * @brief Value of the first instrution's operand, if applicable.
     * 
     * @note If instruction has no operands, the value of 'op0' is undefined.
     */
    int         op0;
    
    /**
     * @brief Value of the second instrution's operand, if applicable.
     *
     * @note If instruction has no second operands, the value of 'op1' is
     *       undefined.
     */
    int         op1;
    
    /**
     * @brief Address of data (the padding bytes skipped) for the switch
     *        instructions.
     *
     * @note For instructions other than switch, the value is undefined.
     */
    const unsigned char*    data;

    /**
     * @brief Returns number of targets for a branch or switch instruction.
     *
     * For branches it's always 1. 
     * For switches the default target is not included in the value.
     * For other instructions returns 0.
     */
    unsigned get_num_targets(void) const
    {
        if (is_branch())   { return 1; };
        if (opcode == OPCODE_TABLESWITCH) {
            return high() - low() + 1;
        }
        if (opcode != OPCODE_LOOKUPSWITCH) {
            return 0;
        }
        // 4 here is the number of bytecode bytes between the values - comes
        // from the JVM Spec
        return bc_int(data + 4);    // +4 - skip the 'defaultbyte*4'
    }

    /**
     * @brief Returns a PC value for the target 'i'.
     *
     * Must be only invoked for the branch or switch instructions and 'i' 
     * must be less or equal to gen_num_targets().
     * For other cases the behavior is unpredictable.
     */
    unsigned get_target(unsigned i) const
    {
        assert(i < get_num_targets());
        if (is_branch()) {
            return (unsigned short)(((unsigned short)pc) + 
                                    ((unsigned short)op0));
        }
        if (opcode == OPCODE_TABLESWITCH) {
            // '4+4+4' - skip defaultbyte, lowbyte and hightbyte
            const int * poffsets = (int*)(data + 4 + 4 + 4);
            return pc + bc_int((unsigned char*)(poffsets + i));
        }
        assert(opcode == OPCODE_LOOKUPSWITCH);
        // '4+4' - skip defaultbyte and npairs
        const int * ppairs = (int*)(data+4+4);
        return pc + bc_int((unsigned char*)(ppairs + i*2 + 1));
    }
    /**
     * @brief Returns default target for switch instructions.
     * 
     * Must only be invoked for TABLESWITCH or LOOKUPSWITCH instructions.
     */
    unsigned get_def_target(void) const
    {
        assert(opcode == OPCODE_TABLESWITCH || opcode == OPCODE_LOOKUPSWITCH);
        unsigned offset = bc_int(data);
        return (unsigned short)(((unsigned short)pc)+((unsigned short)offset));
    }
    
    /**
     * @brief Returns size of data (in bytes) occupied by LOOKUPSWITCH or 
     *        TABLESWITCH datas.
     *
     * Must only be invoked for TABLESWITCH or LOOKUPSWITCH instructions.
     */
    unsigned get_data_len(void) const
    {
        if (opcode == OPCODE_TABLESWITCH) {
            // 4*3 = defaultbyte,lowbyte,highbyte ; +jmp offsets
            return 4*3 + 4*(high()-low()+1);
        }
        assert(opcode == OPCODE_LOOKUPSWITCH);
        // defaultbyte + npairs + (4*2)*npairs
        return (4 + 4 + 4*2 * get_num_targets());
    }
    
    /**
     * @brief Returns minimum value of TABLESWITCH instruction.
     *
     * Must only be invoked for TABLESWITCH instructions.
     */
    int low(void) const
    {
        assert(opcode == OPCODE_TABLESWITCH);
        // 4 here is the number of bytecode bytes between the values - comes
        // from the JVM Spec
        return bc_int(data + 4);
    }
    
    /**
     * @brief Returns maximum value of TABLESWITCH instruction.
     *
     * Must only be invoked for TABLESWITCH instructions.
     */
    int high(void) const
    {
        assert(opcode == OPCODE_TABLESWITCH);
        // 4 here is the number of bytecode bytes between the values - comes
        // from the JVM Spec
        return bc_int(data + 4+4);
    }
    
    /**
     * @brief Returns Nth key of LOOKUPSWITCH instruction.
     *
     * Must only be invoked for LOOKUPSWITCH instructions.
     */
    int key(unsigned i) const
    {
        assert(opcode == OPCODE_LOOKUPSWITCH);
        // skip defaultbyte(+4), npairs(+4) and 'i' pairs
        return bc_int(data+4+4 + i*2*4);
    }
    /**
     * @brief Tests whether the instruction is branch - either conditional or
     *        not. JSR-s are also considered branches, but not SWITCH
     *        instructions.
     */
    bool is_branch(void) const
    {
        return (OPCODE_IFEQ <= opcode && opcode <= OPCODE_JSR) ||
               (OPCODE_IFNULL <= opcode && opcode <= OPCODE_JSR_W);
    }
    /**
     * @brief Tests whether the instruction SWITCH - either TABLESWITCH or
     *        LOOKUPSWITCH.
     */
    bool is_switch(void) const
    {
        return (opcode == OPCODE_TABLESWITCH || opcode == OPCODE_LOOKUPSWITCH);
    }
};

/**
 * @brief General info about basic block
 */
struct BBInfo {
    /**
     * @brief Very first bytecode instruction - the basic block's leader.
     */
    unsigned    start;
    
    /**
     *@brief Last bytecode instruction which belongs to this basic block.
     */
    unsigned    last_pc;
    
    /**
     * @brief Leader of the next basic block.
     * 
     * Actually, the next BB's leader can be obtained by a call to 
     * \link Compiler::fetch Compiler::fetch \endlink. This redundancy is 
     * intentional to avoid this additional call.
     */
    unsigned    next_bb;
    
    /**
     * @brief Offset of the BB's code.
     * 
     * Before layout - offset in codeStream, after layout - in the m_vmCode.
     * If the BB has a prolog, then ipoff points to the prolog code.
     */
    unsigned    ipoff;
    
    /**
     * @brief Total size of native code of the basic block.
     */
    unsigned    code_size;
    
    /**
     * @brief Number of ways this basic block can be reached by.
     */
    unsigned    ref_count;
    
    /**
     * @brief \b true if this basic block is a target for at least 
     *        one JMP-like instruction (goto/if_*).
     *
     * Was intented to perform branch target aligment on IA32/EM64T,
     * but currently unused.
     */
    bool        jmp_target;
    /**
     * @brief \b true if the basic block servers as a target for at least one
     *        JSR instruction.
     */
    bool        jsr_target;

    /**
     * @brief \b true if this basic block is a catch handler.
     *
     * Normally this means that is has an additional small prolog which puts
     * exception object onto the stack.
     */
    bool        ehandler;
    /**
     * @brief \b true if the basic block was processed in bbs_gen_code().
     * 
     * Used to avoid recursion and to detect unreachable code at the code 
     * layout stage.
     */
    bool        processed;
};

/**
 * @brief Defines a map of PC_of_bb_start -> BBInfo.
 */
typedef ::std::map<unsigned, BBInfo>    BBMAP;

/**
 * @brief Set of bitsets which shows whether a code for an item's resolution
 *        was generated (in lazy resolution scheme).
 *
 * Each opcode which requires resolution is mapped into one of #RefType 
 * constants. These constants are then used to select a bitset. In this 
 * bitset, an constant pool's index is used to select a bit. The bit state 
 * shows whether the code for resolution was generated or not. If the 
 * resolution code was already generated, then only code to reuse the result
 * is generated.
 *
 * @deprecated Experimental, not a production feature.
 */
struct ResState {
    /*
    Actually, the size must be 4096, not 32.
    The size of 4096 allows to cover all the real ranges of constant pool 
    indexes for client applications. However, as the compilation in 
    bbs_gen_code is recursive, and the ResState items are created on stack, 
    we'll get stack overflow with 4096.
    512 is the speculative size which does not cause stack overflow.
    However the lazy resolution is not a production feature at this moment, 
    so the size is reduced further to 32, to avoid copying overhead in 
    bbs_gen_code.
    */
    //::std::bitset<512>     states[RefType_Count];
    ::std::bitset<32>     states[RefType_Count];
    /**
     * @brief Tests whether a resolution for the given opcode was performed
     *        in the current basic block already.
     * @return \b true - if the code for the item's resolution was already 
     *         generated
     */
    bool test(JavaByteCodes opcod, unsigned idx) const
    {
        if (idx<states[0].size()) {
            return states[toRefType(opcod)].test(idx);
        }
        return false;
    }
    /**
     * @brief Sets a flags that a resolution for the given opcode was 
     *        performed.
     */
    void done(JavaByteCodes opcod, unsigned idx)
    {
        if (idx<states[0].size()) {
            states[toRefType(opcod)].set(idx);
        }
    }
};

/**
 * @brief State of a basic block during code generation.
 */
struct BBState {
    /**
     * @brief No op.
     */
    BBState() {};
    /**
     * @brief Current stack frame for the BB.
     */
    JFrame      jframe;
    /**
     * @brief Recently stored stack depth. 
     *
     * Used to eliminate unnecessary stack depth updates.
     */
    unsigned stack_depth;
    /**
     * @brief Recently stored GC mask for stack.
     *
     * Used to eliminate unnecessary stack GC map updates.
     *
     * Only single word of the GC mask is stored - it's enough for most 
     * applications.
     */
    unsigned stack_mask;
    /**
     * @brief \b true if #stack_mask contains a 'valid' (that is which was 
     *        really written).
     *
     * The #stack_depth field may contain limited set of values and thus 
     * the field itself may carry info whether it contains 'non-initialized'
     * flag (#NOTHING) or the real stack depth (any other value).
     *
     * In opposite, for the #stack_mask any value is valued combination. Thus
     * it's necessary to indicate whether the stack_mask was initialized or 
     * not. Here is this flag intended for.
     */
    bool stack_mask_valid;
    /**
     * @brief 'true' if there was at least one GC point in the basic block.
     *
     * Set during the code generation and is used to reduce unnecessary back 
     * branch pooling code.
     */
    bool        seen_gcpt;
    /**
     * @brief Shows which items are already have a code for lazy resolution 
     *        generated.
     */    
    ResState    resState;
private:
    /** @brief Disallow copying */
    BBState(const BBState&);
    /** @brief Disallow copying */
    BBState& operator=(const BBState&);
};
/**
 * @brief Information about exception handler.
 */
struct HandlerInfo {
    /**
     * @brief Start PC of bytecode region protected by this exception handler
     *        (inclusive).
     */
    unsigned     start;
    /**
     * @brief End PC of bytecode region protected by this exception handler
     *        (exclusive).
     */
    unsigned     end;
    /**
     * @brief Entry point PC of exception handler.
     */
    unsigned     handler;
    /**
     * @brief Type of exception handled by this handler. 0 for 'any type'.
     */
    unsigned     type;
    /**
     * @brief Class_Handle of the exception class for this handler.
     */
    Class_Handle klass;
    /**
     * @brief IP for #start.
     */
    char *       start_ip;
    /**
     * @brief IP for #end.
     */
    char *       end_ip;
    /**
     * @brief IP for #handler.
     */
    char *       handler_ip;
};

/**
 * @brief Rrepresents a JIT compiler for the Java bytecode under DRLVM
 *        environment.
 */
class Compiler : public VMRuntimeConsts {
public:

    //***********************************************************************
    //* Public interface
    //***********************************************************************
    
    /**
     * @brief Only stores the given parameters.
     */
    Compiler(JIT_Handle jh, const OpenMethodExecutionParams& params) : 
        compilation_params(params)
    {
        jit_handle = jh;
    }
    /**
     * @brief Main compilation routine.
     */
    JIT_Result compile(Compile_Handle ch, Method_Handle method);
    
    /**
     * @brief Default compilation flags propagated to all methods.
     * @see \link JITRINO_JET_JAVA_METHOD_FLAGS JMF_ flags \endlink.
     */                       
    static unsigned defaultFlags;
private:
    //***********************************************************************
    //* Initialization
    //***********************************************************************
    
    /**
     * @brief Initializes various runtime constants - helpers' addresses, 
     *        runtime constants, etc.
     * @see VMRuntimeConsts
     */
    static void initStatics(void);
    
    /**
     * @brief Initializes profiling data.
     *
     * Adds appropriate JMF_ flags to \c pflags if necessary.
     * @param pflags - flags which will be used for compilation. Must not be 
     *        NULL.
     */
    void initProfilingData(unsigned * pflags);

    //***********************************************************************
    //* Byte code processing
    //***********************************************************************

    /**
     * @brief Finds and marks all the basic blocks.
     */
    void bbs_mark_all(void);

    /**
     * @brief A helper function for #bbs_mark_all().
     * @param pc - a PC of basic block's leader bytecode instruction
     * @param jmp_target - \b true is the basic block is a target for an 
     *        goto/ifXX/jsr
     * @param add_ref - if \b true, then increments ref_count for the given 
     *        basic block (i.e. if this is fall-through basic block)
     * @param ehandler - \b true, if basic block represents catch handler
     * @param jsr_target - \b true if basic block is JSR subroutine
     */
    void bbs_mark_one(unsigned pc, bool jmp_target, bool add_ref, 
                      bool ehandler, bool jsr_target);

    /**
     * @brief Generates code for a given basic block.
     * 
     * When pc=0, also generates prolog code.
     *
     * If \c jsr_lead is not NOTHING (which means that this basic block
     * is part of JSR subroutine) then the final state of the stack on a RET
     * instruction will be stored for this jsr_lead and then reused if there
     * are several JSR instructions point to the same (jsr_lead) block.
     *
     * @param pc - program counter of the basic block
     * @param prev - state of the basic block's predecessor, NULL if no 
     *        predecessors (i.e. PC = 0, exception handlers)
     * @param jsr_lead - PC of the beginning of the JSR block, we're 
     *        currently in, or #NOTHING we're not in JSR block.
     */
    void bbs_gen_code(unsigned pc, BBState * prev, unsigned jsr_lead);
    
    /**
     * @brief Performs layout of the native code, so it become same as byte 
     *        code layout.
     */
    void bbs_layout_code(void);

    /**
    * @brief Resolves method's exception handlers.
    * 
    * @note Does call resolve_class() and and thus must not be used when the 
    *       lock protecting method's data is locked.
    * @return \b true if resolution was successful, \b false otherwise
    */
    bool bbs_ehandlers_resolve(void);
    
    /**
     * @brief Registers method's exception handlers.
     *
     * Does not call resolve_class() and may be used when a lock 
     * protecting method's data is locked.
     */
    void bbs_ehandlers_set(void);

    /**
     * @brief Fills out a native addresses of an exception handler and 
     *        a block it protects.
     */
    bool bbs_hi_to_native(char * codeBlock, HandlerInfo& hi);

    /**
     * @brief Fetches out and decodes a bytecode instruction starting 
     *        from the given 'pc'.
     * @return PC of the next instruction, or NOTHING if end of byte code 
     *         reached.
     */
    unsigned fetch(unsigned pc, JInst& jinst);
    /**
     * @brief Invokes appropriate handle_ik_ method to generate native code.
     */
    void handle_inst(const JInst& jinst);
    /**
     * @brief Helper method for #handle_inst(), handles method invocation
     *        instructions.
     */
    void handle_ik_meth(const JInst& jinst);
    /**
     * @brief Helper method for #handle_inst(), handles object manipulation
     *        instructions.
     */
    void handle_ik_obj(const JInst& jinst);
    /**
     * @brief Helper method for #handle_inst(), handles stack manipulation
     *        instructions.
     */
    void handle_ik_stack(const JInst& jinst);
    /**
     * @brief Helper method for #handle_inst(), handles throw instruction.
     */
    void handle_ik_throw(const JInst& jinst);
    /**
     * @brief Helper method for #handle_inst(), handles arithmetic
     *        instructions.
     */
    void handle_ik_a(const JInst& jinst);
    /**
     * @brief Helper method for #handle_inst(), handles control flow
     *        instructions.
     */
    void handle_ik_cf(const JInst& jinst);
    /**
     * @brief Helper method for #handle_inst(), handles conversion
     *        instructions.
     */
    void handle_ik_cnv(const JInst& jinst);
    /**
     * @brief Helper method for #handle_inst(), handles load and store 
     *        instructions.
     */
    void handle_ik_ls(const JInst& jinst);

    //***********************************************************************
    //* Code generation routines
    //***********************************************************************

    /**
     * @brief Generates method's prolog code.
     */
    void gen_prolog(const ::std::vector<jtype>& args);
    /**
     * @brief Generates method's epilogue (on RETURN instructions) code.
     */
    void gen_return(jtype retType);
    /**
     * @brief Generates code for pushing int constant on operand stack.
     */
    void gen_push(int);
    /**
     * @brief Generates code for pushing jlong constant on operand stack.
     */
    void gen_push(jlong);
    /**
     * @brief Generates code which reads a constant value of type 'jt' from 
     *        the address specified by 'p' and pushes it onto the stack.
     */
    void gen_push(jtype jt, const void * p);
    /**
     * @brief Generates code which pops out a value of type 'jt' from the 
     *        operand stack and stores it at the address specified by 'p'.
     */
    void gen_pop(jtype jt, void * p);
    /**
     * @brief Generates code to perform POP2 bytecode instruction.
     */
    void gen_pop2(void);
    /**
     * @brief Generates various DUP_  operations.
     */
    void gen_dup(JavaByteCodes opc);
    /**
     * @brief Generates xSTORE operations.
     */
    void gen_st(jtype jt, unsigned idx);
    /**
     * @brief Generates xLOAD operations.
     */
    void gen_ld(jtype jt, unsigned idx);
    /**
     * @brief Generates LDC operation.
     */
    void gen_ldc(void);
    /**
     * @brief Generates code for INVOKE instructions.
     */
    void gen_invoke(JavaByteCodes opcod, Method_Handle meth, 
                    const ::std::vector<jtype>& args, jtype retType);
    /**
     * @brief Generates code to put a value of specified type onto operand
     *        stack (presuming there was a call which returned the value).
     */
    void gen_save_ret(jtype jtyp);
    /**
     * @brief Generates code to call one of the throw_ helpers.
     *
     * @note The method only synchronize local vars into the memory,
     *       and does not update the status !
     */
    void gen_call_throw(void * target, unsigned num_args, ...);
    
    /**
     * @brief Generates code non-VM helper.
     *
     * The method does not update GC info.
     */
    void gen_call_novm(void * target, unsigned num_args, ...);

    /**
     * @brief Generates code to call a VM helper.
     *
     * The method updates GC info and synchronizes both stack and local vars.
     */
    void gen_call_vm(void * target, unsigned num_args, ...);
    
    /**
     * @brief Generates register/memory operations - spill out and upload.
     *
     * There are some [obvious] constraints for parameters:
     *  - either #MEM_VARS or #MEM_STACK must be specified, or both
     *  - either #MEM_TO_MEM or #MEM_FROM_MEM must be specified, but not both
     *  - either #MEM_UPDATE or #MEM_NO_UPDATE must be specified, but not both
     *  - #MEM_INVERSE can only be used with #MEM_FROM_MEM and #MEM_NO_UPDATE
     * @see JITRINO_JET_MEM_TRANSF_FLAGS.
     */
    void gen_mem(unsigned flags);
    
    /**
     * @brief Generates code to take arguments from Java operand stack and 
     *        put them to the native stack to prepare a call.
     *
     * @param pop - if \b true, the arguments are popped out from the operand 
     *        stack. Otherwise, the operand stack is left intact.
     * @param num - how many operands are in \c args
     * @param args - array of #jtype describing argument types
     */
    void gen_stack_to_args(bool pop, unsigned num, const jtype * args);
    /**
     * @brief Generates various IF_ operations.
     *
     * Also inserts back branch pooling (if the \link #JMF_BBPOOLING 
     * appropriate flag\endlink set), and \link #JMF_PROF_ENTRY_BE \endlink
     * instrumentation code.
     */
    void gen_if(JavaByteCodes opcod, unsigned target);
    /**
     * @brief Generates various IF_ICMP operations.
     *
     * Also inserts back branch pooling (if the \link #JMF_BBPOOLING 
     * appropriate flag\endlink set), and \link #JMF_PROF_ENTRY_BE \endlink
     * instrumentation code.
     */
    void gen_if_icmp(JavaByteCodes opcod, unsigned target);
    /**
     * @brief Generates GOTO operation.
     *
     * Also inserts back branch pooling (if the \link #JMF_BBPOOLING 
     * appropriate flag\endlink set), and \link #JMF_PROF_ENTRY_BE \endlink
     * instrumentation code.
     */
    void gen_goto(unsigned target);
    /**
     * @brief Generates JSR/JSR_W operation.
     */
    void gen_jsr(unsigned target);
    /**
     * @brief Generates RET operation.
     * @param idx - index of local variable.
     */
    void gen_ret(unsigned idx);
    /**
     * @brief Generates IINC operation.
     * @param idx - index of local variable.
     * @param value - value to add.
     */
    void gen_iinc(unsigned idx, int value);
    /**
     * @brief Generates arithmetic operations.
     *
     * @note \c op argument always specified as integer operation (for 
     *       example IADD, INEG), even if \c jt specifies float-point or long
     *       type.
     *
     * @param op - operation to perform.
     * @param jt - types used in the arithmetic operation.
     */
    void gen_a(JavaByteCodes op, jtype jt);
    /**
     * @brief Helper function for #gen_a, generates #i32 arithmetics.
     */
    void gen_a_i32(JavaByteCodes op);
    /**
     * @brief Generates conversion code.
     */
    void gen_cnv(jtype from, jtype to);
    /**
     * @brief Generates various CMP operations.
     */
    void gen_x_cmp(JavaByteCodes op, jtype jt);
    /**
     * @brief Generates ARRAYLENGTH instruction.
     */
    void gen_array_length(void);
    /**
     * @brief Generates ALOAD instruction.
     *
     * @note Does not check bounds. It must be done separately.
     */
    void gen_aload(jtype jt);
    /**
     * @brief Generates ASTORE instruction.
     *
     * @note Does not check bounds. It must be done separately.
     */
    void gen_astore(jtype jt);
    /**
     * @brief Generates PUTFIELD and GETFIELD operations.
     */
    void gen_field_op(JavaByteCodes op, jtype jt, Field_Handle fld);
    /**
     * @brief Generates PUTSTATIC and GETSTATIC operations.
     */
    void gen_static_op(JavaByteCodes op, jtype jt, Field_Handle fld);
    /**
     * @brief Generates code for INSTANCEOF or CHECKCAST operations.
     * @param chk - if \b true, generates CHECKCAST, INSTANCEOF otherwise.
     * @param klass - Class_Handle of the class to cast to.
     */
    void gen_instanceof_cast(bool chk, Class_Handle klass);
    
    /**
     * @brief Generates code to check bounds for array access.
     * @param aref_depth - depth (in the operand stack) of the array's object
     *        reference
     * @param index_depth - depth (in the operand stack) of the index to be
     *        used for array's access.
     */
    void gen_check_bounds(unsigned aref_depth, unsigned index_depth);
    /**
     * @brief Generates code to check whether the object ref at the given 
     *        depth in the operand stack is \c null.
     * @param stack_depth_of_ref - depth in the operand stack of the object 
     *        reference to test against \c null.
     */
    void gen_check_null(unsigned stack_depth_of_ref);
    /**
     * @brief Generates code to check whether an item used in division 
     *        operation is zero.
     * @param jt - type of division operation (#i64 or #i32) to be performed.
     * @param stack_depth_of_divizor - depth in the operand stack of the 
     *        divisor.
     */
    void gen_check_div_by_zero(jtype jt, unsigned stack_depth_of_divizor);
    /**
     * @brief Patches a native instruction identified by \c cpi parameter.
     * @see CodePatchItem
     */
    void gen_patch(const char * codeBlock, const CodePatchItem& cpi);
    /**
     * @brief Generates code for NEW instruction.
     */
    void gen_new(Class_Handle klass);

    /**
     * @brief Generates either LOOKUPSWITCH or TABLESWITCH.
     */
    void gen_switch(const JInst& jinst);

    /**
     * @brief Generates a code which prepares GC info for operand stack - 
     *        stack depth and GC mask.
     *
     * @param depth - if \b -1, then current stack depth is taken (#m_jframe),
     *        otherwise, the specified depth is stored.
     * @param trackIt - if \b true, then the GC info is also reflected in the
     *        current BB's state (BBState::stack_depth, 
     *        BBState::stack_mask).
     * @see BBState
     */
    void gen_gc_stack(int depth=-1, bool trackIt=false);

    /**
     * @brief Generates code which either marks or clears mark on a local 
     *        variable to reflect whether it holds an object or not (runtime
     *        GC info).
     */
    void gen_gc_mark_local(unsigned idx, bool mark);

    /**
     * @brief Generates GC safe point code which facilitate thread suspension 
     *        on back branches.
     */
    void gen_gc_safe_point(void);
    /**
     * @brief Generates ANEWARRAY, NEWARRAY.
     */
    void gen_new_array(Allocation_Handle ah);
    /**
     * @brief Generates MULTIANEWARRAY.
     */
    void gen_multianewarray(Class_Handle klass, unsigned num_dims);
    
    /**
     * @brief generates code for to resolve an item for the specified
     *        opcode at runtime (lazy resolution scheme).
     * 
     * @deprecated Not a production feature.
     */
    void gen_lazy_resolve(unsigned idx, JavaByteCodes opkod);
    
    //***********************************************************************
    //* Code patching and code buffer related routines
    //***********************************************************************
    
    /**
     * @brief Performs code patching.
     * 
     * The code patching is to finalize addresses for relative-addressing 
     * instructions like CALL, JMP, and for instructions which refer to a 
     * data allocated after the instructions generated (indirect JMPs for 
     * LOOKUPSWITCH, data for lazy resolution stuff).
     */
    void cg_patch_code(void);

    /**
     * @brief Returns a current 'ip' where to generate code into.
     *
     * The ip returned is a pointer to an internal \b temporary code buffer.
     *
     * @see CodeStream
     */
    char *  ip(void)
    {
        return m_codeStream.ip();
    }

    /**
     * @brief Sets a current ip for the internal code buffer.
     *
     * If there was a code patch registered before, then finalizes the patch.
     *
     * @see CodeStream
     */
    void    ip(char * ip);
    /**
     * @brief Registers next instruction to be generated as a subject for 
     *        code patching.
     *
     * This means that after \link #bbs_layout_code code layout \endlink, the
     * #gen_patch() method will be invoked with an info to find the generated
     * instruction and to update (patch) it. The data to patch depends on the
     * reg_patch() invoked.
     *
     * After the reg_patch() call, the patching machinery awaits for next 
     * call of #ip(char*) - it allows to determine generated instruction's
     * length.
     */
    unsigned    reg_patch(void);
    /**
     * @brief Registers next instruction (normally JMP/Jcc) to be patched 
     *        with an address of instruction corresponding to \c target_pc.
     *
     * Must only be used for JMP/Jcc/CALL instructions as the address to 
     * write is calculated as relative to the instruction's address.
     */
    unsigned    reg_patch(unsigned target_pc);
    /**
     * @brief Same as #reg_patch(unsigned), but may specify whether the 
     *        address to write must be relative to instruction's address.
     */
    unsigned    reg_patch(unsigned target_pc, bool relative);
    /**
     * @brief Registers next instruction (normally CALL/JMP) to be patched 
     *        with a \c target_ip address.
     */
    unsigned    reg_patch(const void * target_ip);
    /**
     * @brief Sets patch data, so it points to the specified address.
     */
    void    patch_set_target(unsigned patchID, char * _ip);
    /**
     * @brief Sets patch data, so it points to the specified offset in the 
     *        #m_codeStream.
     */
    void    patch_set_target(unsigned patchID, unsigned _ipoff);
    /**
     * @brief Registers TABLESWITCH instruction which will be patched with 
     *        a table of references.
     * 
     * The table allocated during code patching.
     */
    void    reg_table_switch_patch(void);
    /**
     * @brief Special case of patching - data address for lazy resolution 
     *        item.
     * @deprecated Not a production feature.
     */
    void    reg_data_patch(RefType type, unsigned cp_idx);
    
    //***********************************************************************
    //* Various method data
    //***********************************************************************
    
    /**
     * @brief The byte code of the method being compiled.
     */
    unsigned char *         m_bc;
    /**
     * @brief List of types of input arguments (includes 'this' for instance
     *        methods).
     */
    ::std::vector<jtype>    m_args;

    /**
     * @brief Return type of the method.
     */
    jtype       m_retType;
    
    /**
     * @brief Method's flags as they are known for VM (ACC_STATIC, etc...).
     */
    unsigned    m_java_meth_flags;
    
    /**
     * @brief A list to keep exception handlers' info.
     */
    typedef ::std::vector<HandlerInfo> HADLERS_LIST;
    
    /**
     * @brief List of infos about exception handlers.
     * 
     * The list preserves the order in which the VM returns the info about 
     * the handlers.
     */
    HADLERS_LIST        m_handlers;

    /**
     * @brief Method's info block.
     */
    MethodInfoBlock     m_infoBlock;
    
    /**
     * @brief Map of basic blocks. A key is basic block's leader's PC.
     */
    BBMAP               m_bbs;
    /**
     * @brief Array of decoded instructions.
     */
    SmartPtr<JInst>     m_insts;
    /**
     * @brief Current basic block's info.
     * 
     * @note Only valid during code generation.
     */
    const BBInfo *      m_curr_bb;
    /**
     * @brief Current basic block's state.
     * 
     * @note Only valid during code generation.
     */
    BBState *           m_curr_bb_state;
    /**
     * @brief \b true, if the next basic block has ref_count > 1.
     * 
     * @note Only valid at the very last instruction of \link #m_curr_bb 
     *       basic block \endlink.
     */
    bool                m_next_bb_is_multiref;
    
    /**
     * @brief PC of an instruction currently processed.
     */
    unsigned            m_pc;
    /**
     * @brief Instruction currently being processed.
     */
    const JInst *       m_curr_inst;
    
    /**
     * @brief Maps PC->JFrame state.
     */
    typedef ::std::map<unsigned, JFrame> STACK_STATE_MAP;
    
    /**
     * @brief Maps PC(JSR subroutine lead) -> JFrame state.
     * 
     * Used for JSR blocks:
     *
     * If more than one JSR points to a single basic block, then the state 
     * of the stack only calculated once, during the first visit to the BB.
     * Then this state is store in the map, and when we process second 
     * instruction pointing to the same BB, we do not re-iterate over BB's 
     * instructions, but use already caclulated state.
     */
    STACK_STATE_MAP     m_stack_states;
    /**
     * @brief Java method's frame - operand stack and local variables.
     *
     * @note Only valid during code-generation phase.
     */
    JFrame *        m_jframe;

    //***********************************************************************
    //* Instrumentation, profiling 
    //***********************************************************************
    
    /**
     * @brief Pointer to method's entrances counter.
     * @see JMF_PROF_ENTRY_BE
     */
    unsigned *  m_p_methentry_counter;
    /**
     * @brief Pointer to method's back branches counter.
     * @see JMF_PROF_ENTRY_BE
     */
    unsigned *  m_p_backedge_counter;
    /**
     * @brief Threshold for method entry counter which fires recompilation 
     *        (in synchronized recompilation mode).
     */
    unsigned    m_methentry_threshold;
    /**
     * @brief Threshold for back edges counter which fires recompilation 
     *        (in synchronized recompilation mode).
     */
    unsigned    m_backedge_threshold;
    /**
     * @brief Profile handle to be passed to recompilation handler (in 
     *        synchronized recompilation mode).
     */
    void*       m_profile_handle;
    /**
     * @brief Recompilation handler (in synchronized recompilation mode).
     */
    void *      m_recomp_handler_ptr;

    //***********************************************************************
    //* Code generation, patching data
    //***********************************************************************

    /**
     * @brief Code buffer allocated by VM.
     */
    char *          m_vmCode;
    /**
     * @brief Internal temporary buffer where the generated code is 
     *        accumulated.
     * 
     * Normally not to be used directly, but instead through #ip() methods 
     * calls.
     */
    CodeStream      m_codeStream;
    /**
     * @brief Id of a current patch item, or -1 if no active patch awaiting
     *        for #ip(char*) call.
     */
    int     m_patchID;
    /**
     * @brief List of registered patch items.
     */
    ::std::vector<CodePatchItem>    m_patchItems;
    /**
     * @brief A map which maps \link #ref_key reference key \endlink to 
     *        a memory slot.
     * @deprecated Not a production feature. 
     */
    typedef ::std::map<unsigned, void*> U2PTRMAP;
    /**
     * @brief A map which maps \link #ref_key reference key \endlink to 
     *        a memory slot.
     * @deprecated Not a production feature. 
     */
    U2PTRMAP        m_lazyRefs;

    //***********************************************************************
    //* Interface to VM - data and methods
    //***********************************************************************

    /**
     * @brief Compilation parameters.
     */    
    const OpenMethodExecutionParams& compilation_params;
    /**
     * @brief JIT handle.
     */    
    JIT_Handle                      jit_handle;
    /**
     * @brief Compilation handle.
     */    
    Compile_Handle                  m_compileHandle;
    /**
     * @brief Method handle.
     */    
    Method_Handle                   m_method;
    /**
     * @brief Class handle.
     */    
    Class_Handle                    m_klass;
    /**
     * @brief Parses method's signature at the given constant pool entry.
     *
     * @brief Parses a constant pool entry (presuming it contains a method's 
     *        signature) and fills the info about the method's arguments and
     *        return type.
     * @param is_static - must be \b true, if the signature belongs to static
     *        method (this is to add additional 'this' which is not reflected
     *        in signature for instance methods).
     * @param cp_idx - constant pool index.
     * @param args - an array to fill out. Must be empty.
     * @param retType - [out] return type of the method. Must not be NULL.
     */
    void    get_args_info(bool is_static, unsigned cp_idx, 
                          ::std::vector<jtype>& args, jtype * retType);
    /**
     * @brief Obtains an arguments and return info from the given method.
     * @param meth - method handle to get the info for.
     * @param args - an array to fill out. Must be empty.
     * @param retType - [out] return type of the method. Must not be NULL.
     */
    static void get_args_info(Method_Handle meth, ::std::vector<jtype>& args,
                              jtype * retType);
    /**
     * @brief Converts VM_Data_Type to appropriate #jtype.
     */
    static jtype to_jtype(VM_Data_Type vmtype) 
    {
        return ::Jitrino::Jet::to_jtype(vmtype);
    }
    /**
     * @brief Converts given Type_Info_Handle to appropriate #jtype.
     */
    static jtype to_jtype(Type_Info_Handle th);
    
    //***********************************************************************
    //* Debugging stuff
    //***********************************************************************

    /**
     * @brief Dumps out the basic blocks structure.
     * For debugging only.
     */
    void dbg_dump_bbs(void);
    /**
     * @brief Dumps out disassembled piece of code.
     * For debugging only.
     */
    void dbg_dump_code(const char * code, unsigned length, const char * name);
    /**
     * @brief Dumps out disassembled the whole code of the method, mixed with
     *        appropriate bytecode.
     * For debugging only.
     *
     * The code must be already generated and available for VM - 
     * \b method_get_code_block_addr_jit is used to obtain the code.
     */
    void dbg_dump_code(void);
    /**
     * @brief Converts the JInst into the human-readable string. 
     * For debugging only.
     *
     * @param jinst - instruction to be presented as string.
     * @param show_names - if \b true, then the string contains symbolic 
     *        names, otherwise only constant pool indexes.
     */
    ::std::string toStr(const JInst& jinst, bool show_names);
    /**
     * @brief Prints out a string 'compilation started'.
     * For debugging only.
     * @see DBG_TRACE_SUMM
     */
    void dbg_trace_comp_start(void);
    /**
     * @brief Prints out a string 'compilation finished', with a reason why 
     *        compilation failed, if \c success is \b false.
     * For debugging only.
     * @see DBG_TRACE_SUMM
     */
    void dbg_trace_comp_end(bool success, const char * reason);
    
    /**
     * @brief Generates code to ensure stack integrity at runtime.
     * @note For debug checks only.
     * @param start - \b true if check start to be generated, false otherwise
     */
    void gen_dbg_check_stack(bool start);
    /**
     * @brief Generates code to ensure stack integrity at the beginning 
     *        of a basic block at runtime.
     *
     * This is used to ensure stack integrity after branches which can not be
     * controlled by #gen_dbg_check_stack.
     *
     * @note For debug checks only.
     */
    void gen_dbg_check_bb_stack(void);
    
    /**
     * @brief Generates code to output string during runtime.
     *
     * The generated code preserves general-purpose registers.
     * The string is formatted before the code generation, then \link #rt_dbg
     * get printed \endlink during runtime.
     *
     * For debugging only.
     */
    void gen_dbg_rt_out(const char * fmt, ...);
    
    /**
     * @brief Inserts a break point.
     *
     * For debugging only.
     */
    void gen_dbg_brk(void);
    
#if defined(JIT_STATS) || defined(JIT_LOGS) || \
    defined(JET_PROTO) || defined(_DEBUG)
    /**
     * @brief Counter for compilation requests came through the Jitrino.JET.
     * 
     * Technically speaking, not equal to the number of compiled methods 
     * as some methods may be rejected, i.e. if resolution of exception 
     * handlers (performed at the very beginning) fails.
     *
     * @note Only used for debugging and tracing purposes, and so available 
     *       only when either of the following macro defined: JIT_STATS, 
     *       JIT_LOGS, JET_PROTO, _DEBUG.
     *
     * @note If no one of these macro defined, then it get declared and 
     *       defined as a static constant so read-access may be left in the
     *       code without runtime overhead.
     */
    static unsigned     g_methodsSeen;
    /**
     * @brief Contains full name which consists of 'klass name::method name',
     *        but without a signature.
     * 
     * @note See notes for #g_methodsSeen - the same applied here.
     */
    char                            m_fname[1024*10];
#else
    static const char               m_fname[1];
    static const unsigned           g_methodsSeen = 0;
#endif
    
#if defined(_DEBUG) || defined(JET_PROTO)
    /**
     * @brief If not #NOTHING, then breakpoint inserted before the code 
     *        generated for the byte code instruction and the specified PC.
     * For debugging only.
     * @note Absent in production release build.
     */
    unsigned dbg_break_pc;
#endif
    /** @brief Constant to be used to load predefined float-point constant*/
    static const int                g_iconst_m1;
    /** @brief Constant to be used to load predefined float-point constant*/
    static const int                g_iconst_0;
    /** @brief Constant to be used to load predefined float-point constant*/
    static const int                g_iconst_1;
    /** @brief Constant to be used to load predefined float-point constant*/
    static const float              g_fconst_0;
    /** @brief Constant to be used to load predefined float-point constant*/
    static const float              g_fconst_1;
    /** @brief Constant to be used to load predefined float-point constant*/
    static const float              g_fconst_2;
    /** @brief Constant to be used to load predefined float-point constant*/
    static const double             g_dconst_0;
    /** @brief Constant to be used to load predefined float-point constant*/
    static const double             g_dconst_1;

#if !defined(_IPF_)
    //***********************************************************************
    //* Code generation stuff specific to IA32/EM64T
    //***********************************************************************

    /**
     * @brief Layout of native stack frame.
     */
    StackFrame     m_stack;
    
public:   // used for debug stuff
    enum {
        /**
         * @brief How many XMM registers dedicated to hold float/double 
         *        values for the operand stack.
         */
        // Expected to be not less than 2, this presumption is used across 
        // the code
        F_STACK_REGS = 3,
        /**
         * @brief How many XMM registers dedicated to hold float/double 
         *        values for local vars.
         */
        // Expected to be not less than 2, this presumption is used across 
        // the code
        F_LOCAL_REGS = 3, // expected to be not less than 2
        /**
         * @brief How many general-purpose registers dedicated to hold 
         *        integer/long/object values for local vars.
         */
        // Expected to be not less than 2, this presumption is used across 
        // the code
        I_LOCAL_REGS = 2, // expected to be not less than 2
        /**
         * @brief How many general-purpose registers dedicated to hold 
         *        integer/long/object values for operand stack.
         */
        // Expected to be not less than 4, this presumption is used across 
        // the code
        I_STACK_REGS = 4
    };
    /**
     * @brief Set of registers for both operand stack and local variables for
     *        float-pointer items.
     */
    static const RegName    g_frnames[F_STACK_REGS + F_LOCAL_REGS];
    /**
     * @brief Set of registers for both operand stack and local variables for
     *        integer, long and object items.
     */
    static const RegName    g_irnames[I_STACK_REGS + I_LOCAL_REGS];
private:
    /**
     * @brief Returns set of registers for the given type \c jt.
     */
    static inline const RegName * get_regs(jtype jt)
    {
        return is_f(jt) ? g_frnames : g_irnames;
    }

    /**
     * @brief Ensures that EAX register is free.
     *
     * Swaps it out to the memory, if it's occupied.
     */
    void        veax(void);
    /**
     * @brief Ensures that EDX register is free.
     *
     * Swaps it out to the memory, if it's occupied.
     */
    void        vedx(void);
    /**
     * @brief Pushes \c jt onto \link #m_jframe operand stack\endlink, and
     *        checks whether a registers which keep top of the stack need 
     *        spill.
     */
    void        vpush(jtype jt, bool sync);
    /**
     * @brief Pops out \c jt from \link #m_jframe operand stack\endlink.
     */
    void        vpop(jtype jt);
    /**
     * @brief 'Synchronizes' operand stack and local variables.
     *
     * If some items in #m_jframe need either spill to or upload from memory,
     * then perform appropriate action.
     */
    void        vsync(void);
    /**
     * @brief Returns a register which keeps an item of operand stack at the 
     *        specified depth.
     *
     * If the item is spilled to the memory, then generates code to upload.
     *
     * @see JFrame::regable()
     */
    RegName     vstack(unsigned depth);
    
    /**
     * @brief Returns an 'alias' register.
     *
     * The 'alias' register is the same as specified register, but with 
     * size specified by \c jt.
     *
     * For example, 
     *  - valias(dbl64, RegName_XMM0) is RegName_XMM0D
     *  - valias(flt32, RegName_XMM0) is RegName_XMM0S
     *  - valias(i8,    RegName_EAX)  is RegName_AL
     *  - valias(i32,   RegName_AL)   is RegName_EAX
     *
     * @note A special processing is performed for ESI, EDI and EBP: as they
     *       do not have 8bit variant, then a scratch register (EAX or EDX) 
     *       allocated, and its 8bit version returned. As a side-effect, 
     *       valias() call may generate code to free the scratch register.
     */
    RegName     valias(jtype jt, RegName r);
    
    /**
     * @brief Generates specified instruction, with args specified.
     */
    void voper(Mnemonic mn, const EncoderBase::Operands& args)
    {
        ip(EncoderBase::encode(ip(), mn, args));
    }
    /**
     * @brief Generates specified instruction, which has no args.
     */
    void voper(Mnemonic mn)
    {
        EncoderBase::Operands args;
        ip(EncoderBase::encode(ip(), mn, args));
    }
    /**
     * @brief Generates specified instruction, which has a single operand.
     */
    void voper(Mnemonic mn, const EncoderBase::Operand& a0)
    {
        EncoderBase::Operands args(a0);
        ip(EncoderBase::encode(ip(), mn, args));
    }
    /**
     * @brief Generates specified instruction, which has two operands.
     */
    void voper(Mnemonic mn, const EncoderBase::Operand& a0,
                        const EncoderBase::Operand& a1)
    {
        EncoderBase::Operands args(a0, a1);
        ip(EncoderBase::encode(ip(), mn, args));
    }
    
    /**
     * @brief Generates CALL rel32 instruction, and registers patch for it.
     */
    void    vcall(const void * addr)
    {
        reg_patch(addr);
        voper(Mnemonic_CALL, EncoderBase::Operand((int)0));
    }
    /**
     * @brief Generates JMP rel32 instruction, which has its target the 
     *        specified PC and registers patch for it.
     */
    void    vjmp(unsigned pc)
    {
        reg_patch(pc);
        voper(Mnemonic_JMP, (int)0);
    }
    /**
     * @brief Generates short JMP rel8, and registers patch for it.
     * @return patch id
     */
    unsigned vjmp8(void)
    {
        unsigned pid = reg_patch();
        voper(Mnemonic_JMP, EncoderBase::Operand(OpndSize_8, 0));
        return pid;
    }
    /**
     * @brief Inserts specified prefix.
     *
     * If prefix is InstPrefix_Null, no action is taken.
     */
    void vprefix(InstPrefix prefix) {
        if (InstPrefix_Null != prefix) {
            ip(EncoderBase::prefix(ip(), prefix));
        }
    }
    /**
     * @brief Generates conditional Jcc rel32 instruction, and registers 
     *        patch for it.
     *
     * If prefix is InstPrefix_Null, no prefix inserted.
     *
     * @return patch id
     */
    unsigned    vjcc(ConditionMnemonic cond, 
                     InstPrefix prefix = InstPrefix_Null)
    {
        vprefix(prefix);
        unsigned pid = reg_patch();
        voper((Mnemonic)(Mnemonic_Jcc+cond), EncoderBase::Operand((int)0));
        return pid;
    }
    
    /**
     * @brief Generates conditional Jcc rel32 instruction, and registers 
     *        patch for it, so its target is specified PC.
     *
     * If prefix is InstPrefix_Null, no prefix inserted.
     *
     * @return patch id
     */
    void    vjcc(unsigned target_pc, ConditionMnemonic cond, 
                     InstPrefix prefix = InstPrefix_Null)
    {
        vprefix(prefix);
        reg_patch(target_pc);
        voper((Mnemonic)(Mnemonic_Jcc+cond), EncoderBase::Operand((int)0));
    }
    
    /**
     * @brief Generates specified instruction to be performed for 2 operand
     *        stack items.
     * 
     * If an item at depth1 is currently spilled to memory, does not reload
     * it to register, but generates 'mn reg(depth0), mem(depth1)' 
     * instruction. This is to save one load to register.
     */
    void    vdostack(Mnemonic mn, unsigned depth0, int depth1);
    
    /**
     * @brief Tests whether the given operand stack item is in memory.
     */
    bool vstack_swapped(unsigned depth)
    {
        return m_jframe->dip(depth).swapped();
    }
    /**
     * @brief Tests whether the given local variable is in memory.
     */
    bool vlocal_on_mem(unsigned idx);
    /**
     * @brief Forces (generates code for) an operand stack's item to be 
     *        spilled to memory.
     */
    void vstack_swap(unsigned depth);
    /**
     * @brief Returns Encoder's memory operand which points to an operand 
     *        stack item in the memory (with the real size of the item).
     *
     * @see \link StackFrame Stack layout\endlink
     */
    EncoderBase::Operand    vmstack(unsigned depth);
    /**
     * @brief Returns Encoder's memory operand which points to an operand 
     *        stack item in the memory as a pointer to 32bit slot.
     *
     * @see \link StackFrame Stack layout\endlink
     */
    EncoderBase::Operand    vstack_mem_slot(unsigned depth);
    /**
     * @brief Returns Encoder's operand which represents local variable.
     *
     * @see JFrame::alloc().
     */
    EncoderBase::Operand    vlocal(jtype jt, int idx, bool upper, 
                                   bool willDef);
    /**
     * @brief Returns Encoder's memory operand which points to a local 
     *        variable's memory slot, with the size determined by the \c jt.
     *
     * @see \link StackFrame Stack layout\endlink
     */
    EncoderBase::Operand    vmlocal(jtype jt, unsigned idx);
#endif  // if !defined(_IPF_)
};


}}; // ~namespace Jitrino::Jet

#include "bcproc.inl"

#endif  // __COMPILER_H_INCLUDED__
