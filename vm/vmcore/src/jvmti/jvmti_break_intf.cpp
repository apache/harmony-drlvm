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
 * @author Ilya Berezhniuk, Gregory Shimansky
 * @version $Revision$
 */  
/**
 * @file jvmti_break_intf.cpp
 * @brief JVMTI native breakpoints API implementation
 */

#include "cxxlog.h"
#include "environment.h"
#include "interpreter.h"
#include "interpreter_exports.h"
#include "jit_intf_cpp.h"
#include "method_lookup.h"
#include "exceptions.h"
#include "m2n.h"
#include "stack_iterator.h"
#include "open/bytecodes.h"
#include "cci.h"

#include "jvmti_break_intf.h"
#include "cci.h"


#if (defined _IA32_) || (defined _EM64T_)

#include "encoder.h"
// Forward declarations
static ConditionCode
get_condition_code(InstructionDisassembler::CondJumpType jump_type);
#endif

static bool set_jit_mode_breakpoint(VMBreakPoint* bp);
static bool set_native_breakpoint(VMBreakPoint* bp);
static bool clear_native_breakpoint(VMBreakPoint* bp);


//////////////////////////////////////////////////////////////////////////////
// VMBreakPoints implementation
VMBreakPoints::~VMBreakPoints()
{
    lock();
    for(unsigned index = 0; index < PRIORITY_NUMBER; index++ ) {
        while (m_intf[index]) {
            VMBreakInterface* intf = m_intf[index];
            m_intf[index] = intf->m_next;
            delete intf;
        }
    }
    assert(m_break == NULL);
    unlock();
}

VMBreakInterface*
VMBreakPoints::new_intf(TIEnv *env,
                        BPInterfaceCallBack callback,
                        unsigned priority,
                        bool is_interp)
{
    assert(callback);
    assert(priority < PRIORITY_NUMBER);
    VMBreakInterface* intf = new VMBreakInterface(env, callback, priority, is_interp);
    assert(intf);

    lock();

    TRACE2("jvmti.break", "Create breakpoint interface: " << intf );

    // add interface to the end of list
    if( NULL == m_intf[priority] ) {
        m_intf[priority] = intf;
    } else {
        VMBreakInterface *last = m_intf[priority];
        for( ; last->m_next; last = last->m_next )
            ;
        last->m_next = intf;
    }

    // correct thread processing breakpoints
    for(VMLocalBreak *local = m_local; local; local = local->next) {
        if( local->priority == priority && NULL == local->intf ) {
            TRACE2("jvmti.break", "Set local thread interface: "
                << local << ", intf: " << intf );
            local->intf = intf;
        }
    }

    unlock();

    return intf;
}

void
VMBreakPoints::release_intf(VMBreakInterface* intf)
{
    assert(intf);
    assert(intf->get_priority() < PRIORITY_NUMBER);
    LMAutoUnlock lock(get_lock());

    TRACE2("jvmti.break", "Release breakpoint interface: " << intf );

    // correct thread processing breakpoints
    for(VMLocalBreak *local = m_local; local; local = local->next) {
        if( local->intf == intf ) {
            TRACE2("jvmti.break", "Set local thread interface: "
                << local << ", intf: " << intf->m_next );
            local->intf = intf->m_next;
        }
    }

    // release interface
    for (VMBreakInterface** cur_ptr = &m_intf[intf->get_priority()];
         *cur_ptr;
         cur_ptr = &((*cur_ptr)->m_next))
    {
        if (*cur_ptr == intf)
        {
            *cur_ptr = (*cur_ptr)->m_next;

            delete intf;
            return;
        }
    }
    LDIE2("jvmti.break", 23, "{0} try to release unknown interface" << "VMBreakPoints::release_intf:");
}

VMBreakInterface*
VMBreakPoints::get_next_intf(VMBreakInterface *intf)
{
    return intf->m_next;
}

inline bool
VMBreakPoints::check_insert_breakpoint(VMBreakPoint* bp)
{
#ifndef NDEBUG
    if( !bp ) {
        return false;
    } else if( bp->method ) {
        TRACE2("jvmti.break", "Try to insert breakpoint: "
            << class_get_name(method_get_class((Method*)bp->method)) << "."
            << method_get_name((Method*)bp->method)
            << method_get_descriptor((Method*)bp->method)
            << " :" << bp->location << " :" << bp->addr);

        VMBreakPoint* another = find_breakpoint(bp->method, bp->location);
        if( !another ) {
            return true;
        }

        TRACE2("jvmti.break", "Before inserting found another breakpoint: "
            << (another->method
                ? class_get_name(method_get_class((Method*)another->method)): "(nil)")
            << "."
            << (another->method ? method_get_name((Method*)another->method) : "(nil)")
            << (another->method ? method_get_descriptor((Method*)another->method) : "")
            << " :" << another->location << " :" << another->addr);

        if( bp->addr == another->addr) {
            return false;
        }
    } else if( bp->addr ) {
        TRACE2("jvmti.break", "Try to insert breakpoint: native address:"
            << bp->addr);

        VMBreakPoint* another = find_breakpoint(bp->addr);
        if( !another ) {
            return true;
        }

        TRACE2("jvmti.break", "Before inserting found another breakpoint: "
            << (another->method
                ? class_get_name(method_get_class((Method*)another->method)) :"(nil)")
            << "."
            << (another->method ? method_get_name((Method*)another->method) :"(nil)")
            << (another->method ? method_get_descriptor((Method*)another->method) :"")
            << " :" << another->location << " :" << another->addr);

        if(another->method) {
            return false;
        }
    } else {
        return false;
    }
#endif // !NDEBUG
    return true;
}

inline void
VMBreakPoints::insert_breakpoint(VMBreakPoint* bp)
{
    TRACE2("jvmti.break", "Insert breakpoint: "
        << (bp->method
            ? class_get_name(method_get_class((Method*)bp->method)) : "(nil)")
        << "."
        << (bp->method ? method_get_name((Method*)bp->method) : "(nil)")
        << (bp->method ? method_get_descriptor((Method*)bp->method) : "")
        << " :" << bp->location << " :" << bp->addr);

    // add breakpoint to the end of list
    if(m_last) {
        m_last->next = bp;
    } else {
        m_break = bp;
    }
    m_last = bp;
    bp->next = NULL;

    // correct thread processing breakpoints
    for(VMLocalBreak *local = m_local; local; local = local->next) {
        if( !local->bp_next ) {
            TRACE2("jvmti.break", "Set local thread next breakpoint: "
                << local << ", next: "
                << (bp->method
                    ? class_get_name(method_get_class((Method*)bp->method)) : "(nil)")
                << "."
                << (bp->method ? method_get_name((Method*)bp->method) : "(nil)")
                << (bp->method ? method_get_descriptor((Method*)bp->method) : "")
                << " :" << bp->location << " :" << bp->addr);
            local->bp_next = bp;
        }
    }
    return;
}

bool
VMBreakPoints::insert_native_breakpoint(VMBreakPoint* bp)
{
    LMAutoUnlock lock(get_lock());

    assert(!interpreter_enabled());
    bool UNREF check = check_insert_breakpoint(bp);
    assert(check);
    if (bp->method != NULL)
    { // JIT breakpoint
        Method *m = (Method *)bp->method;

        if (m->get_state() == Method::ST_Compiled)
        {
            if (!set_jit_mode_breakpoint(bp))
                return false;
        }
        else
        {
            assert(bp->addr == NULL);
            TRACE2("jvmti.break.intf", "Skipping setting breakpoint in method "
                << class_get_name(method_get_class(m)) << "."
                << method_get_name(m)
                << method_get_descriptor(m)
                << " because it is not compiled yet");
            m->insert_pending_breakpoint();
        }
    }
    else
    {
        if (!set_native_breakpoint(bp))
            return false;
    }
    insert_breakpoint(bp);
    return true;
}

bool
VMBreakPoints::insert_interpreter_breakpoint(VMBreakPoint* bp)
{
    LMAutoUnlock lock(get_lock());

    assert(interpreter_enabled());
    bool UNREF check = check_insert_breakpoint(bp);
    assert(check);
    bp->saved_byte =
        interpreter.interpreter_ti_set_breakpoint(bp->method, bp->location);

    insert_breakpoint(bp);
    return true;
}

inline void
VMBreakPoints::remove_breakpoint(VMBreakPoint* bp)
{
    TRACE2("jvmti.break.intf", "Remove breakpoint: "
        << (bp->method ? class_get_name(method_get_class((Method*)bp->method)) : "(nil)" )
        << "."
        << (bp->method ? method_get_name((Method*)bp->method) : "(nil)" )
        << (bp->method ? method_get_descriptor((Method*)bp->method) : "" )
        << " :" << bp->location << " :" << bp->addr);

    // remove breakpoint from list
    VMBreakPoint *last = NULL;
    for( VMBreakPoint *index = m_break;
         index;
         last = index, index = index->next )
    {
        if(index == bp) {
            if(m_last == bp) {
                m_last = last;
            }
            if(last) {
                last->next = index->next;
            } else {
                m_break = index->next;
            }
            break;
        }
    }

    // correct thread processing breakpoints
    for(VMLocalBreak *local = m_local; local; local = local->next) {
        if( local->bp == bp ) {
            // processed breakpoint was removed
            TRACE2("jvmti.break", "Remove local thread breakpoint: "
                << local << ", bp: "
                << (bp->method
                    ? class_get_name(method_get_class((Method*)bp->method)) : "(nil)")
                << "."
                << (bp->method ? method_get_name((Method*)bp->method) : "(nil)")
                << (bp->method ? method_get_descriptor((Method*)bp->method) : "")
                << " :" << bp->location << " :" << bp->addr);

            local->bp = NULL;
        } else if( local->bp_next == bp ) {
            // set new next breakpoint
            TRACE2("jvmti.break", "Set local thread next breakpoint: "
                << local << ", next: "
                << (bp->method
                    ? class_get_name(method_get_class((Method*)bp->method)) : "(nil)")
                << "."
                << (bp->method ? method_get_name((Method*)bp->method) : "(nil)")
                << (bp->method ? method_get_descriptor((Method*)bp->method) : "")
                << " :" << bp->location << " :" << bp->addr);
            local->bp_next = bp->next;
        }
    }
    return;
}

bool
VMBreakPoints::remove_native_breakpoint(VMBreakPoint* bp)
{
    assert(bp);
    assert(!bp->method || find_breakpoint(bp->method, bp->location));
    assert(bp->method || find_breakpoint(bp->addr));
    assert(!interpreter_enabled());

    LMAutoUnlock lock(get_lock());
    remove_breakpoint(bp);
    if (bp->addr)
    {
        assert(!bp->method || (((Method*)bp->method)->get_state() == Method::ST_Compiled));
        return clear_native_breakpoint(bp);
    }
    else
    {
        assert(bp->method && (((Method*)bp->method)->get_state() != Method::ST_Compiled));
        Method *m = (Method *)bp->method;
        m->remove_pending_breakpoint();
    }
    return true;
}

bool
VMBreakPoints::remove_interpreter_breakpoint(VMBreakPoint* bp)
{
    assert(bp);
    assert(bp->method);
    assert(find_breakpoint(bp->method, bp->location));
    assert(interpreter_enabled());

    LMAutoUnlock lock(get_lock());
    remove_breakpoint(bp);
    interpreter.interpreter_ti_clear_breakpoint(bp->method,
        bp->location, bp->saved_byte);
    return true;
}

VMBreakPoint*
VMBreakPoints::find_breakpoint(jmethodID method, jlocation location)
{
    for (VMBreakPoint* brpt = m_break; brpt; brpt = brpt->next)
    {
        if (brpt->method == method &&
            brpt->location == location)
            return brpt;
    }

    return NULL;
}

VMBreakPoint*
VMBreakPoints::find_breakpoint(NativeCodePtr addr)
{
    assert(addr);

    for (VMBreakPoint* brpt = m_break; brpt; brpt = brpt->next)
    {
        if (brpt->addr == addr)
            return brpt;
    }

    return NULL;
}


VMBreakPoint*
VMBreakPoints::find_other_breakpoint_with_same_addr(VMBreakPoint* bp)
{
    assert(bp);

    for (VMBreakPoint* other = m_break; other; other = other->next)
    {
        if (other != bp && other->addr == bp->addr)
            return other;
    }

    return NULL;
}

VMBreakPoint*
VMBreakPoints::find_next_breakpoint(VMBreakPoint* prev, NativeCodePtr addr)
{
    assert(addr && prev);

    for (VMBreakPoint* brpt = prev->next; brpt; brpt = brpt->next)
    {
        if (brpt->addr == addr)
            return brpt;
    }

    return NULL;
}

VMBreakPoint*
VMBreakPoints::find_next_breakpoint(VMBreakPoint* prev,
                                    jmethodID method,
                                    jlocation location)
{
    assert(prev);
    assert(method);

    for (VMBreakPoint* bp = prev->next; bp; bp = bp->next) {
        if (bp->method == method && bp->location == location) {
            return bp;
        }
    }

    return NULL;
}

VMBreakPoint*
VMBreakPoints::find_method_breakpoint(jmethodID method)
{
    assert(method);

    for (VMBreakPoint* brpt = m_break; brpt; brpt = brpt->next)
    {
        if (brpt->method &&
            brpt->method == method)
            return brpt;
    }

    return NULL;
}

VMBreakPoint*
VMBreakPoints::find_next_method_breakpoint(VMBreakPoint* prev, jmethodID method)
{
    assert(prev);

    for (VMBreakPoint* brpt = prev->next; brpt; brpt = brpt->next)
    {
        if (brpt->method == method)
            return brpt;
    }

    return NULL;
}

VMBreakPointRef*
VMBreakPoints::find_other_reference(VMBreakInterface* intf,
                                    jmethodID method,
                                    jlocation location)
{
    assert(intf);

    for( unsigned index = 0; index < PRIORITY_NUMBER; index++ ) {
        for (VMBreakInterface* cur = m_intf[index]; cur; cur = cur->m_next) {
            if (cur == intf)
                continue;

            VMBreakPointRef* ref = cur->find_reference(method, location);

            if (ref)
                return ref;
        }
    }

    return NULL;
}

VMBreakPointRef*
VMBreakPoints::find_other_reference(VMBreakInterface* intf,
                                    NativeCodePtr addr)
{
    assert(intf);

    for( unsigned index = 0; index < PRIORITY_NUMBER; index++ ) {
        for (VMBreakInterface* cur = m_intf[index]; cur; cur = cur->m_next) {
            if (cur == intf)
                continue;

            VMBreakPointRef* ref = cur->find_reference(addr);

            if (ref)
                return ref;
        }
    }

    return NULL;
}

VMBreakPointRef*
VMBreakPoints::find_other_reference(VMBreakInterface* intf,
                                    VMBreakPoint* brpt)
{
    assert(intf);

    for( unsigned index = 0; index < PRIORITY_NUMBER; index++ ) {
        for (VMBreakInterface* cur = m_intf[index]; cur; cur = cur->m_next) {
            if (cur == intf)
                continue;

            VMBreakPointRef* ref = cur->find_reference(brpt);

            if (ref)
                return ref;
        }
    }

    return NULL;
}

void
VMBreakPoints::set_thread_local_break(VMLocalBreak *local)
{
    local->next = m_local;
    m_local = local;
    TRACE2( "jvmti.break", "Set local thread structure: " << local);
}

void
VMBreakPoints::remove_thread_local_break(VMLocalBreak *local)
{
    TRACE2( "jvmti.break", "Remove local thread structure: " << local);
    VMLocalBreak *last = NULL;
    for( VMLocalBreak *index = m_local;
         index;
         last = index, index = index->next )
    {
        if(index == local) {
            if(last) {
                last->next = index->next;
            } else {
                m_local = index->next;
            }
            return;
        }
    }
    assert(false);
}

void
VMBreakPoints::process_native_breakpoint()
{
#if (defined _IA32_) || (defined _EM64T_)
    // When we get here we know already that breakpoint occurred in JITted code,
    // JVMTI handles it, and registers context is saved for us in TLS
    VM_thread *vm_thread = p_TLS_vmthread;
    lock();
    Registers regs = vm_thread->jvmti_saved_exception_registers;
    NativeCodePtr addr = (NativeCodePtr)regs.get_ip();

    TRACE2("jvmti.break", "Native breakpoint occured: " << addr);

    VMBreakPoint* bp = find_breakpoint(addr);
    if (NULL == bp) {
        // breakpoint could be deleted by another thread
        unlock();
        return;
    }
    assert(bp->addr == addr);
    TRACE2("jvmti.break", "Process native breakpoint: "
        << (bp->method
            ? class_get_name(method_get_class((Method*)bp->method)) : "(nil)")
        << "."
        << (bp->method ? method_get_name((Method*)bp->method) : "(nil)")
        << (bp->method ? method_get_descriptor((Method*)bp->method) : "")
        << " :" << bp->location << " :" << bp->addr);

    M2nFrame* m2nf = m2n_push_suspended_frame(&regs);

    jbyte *instruction_buffer;
    BEGIN_RAISE_AREA;

    // need to be able to pop the frame
    frame_type m2nf_type = m2n_get_frame_type(m2nf);
    m2nf_type = (frame_type) (m2nf_type | FRAME_POPABLE);
    m2n_set_frame_type(m2nf, m2nf_type);

    jbyte orig_byte = bp->saved_byte;

    // Copy disassembler instance in case a breakpoint is deleted
    // inside of callbacks
    InstructionDisassembler idisasm(*bp->disasm);

    for (unsigned priority = 0; priority < PRIORITY_NUMBER; priority++)
    {
        bp = find_breakpoint(addr);
        assert(!bp || bp->addr == addr);
        VMLocalBreak local;
        local.priority = priority;
        while( bp )
        {
            assert(bp->addr == addr);
            // copy breakpoint to local thread variable
            local.bp = bp;
            local.bp_next = find_next_breakpoint(bp, addr);

            VMBreakInterface *intf = get_first_intf(priority);
            while( intf )
            {
                VMBreakPointRef* ref = intf->find_reference(bp);
                assert(!ref || ref->bp->addr == addr);

                if (ref && intf->breakpoint_event_callback != NULL)
                {
                    local.intf = intf->m_next;
                    VMBreakPoint local_bp = *bp;
                    void *data = ref->data;

                    Method *method = (Method*)bp->method;
                    jlocation location = bp->location;
                    NativeCodePtr addr = bp->addr;
                    TRACE2("jvmti.break",
                        "Calling native breakpoint callback function: "
                        << (method
                            ? class_get_name(method_get_class(method)) : "(nil)")
                        << "."
                        << (method ? method_get_name(method) : "(nil)")
                        << (method ? method_get_descriptor(method) : "")
                        << " :" << location << " :" << addr);

                    set_thread_local_break(&local);
                    unlock();

                    // call event breakpoint callback
                    intf->breakpoint_event_callback(intf->get_env(), &local_bp, data);

                    lock();
                    remove_thread_local_break(&local);

                    TRACE2("jvmti.break",
                        "Finished native breakpoint callback function: "
                        << (method
                            ? class_get_name(method_get_class(method)) : "(nil)")
                        << "."
                        << (method ? method_get_name(method) : "(nil)")
                        << (method ? method_get_descriptor(method) : "")
                        << " :" << location << " :" << addr);

                    if( !local.bp ) {
                        // breakpoint was removed, no need report it anymore
                        break;
                    }
                    intf = local.intf;
                } else {
                    intf = intf->m_next;
                }
            }
            bp = local.bp_next;
            if( bp && bp->addr != addr ) {
                bp = find_next_breakpoint(bp, addr);
            }
        }
    }

    // Registers in TLS can be changed in user callbacks
    // It should be restored to keep original address of instrumented instruction
    // Exception/signal handlers use it when HWE occurs in instruction buffer
    vm_thread->jvmti_saved_exception_registers = regs;
    unlock();

    // Now we need to return back to normal code execution, it is
    // necessary to execute the original instruction The idea is to
    // recreate the original instruction in a special thread local
    // buffer followed by a jump to an instruction next after it. In
    // case the instruction was a relative jump or call it requires
    // special handling.
    InstructionDisassembler::Type type = idisasm.get_type();

    instruction_buffer = vm_thread->jvmti_jit_breakpoints_handling_buffer;
    jbyte *interrupted_instruction = (jbyte *)addr;
    jint instruction_length = idisasm.get_length_with_prefix();

    switch(type)
    {
    case InstructionDisassembler::UNKNOWN:
    {
        char *next_instruction = (char *)interrupted_instruction +
            instruction_length;

        // Copy original instruction to the execution buffer
        *instruction_buffer = orig_byte;
        memcpy(instruction_buffer + 1, interrupted_instruction + 1,
            instruction_length - 1);

        // Create JMP $next_instruction instruction in the execution buffer
        jump((char *)instruction_buffer + instruction_length,
            next_instruction);
        break;
    }
    case InstructionDisassembler::RELATIVE_JUMP:
    {
        jint instruction_length = idisasm.get_length_with_prefix();
        char *jump_target = (char *)idisasm.get_jump_target_address();

        // Create JMP to the absolute address which conditional jump
        // had in the execution buffer
        jump((char *)instruction_buffer, jump_target);
        break;
    }
    case InstructionDisassembler::RELATIVE_COND_JUMP:
    {
        char *code = (char *)instruction_buffer;
        InstructionDisassembler::CondJumpType jump_type =
            idisasm.get_cond_jump_type();
        char *next_instruction = (char *)interrupted_instruction +
            instruction_length;
        char *jump_target = (char *)idisasm.get_jump_target_address();

        // Create a conditional JMP of the same type over 1
        // instruction forward, the next instruction is JMP to the
        // $next_instruction
        code = branch8(code, get_condition_code(jump_type), Imm_Opnd(size_8, 0));
        char *branch_address = code - 1;

        code = jump(code, next_instruction);
        jint offset = (jint)(code - branch_address - 1);
        *branch_address = offset;

        jump(code, jump_target);
        break;
    }
    case InstructionDisassembler::RELATIVE_CALL:
    {
        jbyte *next_instruction = interrupted_instruction + instruction_length;
        char *jump_target = (char *)idisasm.get_jump_target_address();
        char *code = (char *)instruction_buffer;

        // Push "return address" to the $next_instruction
        code = push(code, Imm_Opnd(size_platf, (POINTER_SIZE_INT)next_instruction));

        // Jump to the target address of the call instruction
        jump(code, jump_target);
        break;
    }
    case InstructionDisassembler::INDIRECT_JUMP:
    {
        jint instruction_length = idisasm.get_length_with_prefix();
        char *jump_target = (char *)idisasm.get_target_address_from_context(&regs);

        // Create JMP to the absolute address which conditional jump
        // had in the execution buffer
        jump((char *)instruction_buffer, jump_target);
        break;
    }
    case InstructionDisassembler::INDIRECT_CALL:
    {
        jbyte *next_instruction = interrupted_instruction + instruction_length;
        char *jump_target = (char *)idisasm.get_target_address_from_context(&regs);
        char *code = (char *)instruction_buffer;

        // Push "return address" to the $next_instruction
        code = push(code, Imm_Opnd(size_platf, (POINTER_SIZE_INT)next_instruction));

        // Jump to the target address of the call instruction
        jump(code, jump_target);
        break;
    }
    }

    END_RAISE_AREA;

    // This function does not return. It restores register context and
    // transfers execution control to the instruction buffer to
    // execute the original instruction with the registers which it
    // had before breakpoint happened
    StackIterator *si =
        si_create_from_registers(&regs, false, m2n_get_previous_frame(m2nf));

    si_set_ip(si, instruction_buffer, false);
    si_transfer_control(si);
#else
    // PLATFORM dependent code
    abort();
#endif
}

jbyte
VMBreakPoints::process_interpreter_breakpoint(jmethodID method, jlocation location)
{
    TRACE2("jvmti.break.intf", "Interpreter breakpoint occured: "
        << class_get_name(method_get_class((Method*)method)) << "."
        << method_get_name((Method*)method)
        << method_get_descriptor((Method*)method)
        << " :" << location );

    assert(interpreter_enabled());

    lock();
    VMBreakPoint* bp = find_breakpoint(method, location);
    if(NULL == bp) {
        // breakpoint could be deleted by another thread
        unlock();
        return (jbyte)OPCODE_COUNT;
    }
    assert(bp->method == method);
    assert(bp->location == location);
    TRACE2("jvmti.break", "Process interpreter breakpoint: "
        << class_get_name(method_get_class((Method*)method)) << "."
        << method_get_name((Method*)method)
        << method_get_descriptor((Method*)method)
        << " :" << location );

    jbyte orig_byte = bp->saved_byte;
    for (unsigned priority = 0; priority < PRIORITY_NUMBER; priority++)
    {
        bp = find_breakpoint(method, location);;
        assert(bp->method == method);
        assert(bp->location == location);
        VMLocalBreak local;
        local.priority = priority;
        while( bp )
        {
            assert(bp->method == method);
            assert(bp->location == location);
            // copy breakpoint to local thread variable
            local.bp = bp;
            local.bp_next = find_next_breakpoint(bp, method, location);

            VMBreakInterface *intf = get_first_intf(priority);
            while( intf )
            {
                VMBreakPointRef* ref = intf->find_reference(bp);
                assert(!ref || ref->bp->method == method);
                assert(!ref || ref->bp->location == location);

                if (ref && intf->breakpoint_event_callback != NULL)
                {
                    local.intf = intf->m_next;
                    VMBreakPoint local_bp = *bp;
                    void *data = ref->data;

                    TRACE2("jvmti.break.intf",
                        "Calling interpreter breakpoint callback function: "
                        << class_get_name(method_get_class((Method*)method)) << "."
                        << method_get_name((Method*)method)
                        << method_get_descriptor((Method*)method)
                        << " :" << location );

                    set_thread_local_break(&local);
                    unlock();

                    // call event breakpoint callback
                    intf->breakpoint_event_callback(intf->get_env(), &local_bp, data);

                    lock();
                    remove_thread_local_break(&local);

                    TRACE2("jvmti.break",
                        "Finished interpreter breakpoint callback function: "
                        << class_get_name(method_get_class((Method*)method)) << "."
                        << method_get_name((Method*)method)
                        << method_get_descriptor((Method*)method)
                        << " :" << location );

                    if( !local.bp ) {
                        // breakpoint was removed, no need report it anymore
                        break;
                    }
                    intf = local.intf;
                } else {
                    intf = intf->m_next;
                }
            }
            bp = local.bp_next;
            if( bp && !(bp->method == method && bp->location == location) ) {
                bp = find_next_breakpoint(bp, method, location);
            }
        }
    }
    unlock();

    return orig_byte;
}

//////////////////////////////////////////////////////////////////////////////
// VMBreakInterface implementation

static bool insert_native_breakpoint(VMBreakPoint *bp)
{
    return VM_Global_State::loader_env->
        TI->vm_brpt->insert_native_breakpoint(bp);
}

static bool insert_interpreter_breakpoint(VMBreakPoint *bp)
{
    return VM_Global_State::loader_env->
        TI->vm_brpt->insert_interpreter_breakpoint(bp);
}

static bool remove_native_breakpoint(VMBreakPoint *bp)
{
    return VM_Global_State::loader_env->
        TI->vm_brpt->remove_native_breakpoint(bp);
}

static bool remove_interpreter_breakpoint(VMBreakPoint *bp)
{
    return VM_Global_State::loader_env->
        TI->vm_brpt->remove_interpreter_breakpoint(bp);
}

VMBreakInterface::VMBreakInterface(TIEnv *env,
                 BPInterfaceCallBack callback,
                 unsigned priority,
                 bool is_interp)
    : m_next(NULL), breakpoint_event_callback(callback), m_list(NULL),
      m_env(env), m_priority(priority)
{
    if(is_interp) {
        breakpoint_insert = &insert_interpreter_breakpoint;
        breakpoint_remove = &remove_interpreter_breakpoint;
    } else {
        breakpoint_insert = &insert_native_breakpoint;
        breakpoint_remove = &remove_native_breakpoint;
    }
}

inline VMBreakPointRef*
VMBreakInterface::add_reference_internal(VMBreakPoint *bp, void *data)
{
    VMBreakPointRef* bp_ref =
        (VMBreakPointRef*)STD_MALLOC(sizeof(VMBreakPointRef));
    assert(bp_ref);

    bp_ref->bp = bp;
    bp_ref->data = data;
    bp_ref->next = m_list;
    m_list = bp_ref;

    TRACE2("jvmti.break.intf", "Added ref on breakpoint: "
        << (bp->method
            ? class_get_name(method_get_class((Method*)bp->method)) : "(nil)" )
        << "."
        << (bp->method ? method_get_name((Method*)bp->method) : "(nil)")
        << (bp->method ? method_get_descriptor((Method*)bp->method) : "")
        << " :" << bp->location << " :" << bp->addr << ", data: " << data);

    return bp_ref;
}

VMBreakPointRef*
VMBreakInterface::add_reference(jmethodID method, jlocation location, void* data)
{
    assert(method);

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;
    LMAutoUnlock lock(vm_brpt->get_lock());

    // find existing reference
    VMBreakPointRef *ref = find_reference(method, location);
    if( ref && ref->data == data ) {
        return ref;
    }

    VMBreakPoint* brpt = vm_brpt->find_breakpoint(method, location);
    if (!brpt)
    {
        brpt = (VMBreakPoint*)STD_MALLOC(sizeof(VMBreakPoint));
        assert(brpt);

        brpt->addr = NULL;
        brpt->method = method;
        brpt->location = location;
        brpt->saved_byte = 0;
        brpt->disasm = NULL;

        // Insert breakpoint, possibly to the same native address
        if (!breakpoint_insert(brpt))
        {
            STD_FREE(brpt);
            return false;
        }
    }
    return add_reference_internal( brpt, data );
}

VMBreakPointRef*
VMBreakInterface::add_reference(jmethodID method, jlocation location,
                                NativeCodePtr addr, void* data)
{
    assert(method);

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;
    LMAutoUnlock lock(vm_brpt->get_lock());

    // find existing reference
    VMBreakPointRef *ref = find_reference(method, location);
    if( ref && (!addr || addr == ref->bp->addr) && data == ref->data ) {
        return ref;
    }

    VMBreakPoint* brpt = vm_brpt->find_breakpoint(method, location);

    // If breakpoint with the same method location is not found or
    // given native address is differ with obtained breakpoint.
    // The last case could be if the same method location points
    // to different native address.
    if ( !brpt || brpt->addr != addr )
    {
        brpt = (VMBreakPoint*)STD_MALLOC(sizeof(VMBreakPoint));
        assert(brpt);

        brpt->addr = addr;
        brpt->method = method;
        brpt->location = location;
        brpt->saved_byte = 0;
        brpt->disasm = NULL;

        if (!breakpoint_insert(brpt))
        {
            STD_FREE(brpt);
            return false;
        }
    }
    return add_reference_internal( brpt, data );
}

VMBreakPointRef*
VMBreakInterface::add_reference(NativeCodePtr addr, void* data)
{
    assert(addr);
    assert(!interpreter_enabled());

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;
    LMAutoUnlock lock(vm_brpt->get_lock());

    // find existing reference
    VMBreakPointRef *ref = find_reference(addr);
    if( ref && ref->data == data ) {
        return ref;
    }

    VMBreakPoint* brpt = vm_brpt->find_breakpoint(addr);
    if (!brpt)
    {
        brpt = (VMBreakPoint*)STD_MALLOC(sizeof(VMBreakPoint));
        assert(brpt);

        brpt->addr = addr;
        brpt->method = NULL;
        brpt->location = 0;
        brpt->saved_byte = 0;
        brpt->disasm = NULL;

        // Insert breakpoint, possibly duplicating breakpoint with method != NULL
        if (!breakpoint_insert(brpt))
        {
            STD_FREE(brpt);
            return false;
        }
    }
    return add_reference_internal( brpt, data );
}

bool
VMBreakInterface::remove_reference(VMBreakPointRef* bp_ref)
{
    assert(bp_ref);

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;
    LMAutoUnlock lock(vm_brpt->get_lock());

    TRACE2("jvmti.break.intf", "Remove reference on breakpoint: "
        << (bp_ref->bp->method
            ? class_get_name(method_get_class((Method*)bp_ref->bp->method)) : "(nil)")
        << "."
        << (bp_ref->bp->method ? method_get_name((Method*)bp_ref->bp->method) : "(nil)")
        << (bp_ref->bp->method ? method_get_descriptor((Method*)bp_ref->bp->method) : "")
        << " :" << bp_ref->bp->location << " :" << bp_ref->bp->addr
        << ", data: " << bp_ref->data );

    VMBreakPointRef* found = NULL;

    for (VMBreakPointRef** cur_ptr = &m_list;
         *cur_ptr; cur_ptr = &(*cur_ptr)->next)
    {
        if (*cur_ptr == bp_ref)
        {
            found = *cur_ptr;
            *cur_ptr = (*cur_ptr)->next;
            break;
        }
    }

    assert(found);

    VMBreakPoint* brpt = found->bp;
    assert(brpt);

    if (found->data)
        _deallocate((unsigned char*)found->data);

    STD_FREE(found);

    if (vm_brpt->find_other_reference(this, brpt))
        return true; // There are some other references to the same breakpoint

    if (!breakpoint_remove(brpt))
        return false;

    STD_FREE(brpt);
    return true;
}

VMBreakPointRef*
VMBreakInterface::find_reference(jmethodID method, jlocation location)
{
    assert(method);

    for (VMBreakPointRef* ref = m_list; ref; ref = ref->next)
    {
        if (ref->bp->method &&
            ref->bp->method == method &&
            ref->bp->location == location)
        {
            return ref;
        }
    }

    return NULL;
}

VMBreakPointRef*
VMBreakInterface::find_reference(NativeCodePtr addr)
{
    assert(addr);

    for (VMBreakPointRef* ref = m_list; ref; ref = ref->next)
    {
        if (ref->bp->addr == addr)
        {
            return ref;
        }
    }

    return NULL;
}

VMBreakPointRef*
VMBreakInterface::find_reference(VMBreakPoint* brpt)
{
    assert(brpt);

    for (VMBreakPointRef* ref = m_list; ref; ref = ref->next)
    {
        if (ref->bp == brpt)
        {
            return ref;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////////
// Helper functions

#if (defined _IA32_) || (defined _EM64T_)
static inline ConditionCode
get_condition_code(InstructionDisassembler::CondJumpType jump_type)
{
    // Right now InstructionDisassembler::CondJumpType enum values are
    // equal to enums in ia32/em64t encoder, so this statement is ok
    return (ConditionCode)jump_type;
}
#endif

static bool set_jit_mode_breakpoint(VMBreakPoint* bp)
{
    assert(bp);

    // Find native location in the method code
    Method *m = (Method *)bp->method;
    assert(m);
    assert( m->get_state() == Method::ST_Compiled );

    NativeCodePtr np = bp->addr;
    if (!np)
    {
        OpenExeJpdaError res = EXE_ERROR_NONE;
        for (CodeChunkInfo* cci = m->get_first_JIT_specific_info();
             cci; cci = cci->_next)
        {
            JIT *jit = cci->get_jit();
            res = jit->get_native_location_for_bc(m, (uint16)bp->location, &np);
            if (res == EXE_ERROR_NONE)
                break;
        }

        if (NULL == np)
            return false;

        bp->addr = np;
    }

    TRACE2("jvmti.break.intf", "Set JIT breakpoint: "
        << class_get_name(method_get_class((Method*)bp->method)) << "."
        << method_get_name((Method*)bp->method)
        << method_get_descriptor((Method*)bp->method)
        << " :" << bp->location << " :" << bp->addr);

    return set_native_breakpoint(bp);
}

static bool set_native_breakpoint(VMBreakPoint* bp)
{
#if (defined _IA32_) || (defined _EM64T_)
    assert(bp);
    assert(bp->addr);

    TRACE2("jvmti.break.intf", "Instrumenting native: "
        << (bp->method ? class_get_name(method_get_class((Method*)bp->method)) : "(nil)" )
        << "."
        << (bp->method ? method_get_name((Method*)bp->method) : "(nil)" )
        << (bp->method ? method_get_descriptor((Method*)bp->method) : "" )
        << " :" << bp->location << " :" << bp->addr);

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;

    // Look for breakpoint with identical addr
    VMBreakPoint* other_bp = vm_brpt->find_other_breakpoint_with_same_addr(bp);
    if (other_bp)
    {
        assert(other_bp->disasm);
        bp->disasm = new InstructionDisassembler(*other_bp->disasm);
        assert(bp->disasm);
        bp->saved_byte = other_bp->saved_byte;
    }
    else
    {
        bp->disasm = new InstructionDisassembler(bp->addr);
        assert(bp->disasm);

        // code instrumentation
        jbyte* target_instruction = (jbyte*)bp->addr;
        bp->saved_byte = *target_instruction;
        *target_instruction = (jbyte)INSTRUMENTATION_BYTE;
    }

    return true;
#else
    return false;
#endif
}

static bool clear_native_breakpoint(VMBreakPoint* bp)
{
    assert(bp);
    assert(bp->addr);

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;

    // Looking for another breakpoint with the same address,
    // current breakpoint is already removed from breakpoint list.
    if (!vm_brpt->find_breakpoint(bp->addr))
    {
        TRACE2("jvmti.break.intf", "Deinstrumentation native: "
            << (bp->method ? class_get_name(method_get_class((Method*)bp->method)) : "" )
            << "."
            << (bp->method ? method_get_name((Method*)bp->method) : "" )
            << (bp->method ? method_get_descriptor((Method*)bp->method) : "" )
            << " :" << bp->location << " :" << bp->addr);
        jbyte* target_instruction = (jbyte*)bp->addr;
        *target_instruction = bp->saved_byte;
    }

    delete bp->disasm;
    return true;
}


//////////////////////////////////////////////////////////////////////////////
// Native breakpoints
//////////////////////////////////////////////////////////////////////////////


void __cdecl process_native_breakpoint_event()
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    ti->vm_brpt->process_native_breakpoint();
}

#if defined (_WIN32) && !defined(_EM64T_)
static void __declspec(naked)
asm_process_native_breakpoint_event()
{
    __asm {
    push    ebp
    mov     ebp,esp
    pushfd
    cld
    call    process_native_breakpoint_event
    popfd
    pop     ebp
    ret
    }
}
#endif // _WIN32

bool jvmti_jit_breakpoint_handler(Registers *regs)
{
#if PLATFORM_POSIX && INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_INT3
    // Int3 exception address points to the instruction after it
    NativeCodePtr native_location = (NativeCodePtr)(((POINTER_SIZE_INT)regs->get_ip()) - 1);
#else
    NativeCodePtr native_location = (NativeCodePtr)regs->get_ip();
#endif
    ASSERT_NO_INTERPRETER;

    TRACE2("jvmti.break", "BREAKPOINT occured: " << native_location);

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() || ti->getPhase() != JVMTI_PHASE_LIVE)
        return false;

    // Now it is necessary to set up a transition to
    // process_native_breakpoint_event from the exception/signal handler
    VM_thread *vm_thread = p_TLS_vmthread;
    // Store possibly corrected location
    regs->set_ip((void*)native_location);
    // Copy original registers to TLS
    vm_thread->jvmti_saved_exception_registers = *regs;
    // Set return address for exception handler
#if defined (PLATFORM_POSIX) || defined(_EM64T_)
    regs->set_ip((void*)process_native_breakpoint_event);
#else // PLATFORM_POSIX
    regs->set_ip((void*)asm_process_native_breakpoint_event);
#endif //PLATFORM_POSIX

    return true;
}

// Called when method compilation has completed
void jvmti_set_pending_breakpoints(Method *method)
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return;

    VMBreakPoints* vm_brpt = ti->vm_brpt;
    LMAutoUnlock lock(vm_brpt->get_lock());

    if( !method->get_pending_breakpoints() )
        return;

    VMBreakPoint* bp = vm_brpt->find_method_breakpoint((jmethodID)method);
    assert(bp);

    jlocation *locations = (jlocation *)STD_MALLOC(sizeof(jlocation) *
        method->get_pending_breakpoints());
    assert(locations);
    uint32 location_count = 0;

    do
    {
        // It is necessary to set breakpoints only once for each
        // location, so we need to filter out duplicate breakpoints
        for (uint32 iii = 0; iii < location_count; iii++)
            if (bp->location == locations[iii])
                continue;

        set_jit_mode_breakpoint(bp);
        locations[location_count++] = bp->location;

        method->remove_pending_breakpoint();
        bp = vm_brpt->find_next_method_breakpoint(bp, (jmethodID)method);
    }
    while (NULL != bp);

    assert(method->get_pending_breakpoints() == 0);
    STD_FREE(locations);
    return;
}

//////////////////////////////////////////////////////////////////////////////
// Interpreter breakpoints
//////////////////////////////////////////////////////////////////////////////

VMEXPORT jbyte
jvmti_process_interpreter_breakpoint_event(jmethodID method, jlocation location)
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return false;

    return ti->vm_brpt->process_interpreter_breakpoint(method, location);
}
