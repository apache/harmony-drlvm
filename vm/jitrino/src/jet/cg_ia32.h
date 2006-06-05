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
 * @version $Revision: 1.4.12.3.4.4 $
 */
 
/**
 * @file
 * @brief Contains code-gen utilities and structures specific to IA32/EM64T.
 */
 
#if !defined(__CG_IA32_H_INCLUDED__)
#define __CG_IA32_H_INCLUDED__

#include "enc_base.h"
#include "jframe.h"

namespace Jitrino {
namespace Jet {


/**
 * @brief Size of the stack slot, in bytes.
 */
#define SLOT_SIZE (sizeof(void*))

/**
 * @brief Base pointer, for method's stack frame.
 */
#define REG_BASE    (RegName_EBP)

#define PATCH_COUNT_2_BC_SIZE_RATIO             (0.7)
#define NATIVE_CODE_SIZE_2_BC_SIZE_RATIO        (10)
#define PATCH_COUNT_2_BC_SIZE_RATIO_LAZY        (1.1)
#define NATIVE_CODE_SIZE_2_BC_SIZE_RATIO_LAZY   (20)

/**
 * @brief Controls layout of the native stack frame.
 * 
 * The class StackFrame represents EBP-based stack frame, organized 
 * for the methods compiled by Jitrino.JET.
 *
 * @note Don't mess the slots ! Number of slots occupied by local variables,
 *       stack and input args refer to 'Java operand stack slots'. Other 
 *       values are measured in 'native stack slots' which are 'number of 
 *       bytes on the stack affected by single regular PUSH or POP 
 *       instruction'. This 'native stack slot' is 4 bytes on IA32 and 8 
 *       bytes on EM64T.
 *
 * Below is the stack layout, used for methods compiled by Jitrino.JET.
 *
<table color="black" >
<tr align="center">
    <td>Offset</td><td>Purpose</td><td>Comment</td>
</tr>
<tr bgcolor="silver">
    <td> +4+num_input_slots</td><td>in arg 0</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>&nbsp;</td><td>in 1</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>+4+num_input_slots - i</td><td>...</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>+4</td><td>return address</td><td>&nbsp;</td>
</tr>
<tr>
    <td>+3</td><td>saved EBX</td><td>&nbsp;</td>
</tr>
<tr>
    <td>+2</td><td>saved ESI</td><td>&nbsp;</td>
</tr>
<tr>
    <td>+1</td><td>saved EDI</td><td>&nbsp;</td>
</tr>
<tr>
    <td align="right"><b>EBP =></b></td>
    <td>saved EBP</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>-1</td><td>GC_Stack_Depth</td>
    <td>
    Operand stack's depth. Updated during runtime before "GC points".<br>
    Used during root-set enumeration to report objects currently on operand 
    stack.
    </td>
</tr>
<tr bgcolor="silver">
    <td>&nbsp;</td><td>... padding ....</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>&nbsp;</td>
    <td>GC_Locals - end</td><td>GC map for locals </td>
</tr>
<tr bgcolor="silver">
    <td>GC_Stack_Depth - 1 - (words(NUM_LOCALS)+1)&~1)</td>
    <td>GC_Locals - begin</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>&nbsp;</td><td>... padding ...</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>&nbsp;</td>
    <td>GC_Stack - end</td><td>GC map for stack</td>
</tr>
<tr bgcolor="silver">
    <td>GC_Locals - ((words(m_max_stack)+1)&~1)</td>
    <td>GC_Stack - begin</td><td>&nbsp;</td>
</tr>
<tr>
    <td>GC_Locals - 1</td><td>LAST_LOCAL = local N-1</td>
    <td>Area to spill out local vars when needed.</td>
</tr>
<tr>
    <td>&nbsp;</td><td>...</td><td>&nbsp;</td>
</tr>
<tr>
    <td>&nbsp;</td><td>LOCAL_0</td><td>&nbsp;</td>
</tr>
<tr>
    <td>LOCAL_0 - 1</td><td>JSTACK_BOTTOM</td>
    <td>Bottom of Java operand stack - area to spill out operand stack items
    when needed.</td>
</tr>
<tr>
    <td>&nbsp;</td><td>...</td><td>&nbsp;</td>
</tr>
<tr>
    <td>JSTACK_BOTTOM - (MAX_STACK-1)</td><td>JSTACK_TOP</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>JSTACK_TOP - 1</td><td>native stack bottom</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>&nbsp;</td><td>...</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>[ESP-1]</td><td>&nbsp;</td><td>&nbsp;</td>
</tr>
<tr bgcolor="silver">
    <td>[ESP]</td><td>native stack top</td><td>&nbsp;</td>
</tr>
</table>

 */
class StackFrame {
public:
    /**
     * @brief Initializes an instance of StackFrame.
     *
     * @param _num_locals - number of slots occupied by local variables in 
     *        Java method's frame
     * @param _max_stack - max depth of Java operand stack
     * @param _in_slots - number of slots occupied by input args in Java 
     *        on Java operand stack
     * @see #init
     */
    StackFrame(unsigned _num_locals, unsigned _max_stack, unsigned _in_slots)
    {
        m_num_locals = _num_locals;
        m_max_stack = _max_stack;
        m_in_slots = _in_slots;
    }

    /**
     * @brief Noop.
     */
    StackFrame() {}

    /**
     * @brief Initializes fields of this StackFrame instance.
     *
     * @param _num_locals - number of slots occupied by local variables in 
     *        Java method's frame
     * @param _max_stack - max depth of Java operand stack
     * @param _in_slots - number of slots occupied by input args in Java 
     *        on Java operand stack
     */
    void init(unsigned _num_locals, unsigned _max_stack, unsigned _in_slots)
    {
        m_num_locals = _num_locals;
        m_max_stack = _max_stack;
        m_in_slots = _in_slots;
    }

private:
    /**
     * @brief Returns number of slots occupied by local variables.
     *
     * @return number of slots occupied by local variables in Java method's 
     *         frame
     */
    unsigned get_num_locals(void) const
    {
        return m_num_locals;
    }
    
    /**
     * @brief Returns max stack depth of Java method's operand stack
     * @return max stack depth
     */
    unsigned get_max_stack(void) const
    {
        return m_max_stack;
    }
    
    /**
     * @brief Returns number of slots occupied by input args, in Java 
     *        method's operand stack
     * @return number of slots occupied by input args
     */
    unsigned get_in_slots(void) const
    {
        return m_in_slots;
    }
public:

    /**
     * @brief Returns overall size of the stack frame (not including input
     *        args and return value).
     * @return size of the stack frame
     */
    unsigned size(void) const
    {
        return -native_stack_bot();
    }
    
    /**
     * @brief Returns offset of the input argument.
     * @param i - index of input argument's slot
     * @return offset of the input argument.
     */
    int in_slot(unsigned i) const
    {
        return in_ret() + m_in_slots - i;
    }
    
    /**
     * @brief Returns offset of location of return address.
     * @return offset of location of return address
     */
    int in_ret(void) const
    {
        return 4;
    }
    
    /**
     * @brief Returns offset of EBX location.
     * @return offset of EBX location
     */
    int save_ebx(void) const
    {
        return 3;
    }
    
    /**
     * @brief Returns offset of ESI location.
     * @return offset of ESI location
     */
    int save_esi(void) const
    {
        return 2;
    }
    
    /**
     * @brief Returns offset of EDI location.
     * @return offset of EDI location
     */
    int save_edi(void) const
    {
        return 1;
    }
    
    /**
     * @brief Returns offset of EBP location.
     * @return offset of EBP location
     */
    int save_ebp(void) const
    {
        return 0;
    }
    /**
     * @brief Returns offset of operands's stack depth.
     */
    int info_gc_stack_depth(void) const
    {
        return -1;
    }
    /**
     * @brief Returns offset of GC map for local variables.
     */
    int info_gc_locals(void) const
    {
        return info_gc_stack_depth() - 1 - ((words(m_num_locals)+1)&~1);
    }
    /**
     * @brief Returns offset of GC map for operand stack.
     */
    int info_gc_stack(void) const
    {
        return info_gc_locals() - ((words(m_max_stack)+1)&~1);
    }
    /**
     * @brief Returns offset of local variable.
     */
    int local(unsigned i) const
    {
        return info_gc_stack() - m_num_locals + i;
    }
    /**
     * @brief Returns offset of bottom item of operand stack.
     */
    int stack_bot(void) const
    {
        return local(0) - 1;
    }
    /**
     * @brief Returns offset of the item of operand stack (\c slot counted 
     *        from the stack's bottom, and it's opposite to depth which 
     *        counted from stack top).
     */
    int stack_slot(unsigned slot) const
    {
        return stack_bot() - slot;
    }
    /**
     * @brief Returns offset of maximum available operand stack's item.
     */
    int stack_max(void) const
    {
        return stack_bot() - (m_max_stack - 1);
    }
    /**
     * @brief Returns offset of bottom item of native stack.
     */
    int native_stack_bot(void) const
    {
        return stack_max() - 1;
    }
private:
    /**
     * @brief Number of locals in method.
     */
    unsigned m_num_locals;
    /**
     * @brief Max depth of Java operand stack.
     */
    unsigned m_max_stack;
    /**
     * @brief Number of input arguments.
     */
    unsigned m_in_slots;
};

/**
 * @brief Describes some info related to a #jtype from IA32/EM64T code 
 *        generator's point of view.
 * @see typeInfo
 */
struct TypeInfo {
    /// an OpndKind of a register that can be used to store the value of 
    ///  appropriate type
    OpndKind    reg_kind;
    /// Size of the type as OpndSize enum
    OpndSize    size;
    /// Size of type in bytes
    unsigned    bytes;
    /// Mnemonic used to move the type between memory and register 
    /// of 'reg_kind', or between two registers of 'reg_kind'
    Mnemonic    mov;
};

/**
 * @brief Array of TypeInfo, arranged for appropriate #jtype.
 */
extern const TypeInfo typeInfo[num_jtypes];

}}; // ~namespace Jitrino::Jet

#endif      // if !defined __CG_IA32_H_INCLUDED__
