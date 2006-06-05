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
 * @author Ivan Volosyuk
 * @version $Revision: 1.27.12.3.4.3 $
 */  
#include "interpreter.h"
#include "interpreter_exports.h"
#include "interpreter_imports.h"
#include "interp_defs.h"
#include "interp_native.h"
#include "port_malloc.h"
#include "open/thread.h"
#include "thread_generic.h"

static jint skip_old_frames(VM_thread *thread)
{
    if (NULL == getLastStackFrame(thread))
        return 0;

    StackFrame* first_frame = (StackFrame*)(thread->firstFrame);

    if (first_frame)
    {
        Class *clss = method_get_class(first_frame->method);
        assert(clss);

        if (strcmp(method_get_name(first_frame->method), "run") == 0 &&
            strcmp(class_get_name(clss), "java/lang/VMStart$MainThread") == 0)
        {
            return 3;
        }
    }

    return 0;
}

jvmtiError
interpreter_ti_get_frame_count(jvmtiEnv*, VM_thread *thread, jint* count_ptr) {
    StackFrame *frame = getLastStackFrame(thread);
    *count_ptr = 0;

    while(frame != 0) {
        (*count_ptr)++;
        frame = frame->prev;
    }

    (*count_ptr) -= skip_old_frames(thread);

    return JVMTI_ERROR_NONE;
}

jvmtiError
interpreter_ti_getLocalCommon(
        jvmtiEnv*,
        VM_thread *thread,
        jint depth,
        jint slot,
        StackFrame **framePtr) {
    StackFrame *frame = getLastStackFrame(thread);
    while(depth && frame != 0) {
        depth--;
        frame = frame->prev;
    }

    // check error condition: JVMTI_ERROR_NO_MORE_FRAMES
    if (depth != 0) {
        return JVMTI_ERROR_NO_MORE_FRAMES;
    }

    // check error condition: JVMTI_ERROR_OPAQUE_FRAME
    if (frame->ip == 0) {
        return JVMTI_ERROR_OPAQUE_FRAME;
    }

    unsigned uslot = slot;

    // check error condition: JVMTI_ERROR_INVALID_SLOT
    if (uslot >= frame->locals.getLocalsNumber()) {
        return JVMTI_ERROR_INVALID_SLOT;
    }

    *framePtr = frame;
    return JVMTI_ERROR_NONE;
}

jvmtiError
interpreter_ti_getLocal64(
        jvmtiEnv* env,
        VM_thread *thread,
        jint depth,
        jint slot,
        jlong* value_ptr) {
    StackFrame *frame;

    // check error condition: JVMTI_ERROR_NO_MORE_FRAMES
    // check error condition: JVMTI_ERROR_OPAQUE_FRAME
    // check error condition: JVMTI_ERROR_INVALID_SLOT
    jvmtiError err = interpreter_ti_getLocalCommon(env, thread, depth, slot, &frame);
    if (err != JVMTI_ERROR_NONE) return err;

    // TODO: check error condition: JVMTI_ERROR_TYPE_MISMATCH
    // partial check error condition: JVMTI_ERROR_TYPE_MISMATCH
    if (frame->locals.ref(slot) != 0 && frame->locals.ref(slot+1) != 0) {
        return JVMTI_ERROR_TYPE_MISMATCH;
    }

    *value_ptr = frame->locals.getLong(slot).i64;
    return JVMTI_ERROR_NONE;
}

jvmtiError
interpreter_ti_getLocal32(
        jvmtiEnv* env,
        VM_thread *thread,
        jint depth,
        jint slot,
        jint* value_ptr) {
    StackFrame *frame;

    // check error condition: JVMTI_ERROR_NO_MORE_FRAMES
    // check error condition: JVMTI_ERROR_OPAQUE_FRAME
    // check error condition: JVMTI_ERROR_INVALID_SLOT
    jvmtiError err = interpreter_ti_getLocalCommon(env, thread, depth, slot, &frame);
    if (err != JVMTI_ERROR_NONE) return err;

    // TODO: check error condition: JVMTI_ERROR_TYPE_MISMATCH
    // partial check error condition: JVMTI_ERROR_TYPE_MISMATCH
    if (frame->locals.ref(slot) != 0) {
        return JVMTI_ERROR_TYPE_MISMATCH;
    }

    *value_ptr = frame->locals(slot).i;
    return JVMTI_ERROR_NONE;
}

jvmtiError
interpreter_ti_getObject(
        jvmtiEnv* env,
        VM_thread *thread,
        jint depth,
        jint slot,
        jobject* value_ptr)
{
    StackFrame *frame;

    // check error condition: JVMTI_ERROR_NULL_POINTER
    if( value_ptr == NULL )
        return JVMTI_ERROR_NULL_POINTER;

    // check error condition: JVMTI_ERROR_NO_MORE_FRAMES
    // check error condition: JVMTI_ERROR_OPAQUE_FRAME
    // check error condition: JVMTI_ERROR_INVALID_SLOT
    jvmtiError err = interpreter_ti_getLocalCommon(env, thread, depth, slot, &frame);
    if (err != JVMTI_ERROR_NONE)
        return err;

    // TODO: check error condition: JVMTI_ERROR_TYPE_MISMATCH
    // partial check error condition: JVMTI_ERROR_TYPE_MISMATCH
    if (frame->locals.ref(slot) == 0) {
        return JVMTI_ERROR_TYPE_MISMATCH;
    }

    assert(tmn_is_suspend_enabled());
    tmn_suspend_disable();
    ManagedObject *obj = UNCOMPRESS_REF(frame->locals(slot).cr);
    if (NULL == obj) {
        *value_ptr = NULL;
    } else {
        ObjectHandle handle = oh_allocate_local_handle();
        handle->object = obj;
        *value_ptr = (jobject) handle;
    }
    tmn_suspend_enable();
    return JVMTI_ERROR_NONE;
}

jvmtiError
interpreter_ti_setLocal64(
        jvmtiEnv* env,
        VM_thread *thread,
        jint depth,
        jint slot,
        jlong value) {
    StackFrame *frame;

    // check error condition: JVMTI_ERROR_NO_MORE_FRAMES
    // check error condition: JVMTI_ERROR_OPAQUE_FRAME
    // check error condition: JVMTI_ERROR_INVALID_SLOT
    jvmtiError err = interpreter_ti_getLocalCommon(env, thread, depth, slot, &frame);
    if (err != JVMTI_ERROR_NONE) return err;

    // TODO: check error condition: JVMTI_ERROR_TYPE_MISMATCH
    // partial check error condition: JVMTI_ERROR_TYPE_MISMATCH
    if (frame->locals.ref(slot) != 0 && frame->locals.ref(slot+1) != 0) {
        return JVMTI_ERROR_TYPE_MISMATCH;
    }
 
    Value2 v;
    v.i64 = value;
    frame->locals.setLong(slot, v);
    return JVMTI_ERROR_NONE;
}

jvmtiError
interpreter_ti_setLocal32(
        jvmtiEnv* env,
        VM_thread *thread,
        jint depth,
        jint slot,
        jint value) {
    StackFrame *frame;

    // check error condition: JVMTI_ERROR_NO_MORE_FRAMES
    // check error condition: JVMTI_ERROR_OPAQUE_FRAME
    // check error condition: JVMTI_ERROR_INVALID_SLOT
    jvmtiError err = interpreter_ti_getLocalCommon(env, thread, depth, slot, &frame);
    if (err != JVMTI_ERROR_NONE) return err;

    // TODO: check error condition: JVMTI_ERROR_TYPE_MISMATCH
    // partial check error condition: JVMTI_ERROR_TYPE_MISMATCH
    if (frame->locals.ref(slot) != 0) {
        return JVMTI_ERROR_TYPE_MISMATCH;
    }

    frame->locals(slot).i = value;
    return JVMTI_ERROR_NONE;
}

jvmtiError
interpreter_ti_setObject(
        jvmtiEnv* env,
        VM_thread *thread,
        jint depth,
        jint slot,
        jobject value) {
    StackFrame *frame;

    // check error condition: JVMTI_ERROR_NO_MORE_FRAMES
    // check error condition: JVMTI_ERROR_OPAQUE_FRAME
    // check error condition: JVMTI_ERROR_INVALID_SLOT
    jvmtiError err = interpreter_ti_getLocalCommon(env, thread, depth, slot, &frame);
    if (err != JVMTI_ERROR_NONE) return err;

    // TODO: check error condition: JVMTI_ERROR_TYPE_MISMATCH
    // partial check error condition: JVMTI_ERROR_TYPE_MISMATCH
    if (frame->locals.ref(slot) == 0) {
        return JVMTI_ERROR_TYPE_MISMATCH;
    }

    ObjectHandle handle = (ObjectHandle) value;
    frame->locals(slot).cr = COMPRESS_REF(handle->object);
    return JVMTI_ERROR_NONE;
}

/*
 *  Looks like no way to get JNIEnv from jvmtiEnv, strange.
 */
static JNIEnv_Internal * UNUSED jvmti_test_jenv = get_jni_native_intf();

jvmtiError
interpreter_ti_getStackTrace(
        jvmtiEnv * UNREF env,
        VM_thread *thread,
        jint start_depth,
        jint max_frame_count,
        jvmtiFrameInfo *frame_buffer,
        jint *count_ptr) {
    jint start;
    StackFrame *frame = getLastStackFrame(thread);

    jint depth;
    interpreter_ti_get_frame_count(env, thread, &depth);

    if (start_depth < 0)
    {
        start = depth + start_depth;
        if (start < 0)
            return JVMTI_ERROR_ILLEGAL_ARGUMENT;
    }
    else
        start = start_depth;

    // skip start_depth of frames
    while (start) {
        if (frame == NULL)
            return JVMTI_ERROR_ILLEGAL_ARGUMENT;

        frame = frame->prev;
        start--;
    }

    jint count = 0;
    while (frame != NULL && count < max_frame_count &&
           count < depth - start)
    {
        Method *m = frame->method;
        frame_buffer[count].method = (jmethodID)m;

        if (m->is_native())
            frame_buffer[count].location = -1;
        else
            frame_buffer[count].location = (jlocation)((uint8*)frame->ip -
                    (uint8*)m->get_byte_code_addr());

        frame = frame->prev;
        count++;
    }

    *count_ptr = count;
    return JVMTI_ERROR_NONE;
}

jvmtiError
interpreter_ti_getFrameLocation(
        jvmtiEnv * UNREF env, VM_thread *thread, jint depth,
        jmethodID *method_ptr, jlocation *location_ptr) {
    StackFrame *frame = getLastStackFrame(thread);

    // skip depth of frames
    while (depth) {
        if (frame == NULL) {
            return JVMTI_ERROR_NO_MORE_FRAMES;
        }
        frame = frame->prev;
        depth--;
    }

    Method *m = frame->method;
    *method_ptr = (jmethodID) m;
    if (m->is_native()) {
        *location_ptr = -1;
    } else {
        *location_ptr = (jlocation)((uint8*)frame->ip -
                (uint8*)frame->method->get_byte_code_addr());
    }
    return JVMTI_ERROR_NONE;
}

uint8
Opcode_BREAKPOINT(StackFrame& frame) {
    Method *m = frame.method;
    jlocation l = frame.ip - (uint8*)m->get_byte_code_addr();
    return (uint8) (POINTER_SIZE_INT) jvmti_process_breakpoint_event((jmethodID)m, l);
}

void* interpreter_ti_set_breakpoint(jmethodID method, jlocation location) {
    Method *m = (Method*) method;
    uint8 *bytecodes = (uint8*) m->get_byte_code_addr();
    uint8 b = bytecodes[location];
    bytecodes[location] = OPCODE_BREAKPOINT;
    return (void*) (POINTER_SIZE_INT) b;
}

void interpreter_ti_clear_breakpoint(jmethodID method, jlocation location, void* id) {
    Method *m = (Method*) method;
    uint8 *bytecodes = (uint8*) m->get_byte_code_addr();
    bytecodes[location] = (uint8) (POINTER_SIZE_INT) id;
}

int interpreter_ti_notification_mode = 0;

void interpreter_ti_set_notification_mode(jvmtiEvent event_type, bool UNREF enable) {
    int new_mask = 0;

    switch (event_type) {
        case JVMTI_EVENT_METHOD_ENTRY: new_mask = INTERPRETER_TI_METHOD_ENTRY_EVENT; break;
        case JVMTI_EVENT_METHOD_EXIT: new_mask = INTERPRETER_TI_METHOD_EXIT_EVENT; break;
        case JVMTI_EVENT_SINGLE_STEP: new_mask = INTERPRETER_TI_SINGLE_STEP_EVENT; break;
        default: break;
    }

    if (enable) interpreter_ti_notification_mode |= new_mask;
    else interpreter_ti_notification_mode &= ~new_mask;
}

void method_entry_callback(Method *method) {
#if 0
    fprintf(stderr, "enter%s: %s %s%s\n",
            method->is_native() ? " <native>": "",
            method->get_class()->name->bytes,
            method->get_name()->bytes,
            method->get_descriptor()->bytes);
#endif


    assert(tmn_is_suspend_enabled());
    assert(interpreter_ti_notification_mode & INTERPRETER_TI_METHOD_ENTRY_EVENT);

    jvmti_process_method_entry_event((jmethodID) method);

    assert(tmn_is_suspend_enabled());
}
void method_exit_callback(Method *method, bool was_popped_by_exception, jvalue ret_val) {
    assert(tmn_is_suspend_enabled());
    assert(interpreter_ti_notification_mode & INTERPRETER_TI_METHOD_EXIT_EVENT);

    jvmti_process_method_exit_event((jmethodID) method,
            was_popped_by_exception, ret_val);
}

void
frame_pop_callback(FramePopListener *l, Method *method, jboolean was_popped_by_exception) {
    assert(!tmn_is_suspend_enabled());
    tmn_suspend_enable();

    while (l) {
        jvmtiEnv *env = (jvmtiEnv*) l->listener;
        jvmti_process_frame_pop_event(env, (jmethodID) method, was_popped_by_exception);
        FramePopListener *prev = l;
        l = l->next;
        STD_FREE((void*)prev);
    }
    tmn_suspend_disable();
}

jvmtiError
interpreter_ti_pop_frame(jvmtiEnv * UNREF env, VM_thread *thread) {
    assert(tmn_is_suspend_enabled());
    StackFrame *frame = getLastStackFrame(thread);
    if (frame->jvmti_pop_frame == POP_FRAME_AVAILABLE) {
        frame->jvmti_pop_frame = POP_FRAME_NOW;
        return JVMTI_ERROR_NONE;
    } else {
        return JVMTI_ERROR_OPAQUE_FRAME;
    }

}

jvmtiError
interpreter_ti_notify_frame_pop(jvmtiEnv* env,
                                VM_thread *thread,
                                int depth)
{
    assert(tmn_is_suspend_enabled());
    StackFrame *frame = getLastStackFrame(thread);

    // skip depth of frames
    while (depth) {
        if (frame == NULL) {
            return JVMTI_ERROR_NO_MORE_FRAMES;
        }
        frame = frame->prev;
        depth--;
    }

    Method *m = frame->method;
    if (m->is_native()) {
        return JVMTI_ERROR_OPAQUE_FRAME;
    }

    FramePopListener *l = frame->framePopListener;
    while (l) {
        // already set frame pop listener.
        if (l->listener == (void*)env) {
            return JVMTI_ERROR_NONE;
        }
        l = l->next;
    }

    // allocate new frame pop listener
    l = (FramePopListener*) STD_MALLOC(sizeof(FramePopListener));

    l->listener = (jvmtiEnv*) env;
    l->next = frame->framePopListener;
    frame->framePopListener = l;

    return JVMTI_ERROR_NONE;
}


void single_step_callback(StackFrame &frame) {
    uint8 ip0 = *frame.ip;
    if (ip0 == OPCODE_BREAKPOINT) return;
    tmn_suspend_enable();
    Method *method = frame.method;
    
    jvmti_process_single_step_event((jmethodID) method,
            frame.ip - (uint8*)method->get_byte_code_addr());

    tmn_suspend_disable();
}
