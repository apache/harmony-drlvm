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
 * @author Pavel Rebriy
 * @version $Revision: $
 */

#include "jvmti.h"
#include "Class.h"
#include "cxxlog.h"
#include "jvmti_utils.h"
#include "jvmti_internal.h"
#include "jit_intf_cpp.h"
#include "stack_iterator.h"
#include "interpreter.h"
#include "open/bytecodes.h"

static JNIEnv * jvmti_test_jenv = jni_native_intf;

static inline short
jvmti_GetHalfWordValue( const unsigned char *bytecode,
                        unsigned location)
{
    short result = (short)( (bytecode[location] << 8)|(bytecode[location + 1]) );
    return result;
} // jvmti_GetHalfWordValue

static inline int
jvmti_GetWordValue( const unsigned char *bytecode,
                    unsigned location)
{
    int result = (int)( (bytecode[location    ] << 24)|(bytecode[location + 1] << 16)
                       |(bytecode[location + 2] << 8) |(bytecode[location + 3]      ) );
    return result;
} // jvmti_GetWordValue

static unsigned
jvmti_GetNextBytecodeLocation( Method *method,
                               unsigned location)
{
    assert( location < method->get_byte_code_size() );
    const unsigned char *bytecode = method->get_byte_code_addr();
    bool is_wide = false;
    do {
        switch( bytecode[location] )
        {
        case OPCODE_WIDE:           /* 0xc4 */
            assert( !is_wide );
            location++;
            is_wide = true;
            continue;

        case OPCODE_TABLESWITCH:    /* 0xaa + pad + s4 * (3 + N) */
            assert( !is_wide );
            location = (location + 4)&(~0x3U);
            {
                int low = jvmti_GetWordValue( bytecode, location + 4 );
                int high = jvmti_GetWordValue( bytecode, location + 8 );
                return location + 4 * (high - low + 4);
            }

        case OPCODE_LOOKUPSWITCH:   /* 0xab + pad + s4 * 2 * (N + 1) */
            assert( !is_wide );
            location = (location + 4)&(~0x3U);
            {
                int number = jvmti_GetWordValue( bytecode, location + 4 ) + 1;
                return location + 8 * number;
            }

        case OPCODE_IINC:           /* 0x84 + u1|u2 + s1|s2 */
            if( is_wide ) {
                return location + 5;
            } else {
                return location + 3;
            }

        case OPCODE_GOTO_W:         /* 0xc8 + s4 */
        case OPCODE_JSR_W:          /* 0xc9 + s4 */
        case OPCODE_INVOKEINTERFACE:/* 0xb9 + u2 + u1 + u1 */
            assert( !is_wide );
            return location + 5;

        case OPCODE_MULTIANEWARRAY: /* 0xc5 + u2 + u1 */
            assert( !is_wide );
            return location + 4;

        case OPCODE_ILOAD:          /* 0x15 + u1|u2 */
        case OPCODE_LLOAD:          /* 0x16 + u1|u2 */
        case OPCODE_FLOAD:          /* 0x17 + u1|u2 */
        case OPCODE_DLOAD:          /* 0x18 + u1|u2 */
        case OPCODE_ALOAD:          /* 0x19 + u1|u2 */
        case OPCODE_ISTORE:         /* 0x36 + u1|u2 */
        case OPCODE_LSTORE:         /* 0x37 + u1|u2 */
        case OPCODE_FSTORE:         /* 0x38 + u1|u2 */
        case OPCODE_DSTORE:         /* 0x39 + u1|u2 */
        case OPCODE_ASTORE:         /* 0x3a + u1|u2 */
        case OPCODE_RET:            /* 0xa9 + u1|u2  */
            if( is_wide ) {
                return location + 3;
            } else {
                return location + 2;
            }

        case OPCODE_SIPUSH:         /* 0x11 + s2 */
        case OPCODE_LDC_W:          /* 0x13 + u2 */
        case OPCODE_LDC2_W:         /* 0x14 + u2 */
        case OPCODE_IFEQ:           /* 0x99 + s2 */
        case OPCODE_IFNE:           /* 0x9a + s2 */
        case OPCODE_IFLT:           /* 0x9b + s2 */
        case OPCODE_IFGE:           /* 0x9c + s2 */
        case OPCODE_IFGT:           /* 0x9d + s2 */
        case OPCODE_IFLE:           /* 0x9e + s2 */
        case OPCODE_IF_ICMPEQ:      /* 0x9f + s2 */
        case OPCODE_IF_ICMPNE:      /* 0xa0 + s2 */
        case OPCODE_IF_ICMPLT:      /* 0xa1 + s2 */
        case OPCODE_IF_ICMPGE:      /* 0xa2 + s2 */
        case OPCODE_IF_ICMPGT:      /* 0xa3 + s2 */
        case OPCODE_IF_ICMPLE:      /* 0xa4 + s2 */
        case OPCODE_IF_ACMPEQ:      /* 0xa5 + s2 */
        case OPCODE_IF_ACMPNE:      /* 0xa6 + s2 */
        case OPCODE_GOTO:           /* 0xa7 + s2 */
        case OPCODE_GETSTATIC:      /* 0xb2 + u2 */
        case OPCODE_PUTSTATIC:      /* 0xb3 + u2 */
        case OPCODE_GETFIELD:       /* 0xb4 + u2 */
        case OPCODE_PUTFIELD:       /* 0xb5 + u2 */
        case OPCODE_INVOKEVIRTUAL:  /* 0xb6 + u2 */
        case OPCODE_INVOKESPECIAL:  /* 0xb7 + u2 */
        case OPCODE_JSR:            /* 0xa8 + s2 */
        case OPCODE_INVOKESTATIC:   /* 0xb8 + u2 */
        case OPCODE_NEW:            /* 0xbb + u2 */
        case OPCODE_ANEWARRAY:      /* 0xbd + u2 */
        case OPCODE_CHECKCAST:      /* 0xc0 + u2 */
        case OPCODE_INSTANCEOF:     /* 0xc1 + u2 */
        case OPCODE_IFNULL:         /* 0xc6 + s2 */
        case OPCODE_IFNONNULL:      /* 0xc7 + s2 */
            assert( !is_wide );
            return location + 3;

        case OPCODE_BIPUSH:         /* 0x10 + s1 */
        case OPCODE_LDC:            /* 0x12 + u1 */
        case OPCODE_NEWARRAY:       /* 0xbc + u1 */
            assert( !is_wide );
            return location + 2;

        default:
            assert( !is_wide );
            assert( bytecode[location] < OPCODE_COUNT );
            assert( bytecode[location] != _OPCODE_UNDEFINED );
            return location + 1;
        }
        break;
    } while( true );
    return 0;
} // jvmti_GetNextBytecodeLocation

static unsigned
jvmti_GetNextBytecodeAfterInvoke( Method *method,
                                  unsigned location)
{
    const unsigned char UNREF *bytecode = method->get_byte_code_addr();
    assert( bytecode[location] >= OPCODE_INVOKEVIRTUAL
           && bytecode[location] <= OPCODE_INVOKEINTERFACE );
    return jvmti_GetNextBytecodeLocation(method, location);
} // jvmti_GetNextBytecodeAfterInvoke

void
jvmti_SingleStepLocation( VM_thread* thread,
                          Method *method,
                          unsigned bytecode_index,
                          jvmti_StepLocation **next_step,
                          unsigned *count)
{
    assert(next_step);
    assert(count);
    ASSERT_NO_INTERPRETER;

    // get method bytecode array and code length
    const unsigned char *bytecode = method->get_byte_code_addr();
    unsigned len = method->get_byte_code_size();
    unsigned location = bytecode_index;
    assert(location < len);

    // initialize step location count
    *count = 0;

    // parse bytecode
    jvmtiError error;
    bool is_wide = false;
    int offset;
    do {

        switch( bytecode[location] )
        {
        // wide instruction
        case OPCODE_WIDE:           /* 0xc4 */
            assert( !is_wide );
            location++;
            is_wide = true;
            continue;

        // if instructions
        case OPCODE_IFEQ:           /* 0x99 + s2 */
        case OPCODE_IFNE:           /* 0x9a + s2 */
        case OPCODE_IFLT:           /* 0x9b + s2 */
        case OPCODE_IFGE:           /* 0x9c + s2 */
        case OPCODE_IFGT:           /* 0x9d + s2 */
        case OPCODE_IFLE:           /* 0x9e + s2 */

        case OPCODE_IF_ICMPEQ:      /* 0x9f + s2 */
        case OPCODE_IF_ICMPNE:      /* 0xa0 + s2 */
        case OPCODE_IF_ICMPLT:      /* 0xa1 + s2 */
        case OPCODE_IF_ICMPGE:      /* 0xa2 + s2 */
        case OPCODE_IF_ICMPGT:      /* 0xa3 + s2 */
        case OPCODE_IF_ICMPLE:      /* 0xa4 + s2 */

        case OPCODE_IF_ACMPEQ:      /* 0xa5 + s2 */
        case OPCODE_IF_ACMPNE:      /* 0xa6 + s2 */

        case OPCODE_IFNULL:         /* 0xc6 + s2 */
        case OPCODE_IFNONNULL:      /* 0xc7 + s2 */
            assert( !is_wide );
            offset = (int)location + jvmti_GetHalfWordValue( bytecode, location + 1 );
            location += 3;
            *count = 2;
            error = _allocate( sizeof(jvmti_StepLocation) * 2, (unsigned char**)next_step );
            assert( error == JVMTI_ERROR_NONE );
            (*next_step)[0].method = method;
            (*next_step)[0].location = location;
            (*next_step)[1].method = method;
            (*next_step)[1].location = offset;
            break;

        // goto instructions
        case OPCODE_GOTO:           /* 0xa7 + s2 */
        case OPCODE_JSR:            /* 0xa8 + s2 */
            assert( !is_wide );
            offset = (int)location + jvmti_GetHalfWordValue( bytecode, location + 1 );
            *count = 1;
            error = _allocate( sizeof(jvmti_StepLocation), (unsigned char**)next_step );
            assert( error == JVMTI_ERROR_NONE );
            (*next_step)->method = method;
            (*next_step)->location = offset;
            break;
        case OPCODE_GOTO_W:         /* 0xc8 + s4 */
        case OPCODE_JSR_W:          /* 0xc9 + s4 */
            assert( !is_wide );
            offset = (int)location + jvmti_GetWordValue( bytecode, location + 1 );
            *count = 1;
            error = _allocate( sizeof(jvmti_StepLocation), (unsigned char**)next_step );
            assert( error == JVMTI_ERROR_NONE );
            (*next_step)->method = method;
            (*next_step)->location = offset;
            break;

        // tableswitch instruction
        case OPCODE_TABLESWITCH:    /* 0xaa + pad + s4 * (3 + N) */
            assert( !is_wide );
            location = (location + 4)&(~0x3U);
            {
                int low = jvmti_GetWordValue( bytecode, location + 4 );
                int high = jvmti_GetWordValue( bytecode, location + 8 );
                int number = high - low + 2;

                *count = number;
                error = _allocate( sizeof(jvmti_StepLocation) * number, (unsigned char**)next_step );
                assert( error == JVMTI_ERROR_NONE );
                (*next_step)[0].method = method;
                (*next_step)[0].location = (int)bytecode_index
                    + jvmti_GetWordValue( bytecode, location );
                location += 12;
                for( int index = 1; index < number; index++, location += 4 ) {
                    (*next_step)[index].method = method;
                    (*next_step)[index].location = (int)bytecode_index
                        + jvmti_GetWordValue( bytecode, location );
                }
            }
            break;

        // lookupswitch instruction
        case OPCODE_LOOKUPSWITCH:   /* 0xab + pad + s4 * 2 * (N + 1) */
            assert( !is_wide );
            location = (location + 4)&(~0x3U);
            {
                int number = jvmti_GetWordValue( bytecode, location + 4 ) + 1;

                *count = number;
                error = _allocate( sizeof(jvmti_StepLocation) * number, (unsigned char**)next_step );
                assert( error == JVMTI_ERROR_NONE );
                (*next_step)[0].method = method;
                (*next_step)[0].location = (int)bytecode_index
                    + jvmti_GetWordValue( bytecode, location );
                location += 12;
                for( int index = 1; index < number; index++, location += 8 ) {
                    (*next_step)[index].method = method;
                    (*next_step)[index].location = (int)
                        + jvmti_GetWordValue( bytecode, location );
                }
            }
            break;

        // athrow instruction
        case OPCODE_ATHROW:         /* 0xbf */
            assert( !is_wide );
            *count = 0;
            *next_step = NULL;
            break;

        // return instructions
        case OPCODE_IRETURN:        /* 0xac */
        case OPCODE_LRETURN:        /* 0xad */
        case OPCODE_FRETURN:        /* 0xae */
        case OPCODE_DRETURN:        /* 0xaf */
        case OPCODE_ARETURN:        /* 0xb0 */
        case OPCODE_RETURN:         /* 0xb1 */
            assert( !is_wide );
            {
                // create stack iterator, current stack frame should be native
                StackIterator *si = si_create_from_native(thread);
                assert(si_is_native(si));
                // get previous stack frame, it should be java frame
                si_goto_previous(si);
                assert(!si_is_native(si));
                // get previous stack frame
                si_goto_previous(si);
                if (!si_is_native(si)) {
                    // stack frame is java frame, get frame method and location
                    uint16 bc = 0;
                    CodeChunkInfo *cci = si_get_code_chunk_info(si);
                    Method *func = cci->get_method();
                    NativeCodePtr ip = si_get_ip(si);
                    JIT *jit = cci->get_jit();
                    OpenExeJpdaError UNREF result =
                                jit->get_bc_location_for_native(
                                    func, ip, &bc);
                    assert(result == EXE_ERROR_NONE);
                    si_free(si);

                    // set step location structure
                    *count = 1;
                    error = _allocate( sizeof(jvmti_StepLocation), (unsigned char**)next_step );
                    assert( error == JVMTI_ERROR_NONE );
                    (*next_step)->method = func;
                    // gregory - IP in stack iterator points to a
                    // bytecode next after the one which caused call
                    // of the method. So next location is the BC which
                    // IP points to.
                    (*next_step)->location = bc;
                }
            }
            break;

        // invokestatic instruction
        case OPCODE_INVOKESTATIC:   /* 0xb8 + u2 */
            assert( !is_wide );
            {
                unsigned short index = jvmti_GetHalfWordValue( bytecode, location + 1 );
                Class *klass = method_get_class( method );
                assert( cp_is_resolved(klass->const_pool, index) );

                *count = 1;
                error = _allocate( sizeof(jvmti_StepLocation), (unsigned char**)next_step );
                assert( error == JVMTI_ERROR_NONE );
                if( method_is_native( klass->const_pool[index].CONSTANT_ref.method ) ) {
                    (*next_step)->method = method;
                    (*next_step)->location = 
                        jvmti_GetNextBytecodeAfterInvoke( method, bytecode_index );
                } else {
                    (*next_step)->method = klass->const_pool[index].CONSTANT_ref.method;
                    (*next_step)->location = 0;
                }
            }
            break;

        case OPCODE_MULTIANEWARRAY: /* 0xc5 + u2 + u1 */
            assert( !is_wide );
            location++;

        case OPCODE_IINC:           /* 0x84 + u1|u2 + s1|s2 */
            if( is_wide ) {
                location += 2;
                is_wide = false;
            }

        case OPCODE_SIPUSH:         /* 0x11 + s2 */
        case OPCODE_LDC_W:          /* 0x13 + u2 */
        case OPCODE_LDC2_W:         /* 0x14 + u2 */
        case OPCODE_GETSTATIC:      /* 0xb2 + u2 */
        case OPCODE_PUTSTATIC:      /* 0xb3 + u2 */
        case OPCODE_GETFIELD:       /* 0xb4 + u2 */
        case OPCODE_PUTFIELD:       /* 0xb5 + u2 */
        case OPCODE_NEW:            /* 0xbb + u2 */
        case OPCODE_ANEWARRAY:      /* 0xbd + u2 */
        case OPCODE_CHECKCAST:      /* 0xc0 + u2 */
        case OPCODE_INSTANCEOF:     /* 0xc1 + u2 */
            assert( !is_wide );
            location++;

        case OPCODE_ILOAD:          /* 0x15 + u1|u2 */
        case OPCODE_LLOAD:          /* 0x16 + u1|u2 */
        case OPCODE_FLOAD:          /* 0x17 + u1|u2 */
        case OPCODE_DLOAD:          /* 0x18 + u1|u2 */
        case OPCODE_ALOAD:          /* 0x19 + u1|u2 */
        case OPCODE_ISTORE:         /* 0x36 + u1|u2 */
        case OPCODE_LSTORE:         /* 0x37 + u1|u2 */
        case OPCODE_FSTORE:         /* 0x38 + u1|u2 */
        case OPCODE_DSTORE:         /* 0x39 + u1|u2 */
        case OPCODE_ASTORE:         /* 0x3a + u1|u2 */
            if( is_wide ) {
                location++;
                is_wide = false;
            }

        case OPCODE_BIPUSH:         /* 0x10 + s1 */
        case OPCODE_LDC:            /* 0x12 + u1 */
        case OPCODE_NEWARRAY:       /* 0xbc + u1 */
            assert( !is_wide );
            location++;

        default:
            assert( !is_wide );
            assert( bytecode[bytecode_index] < OPCODE_COUNT );
            assert( bytecode[bytecode_index] != _OPCODE_UNDEFINED );

            location++;
            *count = 1;
            error = _allocate( sizeof(jvmti_StepLocation), (unsigned char**)next_step );
            assert( error == JVMTI_ERROR_NONE );
            (*next_step)->method = method;
            (*next_step)->location = location;
            break;

        // ret instruction
        case OPCODE_RET:            /* 0xa9 + u1|u2  */
            // FIXME - need to obtain return address from stack.
            break;

        // invokes instruction with reference in stack 
        case OPCODE_INVOKEVIRTUAL:  /* 0xb6 + u2 */
        case OPCODE_INVOKESPECIAL:  /* 0xb7 + u2 */
        case OPCODE_INVOKEINTERFACE:/* 0xb9 + u2 + u1 + u1 */
            assert( !is_wide );
            // FIXME - need to get reference from stack to determine next method
            break;
        }
        break;
    } while( true );

    return;
} // jvmti_SingleStepLocation

jvmtiError jvmti_set_single_step_breakpoints(DebugUtilsTI *ti, VM_thread *vm_thread,
    jvmti_StepLocation *locations, unsigned locations_number)
{
    // Function is always executed under global TI breakpoints lock
    BreakPoint **thread_breakpoints;
    jvmtiError errorCode = _allocate(sizeof(BreakPoint),
        (unsigned char **)&thread_breakpoints);
    if (JVMTI_ERROR_NONE != errorCode)
        return errorCode;

    for (unsigned iii = 0; iii < locations_number; iii++)
    {
        BreakPoint *bp;
        errorCode = _allocate(sizeof(BreakPoint), (unsigned char **)&bp);
        if (JVMTI_ERROR_NONE != errorCode)
            return errorCode;

        bp->method = (jmethodID)locations[iii].method;
        bp->location = locations[iii].location;
        bp->env = NULL;
        bp->disasm = NULL;

        errorCode = jvmti_set_breakpoint_for_jit(ti, bp);
        if (JVMTI_ERROR_NONE != errorCode)
            return errorCode;

        thread_breakpoints[iii] = bp;
    }

    JVMTISingleStepState *ss_state = vm_thread->ss_state;
    ss_state->predicted_breakpoints = thread_breakpoints;
    ss_state->predicted_bp_count = locations_number;

    return JVMTI_ERROR_NONE;
}

void jvmti_remove_single_step_breakpoints(DebugUtilsTI *ti, VM_thread *vm_thread)
{
    // Function is always executed under global TI breakpoints lock
    JVMTISingleStepState *ss_state = vm_thread->ss_state;

    for (unsigned iii = 0; iii < ss_state->predicted_bp_count; iii++)
    {
        BreakPoint *bp = ss_state->predicted_breakpoints[iii];
        jvmti_remove_breakpoint_for_jit(ti, bp);
    }

    _deallocate((unsigned char *)ss_state->predicted_breakpoints);
    ss_state->predicted_bp_count = 0;
}

jvmtiError jvmti_get_next_bytecodes_up_stack_from_native(VM_thread *thread,
    jvmti_StepLocation **next_step,
    unsigned *count)
{
    StackIterator *si = si_create_from_native(thread);

    // Find first Java frame in the thread stack
    while (!si_is_past_end(si))
        if (!si_is_native(si))
            break;

    *count = 0;

    if (!si_is_past_end(si))
    {
        Method *m = si_get_method(si);
        assert(m);

        CodeChunkInfo *cci = si_get_code_chunk_info(si);
        JIT *jit = cci->get_jit();
        // IP address in stack iterator points to the next bytecode after
        // call
        NativeCodePtr ip = si_get_ip(si);
        uint16 bc;

        OpenExeJpdaError UNREF result =
            jit->get_bc_location_for_native(m, ip, &bc);
        assert(result == EXE_ERROR_NONE);

        jvmtiError errorCode = _allocate(sizeof(jvmti_StepLocation),
            (unsigned char **)next_step);

        if (JVMTI_ERROR_NONE != errorCode)
        {
            si_free(si);
            return errorCode;
        }
        (*next_step)->method = (Method *)m;
        (*next_step)->location = bc;
    }

    si_free(si);
    return JVMTI_ERROR_NONE;
}

jvmtiError DebugUtilsTI::jvmti_single_step_start(void)
{
    assert(hythread_is_suspend_enabled());
    LMAutoUnlock lock(&brkpntlst_lock);

    hythread_iterator_t threads_iterator;

    // Suspend all threads except current
    IDATA tm_ret = hythread_suspend_all(&threads_iterator, NULL);
    if (TM_ERROR_NONE != tm_ret)
        return JVMTI_ERROR_INTERNAL;

    hythread_t ht;

    // Set single step in all threads
    while ((ht = hythread_iterator_next(&threads_iterator)) != NULL)
    {
        VM_thread *vm_thread = get_vm_thread(ht);

        // Init single step state for the thread
        jvmtiError errorCode = _allocate(sizeof(JVMTISingleStepState),
            (unsigned char **)&vm_thread->ss_state);

        if (JVMTI_ERROR_NONE != errorCode)
        {
            hythread_resume_all(NULL);
            return errorCode;
        }

        vm_thread->ss_state->predicted_breakpoints = NULL;
        vm_thread->ss_state->predicted_bp_count = 0;
        vm_thread->ss_state->enabled = true;

        vm_thread->ss_state->enabled = true;

        jvmti_StepLocation *locations;
        unsigned locations_number;

        errorCode = jvmti_get_next_bytecodes_up_stack_from_native(
            vm_thread, &locations, &locations_number);

        if (JVMTI_ERROR_NONE != errorCode)
        {
            hythread_resume_all(NULL);
            return errorCode;
        }

        errorCode = jvmti_set_single_step_breakpoints(this, vm_thread, locations,
                locations_number);

        if (JVMTI_ERROR_NONE != errorCode)
        {
            hythread_resume_all(NULL);
            return errorCode;
        }
    }
    
    single_step_enabled = true;

    tm_ret = hythread_resume_all(NULL);
    if (TM_ERROR_NONE != tm_ret)
        return JVMTI_ERROR_INTERNAL;

    return JVMTI_ERROR_NONE;
}

jvmtiError DebugUtilsTI::jvmti_single_step_stop(void)
{
    assert(hythread_is_suspend_enabled());
    LMAutoUnlock lock(&brkpntlst_lock);

    hythread_iterator_t threads_iterator;

    // Suspend all threads except current
    IDATA tm_ret = hythread_suspend_all(&threads_iterator, NULL);
    if (TM_ERROR_NONE != tm_ret)
        return JVMTI_ERROR_INTERNAL;

    hythread_t ht;

    // Clear single step in all threads
    while ((ht = hythread_iterator_next(&threads_iterator)) != NULL)
    {
        VM_thread *vm_thread = get_vm_thread(ht);
        jvmti_remove_single_step_breakpoints(this, vm_thread);
        vm_thread->ss_state->enabled = false;
        _deallocate((unsigned char *)vm_thread->ss_state);
    }

    single_step_enabled = false;

    tm_ret = hythread_resume_all(NULL);
    if (TM_ERROR_NONE != tm_ret)
        return JVMTI_ERROR_INTERNAL;

    return JVMTI_ERROR_NONE;
}
