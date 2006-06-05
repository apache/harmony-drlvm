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
#if !defined(__JFRAME_H_INCLUDED__)
#define __JFRAME_H_INCLUDED__

/**
 * @file
 * @brief Routines and data structures to mimic Java method's frame - 
 *        operand stack and local variables.
 */
 
#include "jdefs.h"
#include <vector>
#include <assert.h>

namespace Jitrino {
namespace Jet {

//
// SA_ states for 'Stack item Attributes'
//

/**
 * @brief 'Non Zero' - an items tested to be non-zero/non-null.
 */
#define SA_NZ           (0x00000001)
/**
 * @brief An item was tested against negative-size and is either positive
 *        or zero.
 */
#define SA_NOT_NEG      (0x00000002)

//
// SS_ states for 'Stack item State'
//

/** @brief Item was changed and need to be synch-ed back to memory.*/
#define SS_CHANGED      (0x00000004)
/** @brief Item need to be swapped from a register to the memory.*/
#define SS_NEED_SWAP    (0x00000008)
/** @brief Item need to be uploaded from a memory location to a register.*/
#define SS_NEED_UPLOAD  (0x00000010)
/**
 * @brief Item was swapped to memory and might need to be loaded back to a 
 *        register.
 */
#define SS_SWAPPED      (0x00000020)
/**
 * @brief Item represents a higher part of a wide time - either dbl64 or 
 *        i64.
 */
#define SS_HI_PART      (0x00000040)
/**
 * @brief An item's type was reflected in GC map.
 */
#define SS_MARKED       (0x00000080) // todo: seems useless - remove ?

/**
 * @brief Emulates Java method's frame (operand stack and local variables)
 *        and its operations.
 *
 * It's used to maintain a valid state for the operand stack and some 
 * info about locals needed for code generation (i.e. whether a variable 
 * need to be loaded from memory). And also carries some per-bb info like
 * a mask for GC and so on.
 *
 * JFrame also acts as a trivial local register allocator.
 *
 * The JFrame instances does not perform any callbacks to notify when spill 
 * or upload needed. Instead, it sets SS_NEED_SWAP or SS_NEED_UPLOAD on items
 * which require such actions. Whether such items exist may be checked 
 * through #need_update() call. After the spill/upload done, the SS_ state 
 * need to be cleared by #stack_state() or #var_state() call.
 *
 * Registers are separated on 2 groups by the type (float-point (FP) and
 * general-purpose (GP) registers). 
 * Registers in each type group referred by indexes (normally denoted as 
 * \c rid or \c regid), as if they were in a single continuos array.
 * 
 * In the each 'array' there are 2 groups by their function - first group
 * to keep top of the operand stack and another one to keep local variables.
 *
 * The JFrame tracks 'slots' for both operand stack and local variables. Some
 * items may occupy 2 slots, for example #i64 or #dbl64.
 * 
 * Each slot has its own unique id and set of attributes, which are changed 
 * for every defining operation (i.e. IINC operation), but are preserved 
 * during copying (i.e. for xLOAD, or xSTORE operations).
 *
 * When the state of the operand stack is tracked, each item on the stack
 * gets so called 'virtual slot' (normally referred as \b vslot). The virtual 
 * slot allows to track items of different types as if they were in the 
 * different, virtual stacks. The virtual slot shows how many items of the 
 * similar type are already on the stack. The 'similar' types are: float 
 * point items (#dbl64 and #flt32) tracked together. All other types are 
 * tracked also together but separately from float point slots.
 */
class JFrame {
public:
    /**
     * @brief Represents stack or local slot and some info associated with 
     *        it.
     */
    struct Slot {
        friend class JFrame;
        /**
         * @brief Initializes instance with empty values.
         */
        Slot()
        {
            jt = jvoid; 
            m_vslot = m_regid = -1;
            m_attrs = m_state = 0;
            m_id = 0;
        }
        /**
         * @brief Initializes instance with the given values.
         */
        Slot(jtype _jt, bool _half, int _vslot)
        {
            jt = _jt;
            m_vslot = _vslot;
            m_regid = -1;
            m_attrs = 0;
            m_id = 0;
            m_state = _half ? SS_HI_PART : 0;
        }
        /**
         * @brief Tests whether slot is swapped into memory.
         */
        bool swapped(void) const  { return m_state & SS_SWAPPED;   }
        /**
         * @brief Tests whether slot was changed since its last load from 
         *        memory.
         *
         * If the slot was changed it may need to be saved back to 
         * memory, i.e. for local variables.
         */
        bool changed(void) const  { return m_state & SS_CHANGED;   }
        /**
         * @brief Tests whether slot requires swap.
         *
         * For example, if the item was located on a register and this 
         * register was allocated for another slot.
         */
        bool needswap(void) const { return m_state & SS_NEED_SWAP; }
        /**
         * @brief Tests whether the slot represents a high part of a 
         *        wide item (i64 or dbl64).
         */
        bool hipart(void)  const  { return m_state & SS_HI_PART;   }
        
        /**
         * @brief Type of the slot.
         */
        jtype   jt;
        
        /**
         * @brief Returns type of the slot.
         */
        jtype       type(void) const    { return jt; }
        /**
         * @brief Returns id of the slot.
         */
        unsigned    id(void) const      { return m_id; }
        /**
         * @brief Returns register index for the slot.
         */
        int         regid(void) const   { return m_regid; }
        /**
         * @brief Returns virtual slot of the slot.
         */
        int         vslot(void) const   { return m_vslot; }
        /**
         * @brief Returns attributes of the slot.
         */
        unsigned    attrs(void) const   { return m_attrs; }
        /**
         * @brief Returns state of the slot.
         */
        unsigned    state(void) const   { return m_state; }
    private:
        /**
        * @brief Virtual slot (for stack items) occupied by a given stack
        *        item.
        */
        int         m_vslot;
        /**
         * @brief Attributes that may be assigned to a slot during 
         *        compilation.
         */
        unsigned    m_attrs;
        
        /**
         * @brief Compile-time state.
         */
        unsigned    m_state;
        
        /**
         * @brief Unique id of a slot's value. Changes every time when a 
         *        defining operation (write) to the slot happens.
         */
        unsigned    m_id;
        
        /**
         * @brief Register index assigned to a slot.
         */
        int         m_regid;
        
        /**
         * @brief Act like an assignment operator but transfers only
         *        transferable attributes of the slot.
         * 
         * Does not copy \c regid and \c vslot.
         * Also sets SS_CHANGED and clears SS_SWAPPED flags.
         */
        void assign(const Slot& s)
        {
            jt = s.jt;
            m_id = s.m_id;
            m_attrs = s.m_attrs;
            m_state = (s.m_state | SS_CHANGED) & ~SS_SWAPPED;
        }
    };
public:
    /**
     * @brief No op.
     */
    JFrame() {}

    /**
     * @brief Initializes the JFrame making it exactly the same as parent's
     *        state.
     */
    void    init(const JFrame * pparent);

    /**
     * @brief Initializes empty JFrame with the given max stack depth, 
     *        slots allocated for local variables and registers allocated
     *        for all this.
     */
    void    init(unsigned stack_max, unsigned var_slots, 
                 unsigned istackregs, unsigned ivarregs, 
                 unsigned fstackregs, unsigned fvarregs);
private:
    /**
     * @brief Returns a bit mask of occupied integer registers.
     */
    unsigned&   imask(void)
    {
        return m_imask;
    }
    /**
    * @brief Returns a bit mask of occupied float-point registers.
    */
    unsigned&   fmask(void)
    {
        return m_fmask;
    }
public:
    /**
     * @brief Returns number of slots on stack occupied by the float-point
     *        (if jt==flt32 or dbl64) or integer items.
     */
    unsigned    vslots(jtype jt) const 
    {
        assert(jt!=jvoid);
        return is_f(jt) ? fslots() : islots();
    }
private:
    /**
     * @brief Returns number of slots on stack occupied by float-point
     *        items.
     */
    unsigned    fslots(void) const
    {
        return m_fslots;
    }
    /**
     * @brief Returns number of slots on stack occupied by integer/object 
     *        items.
     */
    unsigned    islots(void) const
    {
        return m_islots;
    }
public:
    /**
     * @brief 'Devirtualizes' slot - gets a real slot index by its virtual
     *         slot and a type.
     */
    unsigned    devirt(jtype jt, unsigned vslot);
    /**
     * @brief Converts slot index into depth.
     */
    unsigned    slot2depth(unsigned slot) const
    {
        assert(slot<size());
        return size()-slot-1;
    }
    /**
    * @brief Converts depth into slot index.
    */
    unsigned    depth2slot(unsigned depth) const
    {
        assert(depth<size());
        return size()-depth-1;
    }
    
    /**
     * @brief Returns current stack depth.
     */
    unsigned    size(void) const
    {
        return m_top+1;
    }
    /**
     * @brief Returns type of a slot at the given depth.
     */
    jtype   top(unsigned depth = 0) const
    {
        assert(depth<size());
        return m_stack[size()-depth-1].jt;
    }
    /**
    * @brief Returns slot at the given depth.
    */
    Slot&   dip(unsigned depth)
    {
        assert(depth<size());
        return m_stack[size()-depth-1];
    }
    /**
     * @brief Pushes the given jtype into the stack.
     *
     * Allocates registers for this newly pushed item.
     * As result of this call, some items may need to be spilled to memory,
     * see #need_update().
     */
    void    push(jtype jt);
    /**
     * @brief Pops out an item from the stack.
     *
     * The given \c jt must be same as the item on top of the stack.
     *
     * As result, a state for some slots may change, see #need_update()
     * to check whether there are such items.
     */
    void    pop(jtype jt);
    /**
     * @brief Pops 2 slots out of the stack - either a single wide item 
     *        (#dbl64, #i64) or 2 one-slot items (#i32, #jobj, etc).
     */
    void    pop2(void);
    /**
     * @brief Pops out items specified by the array.
     *
     * Item of args[args.size()-1] is taken away from the top, and so on.
     */
    void    pop_n(const ::std::vector<jtype>& args);
    /**
     * @brief Performs various DUP operations over the stack.
     *
     * Only OPCODE_DUP* and OPCODE_SWAP operations are allowed.
     */
    void    dup(JavaByteCodes op);
    /**
     * @brief Makes the stack empty.
     */
    void    clear_stack(void);
    /**
     * @brief Assigns attributes to a stack slot on the given depth.
     */
    void    stack_attrs(unsigned depth, unsigned attrs);
    /**
     * @brief Sets and clears specified state for a stack slot on the given
     *        depth.
     *
     * @param depth - depth of the slot
     * @param set_state - a mask of states to set
     * @param clr_state - a mask of states to clear
     */
    void    stack_state(unsigned depth, unsigned set_state, 
                        unsigned clr_state);
    /**
     * @brief Loads local variable \c idx of type \c jt onto operand stack.
     */
    void    ld(jtype jt, unsigned idx);
    /**
     * @brief Stores top of operand stack of type \c jt into local variable 
     *        \c idx.
     */
    void    st(jtype jt, unsigned idx);
    /**
     * @brief If local variable \c idx occupies a register, then mark this 
     *        register as 'free'.
     */
    void    erase(unsigned idx);
    /**
     * @brief Clears all slots of local variables, marks all registers of 
     *        local variables 'free'.
     */
    void    clear_vars(void);
    /**
     * @brief Sets variable to type \c jt, increases its id and assigns 
     *        specified attributes.
     */
    void    var_def(jtype jt, unsigned idx, unsigned attrs);
    /**
     * @brief Sets and/or clears specified state flags for local variable.
     * @param idx - slot of local variable to change.
     * @param set_state - state to set.
     * @param clr_state - state to clear.
     */
    void    var_state(unsigned idx, unsigned set_state, 
                      unsigned clr_state);
    /**
     * @brief Allocates a register of specified type for specified local 
     *        variable.
     *
     * If \c idx is -1, then allocates a scratch register not assigned to 
     * any variable. If there is an available register of the given type,
     * it's returned as scratch. Otherwise one of the local variables slots
     * marked as 'SS_NEED_SWAP', its register marked as 'free' and returned.
     * 
     * @param jt - type of local variable/register to allocate.
     * @param idx - index of local variable. -1 to allocate a scratch 
     *        register of specified type.
     * @param upper - true if this slot represents a high part of wide 
     *        type (i.e. of #i64).
     * @param willDef shows whether the next operation with the register
     *        will be defining operation. If it's not and the variable 
     *        was not located on register, then the slot marked as 
     *        'SS_NEED_UPLOAD' - so the value need to be loaded from memory
     *        to register.
     */
    unsigned    alloc(jtype jt, int idx, bool upper, bool willDef);
    /**
     * @brief Releases specified register of local variable of the specified 
     *        type.
     */
    void        release(jtype jt, unsigned rid);
    
    /**
     * @brief Returns number of local variables.
     */
    unsigned    num_vars(void) const
    {
        return m_vars.size();
    }
    
    //
    // register-allocation machinery utilities
    //
    
    /**
     * @brief Returns mask of free/occupied registers for local vars of 
     *        the given type.
     */
    unsigned&   _rmask(jtype jt)
    {
        assert(jt!=jvoid);
        return is_f(jt) ? fmask() : imask();
    }
    /**
     * @brief Returns mask of all available registers for local vars of
     *        the given type.
     */
    unsigned    _rallmask(jtype jt)
    {
        assert(jt!=jvoid);
        return is_f(jt) ? m_fvar_regs_mask : m_ivar_regs_mask;
    }
    /**
     * @brief Returns a mapping between register index and a variable index,
     *        of the given type.
     */
    int *       _rmap(jtype jt)
    {
        assert(jt!=jvoid);
        return is_f(jt) ? m_fvarmap : m_ivarmap;
    }
    /**
     * @brief Returns number of registers for operand stack of the given
     *        type.
     */
    unsigned    _rstackregs(jtype jt) const
    {
        return is_f(jt) ? m_fstack_regs : m_istack_regs;
    }
    /**
     * @brief Returns number of registers for local vars of the given
     *        type.
     */
    unsigned    _rlocalregs(jtype jt) const
    {
        return is_f(jt) ? m_fvar_regs : m_ivar_regs;
    }
    /**
     * @brief Returns index of last used register of the given type.
     */
    unsigned&   rlastused(jtype jt)
    {
        assert(jt!=jvoid);
        return is_f(jt) ? m_flast_used : m_ilast_used;
    }
    /**
     * @brief Returns number of items that require update (#SS_NEED_SWAP or
     *        #SS_NEED_UPLOAD).
     */
    unsigned    need_update(void) const
    {
        return m_need_update;
    }
    /**
     * @brief Clears attributes for both vars and stack slots.
     */
    void    clear_attrs(void);
    /**
     * @brief Tests whether an item of operand stack at the specified depth
     *        can be allocated on a register.
     */
    bool    regable(unsigned depth) const;
    //
    // GC-related routines
    //

    /**
     * @brief Returns number of words (currently == long) which need to hold
     *        a complete GC mask.
     */
    unsigned    gc_width(void) const    { return words(m_stack.size()); };
    /**
     * @brief Returns a word representing a bit mask which in reflects
     *        the state the stack - '1' means 'an object is on the operand
     *        stack'.
     */
    unsigned    gc_mask(unsigned word_no) const;
    /**
     * @brief Returns stack slot at the given position (not depth).
     */
    Slot& at(unsigned slot)
    {
        assert(slot<size());
        return m_stack[slot];
    }
    
    /**
     * @brief Returns local variable's slot at the given position.
     */
    Slot& var(unsigned slot)
    {
        assert(slot<m_vars.size());
        return m_vars[slot];
    }
    /**
     * @brief Returns read-only item representing local slot.
     * @param slot - index of the local slot to return
     * @return an item representing local slot, read-only
     */
    const Slot& get_var(unsigned slot) const
    {
        assert(slot<m_vars.size());
        return m_vars[slot];
    }
    /**
     * @brief Returns read-only item of the stack slot, at the given position.
     *
     * @note \c pos is \b position, not depth.
     *
     * @param pos - index of the stack slot to return
     * @return an item representing stack slot, read-only
     */
    const Slot& get_stack(unsigned pos) const
    {
        assert(pos<size());
        return m_stack[pos];
    }
    
    
private:
#ifdef _DEBUG
    /**
     * @brief Check's data consistency. No-op in release mode.
     */
    void    verify(void );
#else
    void    verify(void) {};
#endif
    /**
     * @brief Returns stack slot at the given depth.
     */
    Slot&   __get(unsigned depth) 
    {
        assert(depth<size());
        return m_stack[depth2slot(depth)];
    }
    /**
     * @brief Current stack pointer. -1 if stack is empty.
     */
    int     m_top;

    /**
     * @brief Operand tack data.
     */
    ::std::vector<Slot>     m_stack;
    /**
     * @brief Local variables data.
     */
    ::std::vector<Slot>     m_vars;
    
    /**
     * @brief Number of stack vslots occupied by flt32 and dbl64 items.
     *
     * Both dbl64 and flt32 contribute one vslot.
     */
    unsigned    m_fslots;
    
    /**
     * @brief Bit-mask of which FP registers are occupied.
     */
    unsigned    m_fmask;
    /**
     * @brief Mapping between FP registers and local variables they are 
     *        allocated for.
     *
     * m_fvarmap[regid] contains index of local variable assigned to the 
     * register \c regid, or -1 if the register is free.
     */
    int         m_fvarmap[MAX_REGS];
    /**
     * @brief Index of last used float-point register.
     */
    unsigned    m_flast_used;
    /**
     * @brief Max number of FP registers available to store stack items.
     */
    unsigned    m_fstack_regs;
    /**
     * @brief Max number of FP registers available to store local 
     *        variables.
     */
    unsigned    m_fvar_regs;
    /** 
     * @brief Bit mask which represents all FP registers available to store 
     *        local variables.
     */
    unsigned    m_fvar_regs_mask;
    /**
     * @brief Number of stack vslots occupied by integer and object items.
     *
     * #i64 items contribute 2 vslots.
     */
    unsigned    m_islots;
    /**
     * @brief Bit-mask of which GP registers are occupied.
     */
    unsigned    m_imask;
    /**
     * @brief Mapping between GP registers and local variables they
     *        are allocated for.
     *
     * m_ivarmap[regid] contains index of local variable assigned to the 
     * register \c regid, or -1 if the register is free.
     */
    int         m_ivarmap[MAX_REGS];
    /**
     * @brief Index of last used GP register.
     */
    unsigned    m_ilast_used;
    /**
     * @brief Max number of GP registers available to store stack items.
     */
    unsigned    m_istack_regs;
    /**
     * @brief Max number of GP registers available to store local variables.
     */
    unsigned    m_ivar_regs;
    /** 
     * @brief Bit mask which represents all GP registers available to store 
     *        local variables.
     */
    unsigned    m_ivar_regs_mask;
    /**
     * @brief Counter used as unique id for slots for defining operations.
     */
    unsigned    m_slotids;
    /**
     * @brief How many items (both stack and local vars) need update (i.e.
     *        have either #SS_NEED_SWAP or #SS_NEED_UPLOAD states).
     */
    unsigned    m_need_update;
};

}}; // ~namespace Jitrino::Jet

#endif      // ~__JFRAME_H_INCLUDED__
