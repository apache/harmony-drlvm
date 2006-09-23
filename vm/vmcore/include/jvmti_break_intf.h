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
 * @author Ilya Berezhniuk
 * @version $Revision$
 */
/**
 * @file jvmti_break_intf.h
 * @brief JVMTI native breakpoints API
 */
#if !defined(__JVMTI_BREAK_INTF_H__)
#define __JVMTI_BREAK_INTF_H__

#include "open/types.h"
#include "jni.h"
#include "lock_manager.h"
#include "jvmti_dasm.h"
#include "environment.h"

#define INSTRUMENTATION_BYTE_HLT 0xf4 // HLT instruction
#define INSTRUMENTATION_BYTE_CLI 0xfa // CLI instruction
#define INSTRUMENTATION_BYTE_INT3 0xcc // INT 3 instruction

#ifdef PLATFORM_NT
#define INSTRUMENTATION_BYTE INSTRUMENTATION_BYTE_CLI
#else
#define INSTRUMENTATION_BYTE INSTRUMENTATION_BYTE_INT3
#endif

// Callbacks are called for interfaces according to its priority
typedef enum {
    PRIORITY_SINGLE_STEP_BREAKPOINT = 0,
    PRIORITY_SIMPLE_BREAKPOINT,
    PRIORITY_NUMBER
} jvmti_BreakPriority;

class VMBreakInterface;

struct VMBreakPoint
{
    InstructionDisassembler* disasm;
    VMBreakPoint*            next;
    NativeCodePtr            addr;
    jlocation                location;
    jmethodID                method;
    jbyte                    saved_byte;
    bool                     is_interp;
    bool                     is_processed;
};

// Breakpoint reference
struct VMBreakPointRef
{
    VMBreakPoint*    brpt;
    void*            data;
    VMBreakPointRef* next;
};

// Pointer to interface callback function
typedef bool (*BPInterfaceCallBack)(VMBreakInterface* intf, VMBreakPointRef* bp_ref);

class VMBreakPoints
{
public:
    VMBreakPoints() : m_break(NULL)
    {
        for(unsigned index = 0; index < PRIORITY_NUMBER; index++ ) {
            m_intf[index] = NULL;
        }
    }

    ~VMBreakPoints();

    void lock() {m_lock._lock();}
    void unlock() {m_lock._unlock();}
    // Is used for LMAutoUnlock
    Lock_Manager* get_lock() {return &m_lock;}

    // Returns interface for breakpoint handling
    VMBreakInterface* new_intf(BPInterfaceCallBack callback,
        unsigned priority, bool is_interp);
    // Destroys interface and deletes all its breakpoints
    void release_intf(VMBreakInterface* intf);

    // Checks breakpoint before inserting
    bool check_insert_breakpoint(VMBreakPoint* bp);
    // Inserts breakpoint into global list and performs instrumentation
    bool insert_breakpoint(VMBreakPoint* brpt);
    // Removes breakpoint from global list and restores instrumented area
    bool remove_breakpoint(VMBreakPoint* brpt);

    // Search operations
    VMBreakPoint* find_breakpoint(jmethodID method, jlocation location);
    VMBreakPoint* find_breakpoint(NativeCodePtr addr);
    VMBreakPoint* find_other_breakpoint_with_same_addr(VMBreakPoint* bp);
    VMBreakPoint* find_next_breakpoint(VMBreakPoint* prev, NativeCodePtr addr);

    // Search breakpoints for given method
    bool has_breakpoint(jmethodID method);
    VMBreakPoint* find_first(jmethodID method);
    VMBreakPoint* find_next(VMBreakPoint* prev, jmethodID method);

    // Checks if given breakpoint is set by other interfaces
    VMBreakPointRef* find_other_reference(VMBreakInterface* intf,
        jmethodID method, jlocation location);
    VMBreakPointRef* find_other_reference(VMBreakInterface* intf,
        NativeCodePtr addr);
    VMBreakPointRef* find_other_reference(VMBreakInterface* intf,
        VMBreakPoint* brpt);

    // General callback functions
    void  process_native_breakpoint();
    jbyte process_interpreter_breakpoint(jmethodID method, jlocation location);

protected:
    void clear_breakpoints_processed_flags();
    VMBreakPoint* get_none_processed_breakpoint(NativeCodePtr addr);
    void set_breakpoint_processed( VMBreakPoint *bp, bool flag )
    {
        bp->is_processed = flag;
    }
    bool breakpoint_is_processed( VMBreakPoint *bp )
    {
        return bp->is_processed == true;
    }

    void clear_intfs_processed_flags();
    VMBreakInterface* get_none_processed_intf(unsigned priority);

private:
    VMBreakInterface* m_intf[PRIORITY_NUMBER];
    VMBreakPoint*     m_break;
    Lock_Manager m_lock;
};

class VMBreakInterface
{
    friend class VMBreakPoints;

public:
    void lock()     {VM_Global_State::loader_env->TI->vm_brpt->lock();}
    void unlock()   {VM_Global_State::loader_env->TI->vm_brpt->unlock();}
    Lock_Manager* get_lock()
                    {return VM_Global_State::loader_env->TI->vm_brpt->get_lock();}

    int get_priority() const { return m_priority; }

    // Iteration
    VMBreakPointRef* get_first() { return m_list; }
    VMBreakPointRef* get_next(VMBreakPointRef* prev)
    {
        assert(prev);
        return prev->next;
    }

    // Basic operations

    // 'data' must be allocated with JVMTI Allocate (or internal _allocate)
    // Users must not deallocate 'data', it will be deallocated by 'remove'
    VMBreakPointRef* add(jmethodID method, jlocation location, void* data);
    // To specify address explicitly
    VMBreakPointRef* add(jmethodID method, jlocation location,
                    NativeCodePtr addr, void* data);
    VMBreakPointRef* add(NativeCodePtr addr, void* data);

    void remove_all();
    bool remove(VMBreakPointRef* ref);

    VMBreakPointRef* find(jmethodID method, jlocation location);
    VMBreakPointRef* find(NativeCodePtr addr);
    VMBreakPointRef* find(VMBreakPoint* brpt);

protected:
    VMBreakInterface(BPInterfaceCallBack callback, unsigned priority, bool is_interp)
        : m_list(NULL), m_callback(callback), m_priority(priority),
          m_next(NULL), m_processed(false), m_is_interp(is_interp) {}
    ~VMBreakInterface() { remove_all(); }

    void set_processed(bool processed) { m_processed = processed; }
    bool is_processed() const { return (m_processed == true); }
    VMBreakInterface*   m_next;

private:
    VMBreakPointRef*    m_list;
    BPInterfaceCallBack m_callback;
    bool                m_is_interp;
    unsigned            m_priority;
    bool                m_processed;
};

// Callback function for native breakpoint processing
bool jvmti_jit_breakpoint_handler(Registers *regs);

// Callback function for interpreter breakpoint processing
VMEXPORT jbyte
jvmti_process_interpreter_breakpoint_event(jmethodID method, jlocation location);

// Callback for JIT method compile
void jvmti_set_pending_breakpoints(Method *method);

#endif  // __JVMTI_BREAK_INTF_H__
