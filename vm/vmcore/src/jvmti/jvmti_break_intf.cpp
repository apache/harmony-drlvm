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
 * @author Ilya Berezhniuk, Gregory Shimansky
 * @version $Revision$
 */  
/**
 * @file jvmti_break_intf.cpp
 * @brief JVMTI native breakpoints API implementation
 */

#include "cxxlog.h"
#include "environment.h"
#include "encoder.h"
#include "interpreter_exports.h"
#include "jit_intf_cpp.h"
#include "method_lookup.h"
#include "exceptions.h"
#include "m2n.h"
#include "stack_iterator.h"

#include "jvmti_break_intf.h"


// Forvard declarations
static ConditionCode
get_condition_code(InstructionDisassembler::CondJumpType jump_type);
static bool set_jit_mode_breakpoint(VMBreakPoints* vm_brpt, VMBreakPoint* bp);
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
VMBreakPoints::new_intf(BPInterfaceCallBack callback,
                        unsigned priority,
                        bool is_interp)
{
    assert(callback);
    assert(priority < PRIORITY_NUMBER);
    VMBreakInterface* intf = new VMBreakInterface(callback, priority, is_interp);
    assert(intf);

    lock();
    intf->m_next = m_intf[priority];
    m_intf[priority] = intf;
    unlock();

    return intf;
}

void
VMBreakPoints::release_intf(VMBreakInterface* intf)
{
    assert(intf);
    assert(intf->get_priority() < PRIORITY_NUMBER);
    LMAutoUnlock lock(get_lock());

    for (VMBreakInterface** cur_ptr = &m_intf[intf->get_priority()];
         *cur_ptr; cur_ptr = &((*cur_ptr)->m_next))
    {
        if (*cur_ptr == intf)
        {
            *cur_ptr = (*cur_ptr)->m_next;

            delete intf;
            return;
        }
    }
    DIE2("jvmti.break", "VMBreakPoints::release_intf: try to release unknown interface");
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
            << (another->method ? method_get_descriptor((Method*)another->method) : "(nil)")
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
            << (another->method ? method_get_descriptor((Method*)another->method) :"(nil)")
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

bool
VMBreakPoints::insert_breakpoint(VMBreakPoint* brpt)
{
    check_insert_breakpoint(brpt);
    if (brpt->is_interp)
    {
        assert(interpreter_enabled());
        brpt->saved_byte = (POINTER_SIZE_INT)
            interpreter.interpreter_ti_set_breakpoint(brpt->method, brpt->location);
    }
    else
    {
        if (brpt->method != NULL)
        { // JIT breakpoint
            Method *m = (Method *)brpt->method;

            if (m->get_state() == Method::ST_Compiled)
            {
                if (!set_jit_mode_breakpoint(this, brpt))
                    return false;
            }
            else
            {
                assert(brpt->addr == NULL);
                TRACE2("jvmti.break.intf", "Skipping setting breakpoing in method "
                    << class_get_name(method_get_class(m)) << "."
                    << method_get_name(m)
                    << method_get_descriptor(m)
                    << " because it is not compiled yet");
                m->insert_pending_breakpoint();
            }
        }
        else
        {
            if (!set_native_breakpoint(brpt))
                return false;
        }
    }
    TRACE2("jvmti.break.intf", "Insert breakpoint: "
        << class_get_name(method_get_class((Method*)brpt->method)) << "."
        << method_get_name((Method*)brpt->method)
        << method_get_descriptor((Method*)brpt->method)
        << " :" << brpt->location << " :" << brpt->addr);

    brpt->next = m_break;
    m_break = brpt;

    return true;
}

bool
VMBreakPoints::remove_breakpoint(VMBreakPoint* brpt)
{
    assert(brpt);
    assert(!brpt->method || find_breakpoint(brpt->method, brpt->location));
    assert(brpt->method || find_breakpoint(brpt->addr));

    TRACE2("jvmti.break.intf", "Remove breakpoint: "
        << class_get_name(method_get_class((Method*)brpt->method)) << "."
        << method_get_name((Method*)brpt->method)
        << method_get_descriptor((Method*)brpt->method)
        << " :" << brpt->location << " :" << brpt->addr);

    for (VMBreakPoint** cur_ptr = &m_break;
         *cur_ptr; cur_ptr = &(*cur_ptr)->next)
    {
        if (*cur_ptr == brpt)
        {
            *cur_ptr = (*cur_ptr)->next;
            break;
        }
    }

    if (brpt->is_interp)
    {
        assert(interpreter_enabled());
        interpreter.interpreter_ti_clear_breakpoint(brpt->method,
                                    brpt->location, brpt->saved_byte);
    }
    else
    {
        if (brpt->addr)
        {
            assert(!brpt->method || (((Method*)brpt->method)->get_state() == Method::ST_Compiled));
            return clear_native_breakpoint(brpt);
        }
        else
        {
            assert(brpt->method && (((Method*)brpt->method)->get_state() != Method::ST_Compiled));
            Method *m = (Method *)brpt->method;
            m->remove_pending_breakpoint();
        }
    }

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

bool
VMBreakPoints::has_breakpoint(jmethodID method)
{
    return (find_first(method) != NULL);
}

VMBreakPoint*
VMBreakPoints::find_first(jmethodID method)
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
VMBreakPoints::find_next(VMBreakPoint* prev, jmethodID method)
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

            VMBreakPointRef* ref = cur->find(method, location);

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

            VMBreakPointRef* ref = cur->find(addr);

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

            VMBreakPointRef* ref = cur->find(brpt);

            if (ref)
                return ref;
        }
    }

    return NULL;
}

void
VMBreakPoints::process_native_breakpoint()
{
    // When we get here we know already that breakpoint occurred in JITted code,
    // JVMTI handles it, and registers context is saved for us in TLS
    VM_thread *vm_thread = p_TLS_vmthread;
    Registers regs = vm_thread->jvmti_saved_exception_registers;

#if _IA32_ && PLATFORM_POSIX && INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_INT3
    // Int3 exception address points to the instruction after it
    regs.eip -= 1;
#endif //_IA32_ && PLATFORM_POSIX && INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_INT3
    NativeCodePtr addr = (NativeCodePtr)regs.get_ip();

    TRACE2("jvmti.break.intf", "Native breakpoint occured: " << addr);

    VMBreakPoint* bp = find_breakpoint(addr);
    assert(bp);

    bool push_frame = (vm_identify_eip(addr) == VM_TYPE_JAVA);
    M2nFrame* m2nf;

    if (push_frame)
    {
        m2nf = m2n_push_suspended_frame(&regs);
    }
    else
        m2nf = m2n_get_last_frame();

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
        while( bp = get_none_processed_breakpoint(addr) ) {
            set_breakpoint_processed(bp, true);

            VMBreakInterface* intf;
            while( intf = get_none_processed_intf(priority) )
            {
                intf->set_processed(true);
                VMBreakPointRef* ref = intf->find(bp);

                if (ref && bp->addr != addr)
                    break; // It's another breakpoint now...

                if (ref && intf->m_callback != NULL)
                {
                    TRACE2("jvmti.break.intf",
                        "Calling native breakpoint callback function: "
                        << class_get_name(method_get_class((Method*)bp->method)) << "."
                        << method_get_name((Method*)bp->method)
                        << method_get_descriptor((Method*)bp->method)
                        << " :" << bp->location << " :" << bp->addr );

                    intf->m_callback(intf, ref);

                    TRACE2("jvmti.break.intf",
                        "Finished native breakpoint callback function: " << addr );
                }
            }
            clear_intfs_processed_flags();
        }
        clear_breakpoints_processed_flags();
    }

    // Now we need to return back to normal code execution, it is
    // necessary to execute the original instruction The idea is to
    // recreate the original instriction in a special thread local
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

        // Copy original instruction to the exeuction buffer
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
        jint offset = code - branch_address - 1;
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
        code = push(code, Imm_Opnd((POINTER_SIZE_INT)next_instruction));

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
        code = push(code, Imm_Opnd((POINTER_SIZE_INT)next_instruction));

        // Jump to the target address of the call instruction
        jump(code, jump_target);
        break;
    }
    }

    unlock();

    END_RAISE_AREA;

    // This function does not return. It restores register context and
    // transfers execution control to the instruction buffer to
    // execute the original instruction with the registers which it
    // had before breakpoint happened
    StackIterator *si =
        si_create_from_registers(&regs, false, m2n_get_previous_frame(m2nf));

    if (push_frame)
    {
        m2n_set_last_frame(m2n_get_previous_frame(m2nf));
        STD_FREE(m2nf);
    }

    si_set_ip(si, instruction_buffer, false);
    si_transfer_control(si);
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
    assert(bp);

    jbyte orig_byte = bp->saved_byte;
    for (unsigned priority = 0; priority < PRIORITY_NUMBER; priority++)
    {
        VMBreakInterface* intf;

        while (intf = get_none_processed_intf(priority) )
        {
            intf->set_processed(true);
            VMBreakPointRef* ref = intf->find(bp);

            if (ref &&
                  ( bp->method != method ||
                    bp->location != location ))
                break; // It's another breakpoint now...

            if (ref && intf->m_callback != NULL)
            {
                JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
                TRACE2("jvmti.break.intf",
                    "Calling interpreter breakpoint callback function: "
                    << class_get_name(method_get_class((Method*)method)) << "."
                    << method_get_name((Method*)method)
                    << method_get_descriptor((Method*)method)
                    << " :" << location );

                intf->m_callback(intf, ref);

                TRACE2("jvmti.break.intf",
                    "Finished interpreter breakpoint callback function: "
                    << class_get_name(method_get_class((Method*)method)) << "."
                    << method_get_name((Method*)method)
                    << method_get_descriptor((Method*)method)
                    << " :" << location );
            }
        }
    }

    clear_intfs_processed_flags();
    unlock();

    return orig_byte;
}

void
VMBreakPoints::clear_breakpoints_processed_flags()
{
    LMAutoUnlock lock(get_lock());

    for(VMBreakPoint *bp = m_break; bp; bp = bp->next ) {
        set_breakpoint_processed( bp, false );
    }
}

VMBreakPoint*
VMBreakPoints::get_none_processed_breakpoint(NativeCodePtr addr)
{
    LMAutoUnlock lock(get_lock());

    for(VMBreakPoint *bp = find_breakpoint(addr);
        bp;
        bp = find_next_breakpoint(bp, addr) )
    {
        if(!breakpoint_is_processed(bp)) {
            return bp;
        }
    }

    return NULL;
}

void
VMBreakPoints::clear_intfs_processed_flags()
{
    LMAutoUnlock lock(get_lock());

    for(unsigned index = 0; index < PRIORITY_NUMBER; index++ ) {
        for (VMBreakInterface* intf = m_intf[index]; intf; intf = intf->m_next) {
            intf->set_processed(false);
        }
    }
}

VMBreakInterface*
VMBreakPoints::get_none_processed_intf(unsigned priority)
{
    LMAutoUnlock lock(get_lock());

    for (VMBreakInterface* intf = m_intf[priority]; intf; intf = intf->m_next) {
        if (!intf->is_processed()) {
            assert(intf->get_priority() == priority);
            return intf;
        }
    }

    return NULL;
}


//////////////////////////////////////////////////////////////////////////////
// VMBreakInterface implementation

VMBreakPointRef* VMBreakInterface::add(jmethodID method, jlocation location, void* data)
{
    assert(method);
    assert(!this->find(method, location));

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;
    VMBreakPoint* brpt = vm_brpt->find_breakpoint(method, location);

    if (!brpt)
    {
        brpt = (VMBreakPoint*)STD_MALLOC(sizeof(VMBreakPoint));
        assert(brpt);

        brpt->is_interp = m_is_interp;
        brpt->addr = NULL;
        brpt->method = method;
        brpt->location = location;
        brpt->saved_byte = 0;
        brpt->disasm = NULL;
        brpt->is_processed = false;

        // Insert breakpoint, possibly to the same native address
        if (!vm_brpt->insert_breakpoint(brpt))
        {
            STD_FREE(brpt);
            return false;
        }
    }

    VMBreakPointRef* brpt_ref =
        (VMBreakPointRef*)STD_MALLOC(sizeof(VMBreakPointRef));
    assert(brpt_ref);

    brpt_ref->brpt = brpt;
    brpt_ref->data = data;
    brpt_ref->next = m_list;
    m_list = brpt_ref;

    TRACE2("jvmti.break.intf", "Added ref on breakpoint: "
        << class_get_name(method_get_class((Method*)brpt_ref->brpt->method)) << "."
        << method_get_name((Method*)brpt_ref->brpt->method)
        << method_get_descriptor((Method*)brpt_ref->brpt->method)
        << " :" << brpt_ref->brpt->location << " :" << brpt_ref->brpt->addr);

    return brpt_ref;
}

VMBreakPointRef* VMBreakInterface::add(jmethodID method, jlocation location,
                            NativeCodePtr addr, void* data)
{
    assert(method);
    assert(!this->find(method, location));

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;
    VMBreakPoint* brpt = vm_brpt->find_breakpoint(method, location);

    // If breakpoint with the same method location is not found or
    // given native address is differ with obtained breapoint.
    // The last case cound be if the same method location points
    // to different native address.
    if ( !brpt || !brpt->addr || brpt->addr != addr )
    {
        brpt = (VMBreakPoint*)STD_MALLOC(sizeof(VMBreakPoint));
        assert(brpt);

        brpt->is_interp = m_is_interp;
        brpt->addr = addr;
        brpt->method = method;
        brpt->location = location;
        brpt->saved_byte = 0;
        brpt->disasm = NULL;
        brpt->is_processed = false;

        if (!vm_brpt->insert_breakpoint(brpt))
        {
            STD_FREE(brpt);
            return false;
        }
    }

    VMBreakPointRef* brpt_ref =
        (VMBreakPointRef*)STD_MALLOC(sizeof(VMBreakPointRef));
    assert(brpt_ref);

    brpt_ref->brpt = brpt;
    brpt_ref->data = data;
    brpt_ref->next = m_list;
    m_list = brpt_ref;

    TRACE2("jvmti.break.intf", "Added ref on breakpoint: "
        << class_get_name(method_get_class((Method*)brpt_ref->brpt->method)) << "."
        << method_get_name((Method*)brpt_ref->brpt->method)
        << method_get_descriptor((Method*)brpt_ref->brpt->method)
        << " :" << brpt_ref->brpt->location << " :" << brpt_ref->brpt->addr);

    return brpt_ref;
}

VMBreakPointRef* VMBreakInterface::add(NativeCodePtr addr, void* data)
{
    assert(addr);
    assert(!this->m_is_interp);
    assert(!this->find(addr));

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;
    VMBreakPoint* brpt = vm_brpt->find_breakpoint(addr);

    if (!brpt)
    {
        brpt = (VMBreakPoint*)STD_MALLOC(sizeof(VMBreakPoint));
        assert(brpt);

        brpt->is_interp = m_is_interp; // false
        brpt->addr = addr;
        brpt->method = NULL;
        brpt->location = 0;
        brpt->saved_byte = 0;
        brpt->disasm = NULL;
        brpt->is_processed = false;

        // Insert breakpoint, possibly duplicating breakpoint with method != NULL
        if (!vm_brpt->insert_breakpoint(brpt))
        {
            STD_FREE(brpt);
            return false;
        }
    }

    VMBreakPointRef* brpt_ref =
        (VMBreakPointRef*)STD_MALLOC(sizeof(VMBreakPointRef));
    assert(brpt_ref);

    brpt_ref->brpt = brpt;
    brpt_ref->data = data;
    brpt_ref->next = m_list;
    m_list = brpt_ref;

    TRACE2("jvmti.break.intf", "Added ref on breakpoint: "
        << class_get_name(method_get_class((Method*)brpt_ref->brpt->method)) << "."
        << method_get_name((Method*)brpt_ref->brpt->method)
        << method_get_descriptor((Method*)brpt_ref->brpt->method)
        << " :" << brpt_ref->brpt->location << " :" << brpt_ref->brpt->addr);

    return brpt_ref;
}

bool VMBreakInterface::remove(VMBreakPointRef* ref)
{
    assert(ref);

    TRACE2("jvmti.break.intf", "Remove ref on breakpoint: "
        << class_get_name(method_get_class((Method*)ref->brpt->method)) << "."
        << method_get_name((Method*)ref->brpt->method)
        << method_get_descriptor((Method*)ref->brpt->method)
        << " :" << ref->brpt->location << " :" << ref->brpt->addr);

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;
    VMBreakPointRef* found = NULL;

    for (VMBreakPointRef** cur_ptr = &m_list;
         *cur_ptr; cur_ptr = &(*cur_ptr)->next)
    {
        if (*cur_ptr == ref)
        {
            found = *cur_ptr;
            *cur_ptr = (*cur_ptr)->next;
            break;
        }
    }

    assert(found);

    VMBreakPoint* brpt = found->brpt;
    assert(brpt);

    if (found->data)
        _deallocate((unsigned char*)found->data);

    STD_FREE(found);

    if (vm_brpt->find_other_reference(this, brpt))
        return true; // There are some other references to the same breakpoint

    if (!vm_brpt->remove_breakpoint(brpt))
        return false;

    STD_FREE(brpt);
    return true;
}

VMBreakPointRef* VMBreakInterface::find(jmethodID method, jlocation location)
{
    assert(method);

    for (VMBreakPointRef* ref = m_list; ref; ref = ref->next)
    {
        if (ref->brpt->method &&
            ref->brpt->method == method &&
            ref->brpt->location == location)
        {
            return ref;
        }
    }

    return NULL;
}

VMBreakPointRef* VMBreakInterface::find(NativeCodePtr addr)
{
    assert(addr);

    for (VMBreakPointRef* ref = m_list; ref; ref = ref->next)
    {
        if (ref->brpt->addr == addr)
        {
            return ref;
        }
    }

    return NULL;
}

VMBreakPointRef* VMBreakInterface::find(VMBreakPoint* brpt)
{
    assert(brpt);

    for (VMBreakPointRef* ref = m_list; ref; ref = ref->next)
    {
        if (ref->brpt == brpt)
        {
            return ref;
        }
    }

    return NULL;
}

void VMBreakInterface::remove_all()
{
    while (m_list)
        remove(m_list);
}

//////////////////////////////////////////////////////////////////////////////
// Helper functions

static inline ConditionCode
get_condition_code(InstructionDisassembler::CondJumpType jump_type)
{
    // Right now InstructionDisassembler::CondJumpType enum values are
    // equal to enums in ia32/em64t encoder, so this statement is ok
    return (ConditionCode)jump_type;
}

static bool set_jit_mode_breakpoint(VMBreakPoints* vm_brpt, VMBreakPoint* bp)
{
    assert(bp);

    // Find native location in the method code
    Method *m = (Method *)bp->method;
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
    assert(bp);
    assert(bp->addr);

    TRACE2("jvmti.break.intf", "Instrumenting native: "
        << (bp->method ? class_get_name(method_get_class((Method*)bp->method)) : "" )
        << "."
        << (bp->method ? method_get_name((Method*)bp->method) : "" )
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

        // instrumening code
        jbyte* target_instruction = (jbyte*)bp->addr;
        bp->saved_byte = *target_instruction;
        *target_instruction = (jbyte)INSTRUMENTATION_BYTE;
    }

    return true;
}

static bool clear_native_breakpoint(VMBreakPoint* bp)
{
    assert(bp);
    assert(bp->addr);

    VMBreakPoints* vm_brpt = VM_Global_State::loader_env->TI->vm_brpt;

    // Looking for another breakpoint with the same address,
    // currect breakpoint is already removed from breakpoint list.
    if (!vm_brpt->find_breakpoint(bp->addr))
    {
        TRACE2("jvmti.break.intf", "Deinstrumenting native: "
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

static void process_native_breakpoint_event()
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    ti->vm_brpt->process_native_breakpoint();
}

bool jvmti_jit_breakpoint_handler(Registers *regs)
{
#if PLATFORM_POSIX && INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_INT3
    // Int3 exception address points to the instruction after it
    NativeCodePtr native_location = (NativeCodePtr)(((POINTER_SIZE_INT)regs->get_ip()) - 1);
#else
    NativeCodePtr native_location = (NativeCodePtr)regs->get_ip();
#endif

    TRACE2("jvmti.break", "BREAKPOINT occured: " << native_location);

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() || ti->getPhase() != JVMTI_PHASE_LIVE)
        return false;

    VMBreakPoints* vm_brpt = ti->vm_brpt;
    vm_brpt->lock();
    VMBreakPoint* bp = vm_brpt->find_breakpoint(native_location);
    if (NULL == bp)
    {
        vm_brpt->unlock();
        return false;
    }

    assert(!interpreter_enabled());

    // Now it is necessary to set up a transition to
    // process_native_breakpoint_event from the exception/signal
    // handler
    VM_thread *vm_thread = p_TLS_vmthread;
    // Copy original registers to TLS
    vm_thread->jvmti_saved_exception_registers = *regs;
#ifndef _EM64T_
    regs->eip = (POINTER_SIZE_INT)process_native_breakpoint_event;
#else
    regs->rip = (POINTER_SIZE_INT)process_native_breakpoint_event;
#endif

    // Breakpoints list lock is not released until it is unlocked
    // inside of process_native_breakpoint_event
    return true;
}

// Called when method compilation has completed
void jvmti_set_pending_breakpoints(Method *method)
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return;

    if( !method->get_pending_breakpoints() )
        return;

    VMBreakPoints* vm_brpt = ti->vm_brpt;
    LMAutoUnlock lock(vm_brpt->get_lock());

    if( !method->get_pending_breakpoints() )
        return;

    VMBreakPoint* bp = vm_brpt->find_first((jmethodID)method);
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

        set_jit_mode_breakpoint(vm_brpt, bp);
        locations[location_count++] = bp->location;

        method->remove_pending_breakpoint();
        bp = vm_brpt->find_next(bp, (jmethodID)method);
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
